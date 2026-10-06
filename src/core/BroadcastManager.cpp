#include "BroadcastManager.h"
#include "GameCatalog.h"
#include "I18n.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

const QString BroadcastManager::FILE_NAME = "broadcast.json";

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

// Testo che può essere una stringa oppure {"it": "...", "en": "...", "de": "..."}: sceglie la lingua del launcher
static QString locText(const QJsonValue& v) {
    if (v.isString()) return v.toString();
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QStringList order = {I18n::code(I18n::current()), QStringLiteral("it"), QStringLiteral("en")};
        for (const QString& k : order) {
            const QString s = o.value(k).toString();
            if (!s.isEmpty()) return s;
        }
        for (auto it = o.begin(); it != o.end(); ++it)
            if (it.value().isString() && !it.value().toString().isEmpty()) return it.value().toString();
    }
    return QString();
}

// Elenco di note: array di testi, un testo con una nota per riga, oppure un oggetto per lingua
static QStringList locList(const QJsonValue& v) {
    QStringList out;
    if (v.isArray()) {
        for (const auto& e : v.toArray()) {
            const QString s = locText(e).trimmed();
            if (!s.isEmpty()) out << s;
        }
    } else if (v.isString()) {
        for (const QString& line : v.toString().split('\n', Qt::SkipEmptyParts)) {
            const QString s = line.trimmed();
            if (!s.isEmpty()) out << s;
        }
    } else if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QStringList order = {I18n::code(I18n::current()), QStringLiteral("it"), QStringLiteral("en")};
        for (const QString& k : order)
            if (o.contains(k)) return locList(o.value(k));
    }
    return out;
}

BroadcastManager::BroadcastManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setInterval(5 * 60 * 1000);
    connect(m_timer, &QTimer::timeout, this, &BroadcastManager::refresh);
    buildSources();
}

void BroadcastManager::buildSources() {
    m_sources.clear();
    // Repo del launcher
    m_sources.append({"launcher", "ETA Launcher", "Eta-Games", "Eta-louncer"});
    // Repo dei giochi (dal catalogo)
    for (const auto& g : gameCatalog()) {
        QString owner, repo;
        if (parseRepo(g.repoUrl, &owner, &repo)) m_sources.append({g.id, g.title, owner, repo});
    }
}

void BroadcastManager::start() {
    loadCache();
    m_timer->start();
    refresh();
    emit changed();
}

void BroadcastManager::refresh() {
    for (const auto& s : m_sources) checkSource(s);
}

void BroadcastManager::refreshSource(const QString& sourceId) {
    for (const auto& s : m_sources)
        if (s.id == sourceId) { checkSource(s); return; }
}

bool BroadcastManager::latestUpdate(const QString& sourceId, BroadcastMessage* out) const {
    const QList<BroadcastMessage> list = m_bySource.value(sourceId);
    int best = -1;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).level != "update") continue;
        if (best < 0 || list.at(i).date > list.at(best).date) best = i;
    }
    if (best < 0) return false;
    if (out) *out = list.at(best);
    return true;
}

QStringList BroadcastManager::readKeys() const { return store().value("broadcast/read").toStringList(); }

// ── Passo 1: ultimo commit che ha toccato broadcast.json ─────────────────
void BroadcastManager::checkSource(const Source& s) {
    QUrl url(QString("https://api.github.com/repos/%1/%2/commits").arg(s.owner, s.repo));
    QUrlQuery q;
    q.addQueryItem("path", FILE_NAME);
    q.addQueryItem("per_page", "1");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", "application/vnd.github+json");
    const QString etag = store().value("broadcast/etag/" + s.id).toString();
    if (!etag.isEmpty()) req.setRawHeader("If-None-Match", etag.toUtf8()); // 304 = nessun nuovo commit, costo zero

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, s, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (status != 200) return; // 304 (invariato) o errore/limite: teniamo la cache

        const QJsonArray commits = QJsonDocument::fromJson(body).array();
        if (commits.isEmpty()) { // nessun commit sul file: nessun broadcast per questa sorgente
            if (m_bySource.contains(s.id)) { m_bySource.remove(s.id); emit changed(); }
            store().setValue("broadcast/etag/" + s.id, newEtag);
            store().remove("broadcast/cache/" + s.id);
            store().remove("broadcast/sha/" + s.id);
            return;
        }
        const QString sha = commits.first().toObject().value("sha").toString();
        if (sha == store().value("broadcast/sha/" + s.id).toString() && m_bySource.contains(s.id)) {
            store().setValue("broadcast/etag/" + s.id, newEtag);
            return;
        }
        fetchFile(s, sha, newEtag);
    });
}

