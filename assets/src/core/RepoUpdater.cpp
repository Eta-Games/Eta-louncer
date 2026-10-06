#include "RepoUpdater.h"
#include "GameManager.h"
#include "GameCatalog.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#ifndef ETA_BUILD_COMMIT
#define ETA_BUILD_COMMIT ""
#endif

static const char* LAUNCHER_OWNER = "Eta-Games";
static const char* LAUNCHER_REPO  = "Eta-louncer";

static QSettings& store() { static QSettings s; return s; }

// "https://github.com/Eta-Games/Dante-s-Revenge.git" → {"Eta-Games", "Dante-s-Revenge"}
static bool parseRepo(const QString& url, QString* owner, QString* repo) {
    QString u = url;
    if (u.endsWith(".git")) u.chop(4);
    const QStringList parts = u.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;
    *repo = parts.last();
    *owner = parts.at(parts.size() - 2);
    return true;
}

static QNetworkRequest makeRequest(const QUrl& url, const QByteArray& accept) {
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", accept);
    return req;
}

RepoUpdater::RepoUpdater(GameManager* games, QObject* parent) : QObject(parent), m_games(games) {
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setInterval(5 * 60 * 1000);
    connect(m_timer, &QTimer::timeout, this, &RepoUpdater::refresh);
}

QString RepoUpdater::launcherPageUrl() {
    return QString("https://github.com/%1/%2/releases/latest").arg(LAUNCHER_OWNER, LAUNCHER_REPO);
}

void RepoUpdater::start() {
    m_timer->start();
    refresh();
}

void RepoUpdater::refresh() {
    for (const Target& t : targets()) checkTarget(t);
}

QList<RepoUpdater::Target> RepoUpdater::targets() const {
    QList<Target> list;

    // Launcher: si può controllare solo se sappiamo con quale commit è stato compilato
    const QString buildCommit = QString::fromLatin1(ETA_BUILD_COMMIT);
    if (!buildCommit.isEmpty())
        list.append({"launcher", "ETA Launcher", LAUNCHER_OWNER, LAUNCHER_REPO, QString(), buildCommit, true});

    // Giochi installati
    for (const auto& g : gameCatalog()) {
        if (!m_games->isInstalled(g.id)) continue;
        QString owner, repo;
        if (!parseRepo(g.repoUrl, &owner, &repo)) continue;
        const QString local = m_games->localCommit(g.id);
        if (local.isEmpty()) continue; // la cartella non è un clone git: niente da confrontare
        list.append({g.id, g.title, owner, repo, g.branch, local, false});
    }
    return list;
}

// ── Passo 1: commit più recente della repo (solo lo sha, con ETag) ────────
void RepoUpdater::checkTarget(const Target& t) {
    const QString ref = t.branch.isEmpty() ? QStringLiteral("HEAD") : t.branch;
    QUrl url(QString("https://api.github.com/repos/%1/%2/commits/%3").arg(t.owner, t.repo, ref));

    QNetworkRequest req = makeRequest(url, "application/vnd.github.sha");
    const QString etagKey = "repoupdate/etag/" + t.id;
    const QString shaKey  = "repoupdate/sha/" + t.id;
    const QString etag = store().value(etagKey).toString();
    if (!etag.isEmpty() && !store().value(shaKey).toString().isEmpty())
        req.setRawHeader("If-None-Match", etag.toUtf8()); // 304 = invariato, non pesa sul limite

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, t, reply, etagKey, shaKey]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
        const QString body = QString::fromUtf8(reply->readAll()).trimmed();
        reply->deleteLater();

        QString remoteSha;
        if (status == 200) {
            remoteSha = body;
            store().setValue(etagKey, newEtag);
            store().setValue(shaKey, remoteSha);
        } else if (status == 304) {
            remoteSha = store().value(shaKey).toString();
        } else {
            return; // errore di rete o limite richieste: riproviamo al prossimo giro
        }

        if (remoteSha.size() < 40 || remoteSha == t.localSha) return; // uguali (o risposta strana)
        compare(t, remoteSha);
    });
}

// ── Passo 2: la repo è davvero AVANTI? (evita falsi allarmi se hai commit tuoi non pubblicati) ──
void RepoUpdater::compare(const Target& t, const QString& remoteSha) {
    const QString key = t.id + ":" + t.localSha + ":" + remoteSha;
    if (m_done.contains(key) || m_inFlight.contains(key)) return;
    m_inFlight.insert(key);

    QUrl url(QString("https://api.github.com/repos/%1/%2/compare/%3...%4")
                 .arg(t.owner, t.repo, t.localSha, remoteSha));
    QNetworkReply* reply = m_nam->get(makeRequest(url, "application/vnd.github+json"));
    connect(reply, &QNetworkReply::finished, this, [this, t, remoteSha, key, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        m_inFlight.remove(key);

        if (status == 404 || status == 422) { m_done.insert(key); return; } // commit locale non presente sulla repo
        if (status != 200) return;                                          // riproviamo al prossimo giro
        m_done.insert(key);

        const QJsonObject o = QJsonDocument::fromJson(body).object();
        if (o.value("status").toString() != "ahead") return; // identica, indietro o divergente: niente richiesta

        RepoUpdate u;
        u.id = t.id;
        u.name = t.name;
        u.localSha = t.localSha;
        u.remoteSha = remoteSha;
        u.isLauncher = t.isLauncher;
        u.aheadBy = o.value("ahead_by").toInt();

        const QJsonArray commits = o.value("commits").toArray(); // dal più vecchio al più recente
        for (int i = commits.size() - 1; i >= 0 && u.recent.size() < 5; --i) {
            const QString msg = commits.at(i).toObject().value("commit").toObject().value("message").toString();
            u.recent << msg.section('\n', 0, 0).trimmed();
        }
        emit updateAvailable(u);
    });
}
