#include "UpdateChecker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

// ---------------------------------------------------------------------------
// Aggiornamenti dei giochi (invariato)
// ---------------------------------------------------------------------------
void UpdateChecker::check(const QString& gameId, const QString& releasesApi, const QString& installedVersion) {
    if (releasesApi.isEmpty()) {
        emit result(gameId, UpdateInfo{});
        return;
    }
    QNetworkRequest req((QUrl(releasesApi)));
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        UpdateInfo info;
        if (reply->error() != QNetworkReply::NoError) {
            info.error = reply->errorString();
            emit result(gameId, info);
            reply->deleteLater();
            return;
        }
        QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QString latestTag = o.value("tag_name").toString();
        info.latestVersion = latestTag;
        info.releaseName = o.value("name").toString(latestTag);
        info.releaseNotes = o.value("body").toString();
        info.hasUpdate = !latestTag.isEmpty() && latestTag != installedVersion;
        emit result(gameId, info);
        reply->deleteLater();
    });
}

// ---------------------------------------------------------------------------
// Aggiornamento del launcher + "broadcast" = ultimo commit del branch master.
// Titolo broadcast  = prima riga del messaggio di commit
// Testo broadcast   = resto del messaggio di commit
// ID / versione     = SHA del commit
// L'ETag evita di consumare il rate limit (le risposte 304 non contano).
// ---------------------------------------------------------------------------
void UpdateChecker::checkLauncher(const QString& installedSha) {
    QNetworkRequest req(QUrl("https://api.github.com/repos/Eta-Games/Eta-louncer/commits/master"));
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", "application/vnd.github+json");
    if (!m_etag.isEmpty())
        req.setRawHeader("If-None-Match", m_etag);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        CommitInfo info;

        if (http == 304) {
            info = m_cached;                         // nessun cambiamento dall'ultima richiesta
        } else if (http == 200) {
            QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonObject c = o.value("commit").toObject();
            QString msg = c.value("message").toString();
            info.sha   = o.value("sha").toString();
            info.title = msg.section('\n', 0, 0).trimmed();
            info.body  = msg.section('\n', 1).trimmed();
            info.date  = c.value("committer").toObject().value("date").toString();
            info.url   = o.value("html_url").toString();
            m_etag = reply->rawHeader("ETag");
            m_cached = info;
        } else if (http == 403 || http == 429) {
            info.error = QStringLiteral("Limite richieste GitHub raggiunto, riprova piu' tardi");
        } else if (http == 404) {
            info.error = QStringLiteral("Branch master non trovato");
        } else {
            info.error = reply->errorString();
        }

        info.hasUpdate = info.error.isEmpty() && !info.sha.isEmpty() && info.sha != installedSha;
        emit launcherResult(info);
        reply->deleteLater();
    });
}