// ── Passo 2: contenuto del file (solo se c'è un commit nuovo) ─────────────
void BroadcastManager::fetchFile(const Source& s, const QString& commitSha, const QString& etag) {
    QUrl url(QString("https://api.github.com/repos/%1/%2/contents/%3").arg(s.owner, s.repo, FILE_NAME));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, s, commitSha, etag, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (status != 200) return;

        const QJsonObject o = QJsonDocument::fromJson(body).object();
        QByteArray content = o.value("content").toString().remove('\n').toUtf8();
        const QByteArray json = QByteArray::fromBase64(content);
        if (json.isEmpty()) return;

        // salviamo etag/sha SOLO ora che il file è stato scaricato davvero
        store().setValue("broadcast/etag/" + s.id, etag);
        store().setValue("broadcast/sha/" + s.id, commitSha);
        store().setValue("broadcast/cache/" + s.id, json);
        applyJson(s, json);
    });
}

void BroadcastManager::applyJson(const Source& s, const QByteArray& json) {
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QStringList read = readKeys();
    const QDate today = QDate::currentDate();

    QList<BroadcastMessage> list;
    for (const auto& v : root.value("messages").toArray()) {
        const QJsonObject o = v.toObject();
        BroadcastMessage m;
        m.id = o.value("id").toString();
        if (m.id.isEmpty()) continue;
        m.sourceId = s.id;
        m.sourceName = s.name;
        m.key = s.id + ":" + m.id;
        m.title = locText(o.value("title"));
        m.message = locText(o.value("message"));
        m.version = o.value("version").toString();
        m.notes = locList(o.value("notes"));
        m.level = o.value("level").toString("info");
        m.url = o.value("url").toString();
        m.date = QDateTime::fromString(o.value("date").toString(), Qt::ISODate);
        if (!m.date.isValid()) m.date = QDateTime::fromString(o.value("date").toString(), "yyyy-MM-dd");
        const QDate expires = QDate::fromString(o.value("expires").toString(), Qt::ISODate);
        if (expires.isValid() && expires < today) continue; // scaduto
        m.read = read.contains(m.key);
        list.append(m);
    }

    // messaggi davvero nuovi (non presenti prima e non già letti) → per eventuale avviso
    QList<BroadcastMessage> fresh;
    const auto previous = m_bySource.value(s.id);
    for (const auto& m : list) {
        const bool known = std::any_of(previous.begin(), previous.end(), [&](const BroadcastMessage& p) { return p.key == m.key; });
        if (!known && !m.read) fresh.append(m);
    }

    m_bySource[s.id] = list;
    emit changed();
    if (m_initialLoadDone && !fresh.isEmpty()) emit newMessages(fresh);
    m_initialLoadDone = true;
}

void BroadcastManager::loadCache() {
    for (const auto& s : m_sources) {
        const QByteArray json = store().value("broadcast/cache/" + s.id).toByteArray();
        if (!json.isEmpty()) applyJson(s, json);
    }
    m_initialLoadDone = false; // la cache iniziale non genera avvisi
}

QList<BroadcastMessage> BroadcastManager::messages() const {
    QList<BroadcastMessage> all;
    for (auto it = m_bySource.constBegin(); it != m_bySource.constEnd(); ++it) all += it.value();
    std::sort(all.begin(), all.end(), [](const BroadcastMessage& a, const BroadcastMessage& b) { return a.date > b.date; });
    return all;
}

int BroadcastManager::unreadCount() const {
    int n = 0;
    for (auto it = m_bySource.constBegin(); it != m_bySource.constEnd(); ++it)
        for (const auto& m : it.value()) if (!m.read) ++n;
    return n;
}

int BroadcastManager::unreadCount(const QString& sourceId) const {
    int n = 0;
    for (const auto& m : m_bySource.value(sourceId)) if (!m.read) ++n;
    return n;
}

void BroadcastManager::markRead(const QString& sourceId) {
    if (sourceId.isEmpty()) { markAllRead(); return; }
    QStringList read = readKeys();
    auto it = m_bySource.find(sourceId);
    if (it == m_bySource.end()) return;
    for (auto& m : it.value()) {
        if (!read.contains(m.key)) read << m.key;
        m.read = true;
    }
    store().setValue("broadcast/read", read);
    emit changed();
}

void BroadcastManager::markAllRead() {
    QStringList read = readKeys();
    for (auto it = m_bySource.begin(); it != m_bySource.end(); ++it)
        for (auto& m : it.value()) {
            if (!read.contains(m.key)) read << m.key;
            m.read = true;
        }
    store().setValue("broadcast/read", read);
    emit changed();
}
