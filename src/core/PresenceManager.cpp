#include "PresenceManager.h"
#include "AuthManager.h"
#include "FirestoreClient.h"
#include "LauncherSettings.h"
#include "ProcessUtil.h"
#include <QTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonArray>

static const int HEARTBEAT_MS = 60 * 1000;
static const int WATCH_MS     = 8 * 1000;
static const int ONLINE_WINDOW_S = 150;

PresenceManager::PresenceManager(AuthManager* auth, FirestoreClient* fs, QObject* parent)
    : QObject(parent), m_auth(auth), m_fs(fs) {
    m_tick = new QTimer(this);
    m_tick->setInterval(HEARTBEAT_MS);
    connect(m_tick, &QTimer::timeout, this, &PresenceManager::tick);
    m_watch = new QTimer(this);
    m_watch->setInterval(WATCH_MS);
    connect(m_watch, &QTimer::timeout, this, &PresenceManager::watchGame);
}

QString PresenceManager::ownStatus() const {
    if (!m_active || !LauncherSettings::showOnlineStatus()) return "offline";
    return m_gameId.isEmpty() ? "online" : "playing";
}

void PresenceManager::start() {
    m_active = true;
    m_gameId.clear();
    m_pid = 0;
    m_tick->start();
    tick();
    emit ownStatusChanged();
}

void PresenceManager::stop(bool waitForNetwork) {
    if (!m_active) return;
    m_active = false;
    m_tick->stop();
    m_watch->stop();
    m_gameId.clear();
    m_pid = 0;
    m_online.clear();

    if (!m_auth->currentUser().isValid()) { emit ownStatusChanged(); return; }
    if (waitForNetwork) {
        // best effort alla chiusura dell'app: aspettiamo la risposta al massimo 1.5 s
        QEventLoop loop;
        QTimer::singleShot(1500, &loop, &QEventLoop::quit);
        writeOwn([&loop]() { loop.quit(); });
        loop.exec();
    } else {
        writeOwn();
    }
    emit ownStatusChanged();
    emit onlineChanged();
}

void PresenceManager::gameStarted(const QString& gameId, qint64 pid) {
    if (!m_active) return;
    m_gameId = gameId;
    m_pid = pid;
    if (pid > 0) m_watch->start();
    writeOwn();
    emit ownStatusChanged();
}

void PresenceManager::settingsChanged() {
    if (!m_active) return;
    writeOwn();
    emit ownStatusChanged();
}

void PresenceManager::watchGame() {
    if (m_gameId.isEmpty()) { m_watch->stop(); return; }
    if (isProcessRunning(m_pid)) return;
    m_gameId.clear();
    m_pid = 0;
    m_watch->stop();
    writeOwn();
    emit ownStatusChanged();
}

void PresenceManager::tick() {
    if (!m_auth->currentUser().isValid()) return;
    writeOwn();
    refreshOthers();
}

void PresenceManager::writeOwn(std::function<void()> done) {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) { if (done) done(); return; }
    QJsonObject f;
    f["uid"] = u.uid;
    f["displayName"] = u.displayName.isEmpty() ? QString("Giocatore") : u.displayName;
    f["status"] = ownStatus();
    f["gameId"] = m_gameId;
    f["lastSeen"] = FirestoreClient::timestampNow();
    m_fs->setDocument("presence/" + u.uid, f, [done](bool, const QJsonObject&, const QString&) { if (done) done(); });
}

void PresenceManager::refreshOthers() {
    QJsonObject field;  field["fieldPath"] = "lastSeen";
    QJsonObject value;  value["timestampValue"] = FirestoreClient::timestampNow(-ONLINE_WINDOW_S).value("$ts");
    QJsonObject filter; filter["field"] = field; filter["op"] = "GREATER_THAN_OR_EQUAL"; filter["value"] = value;
    QJsonObject where;  where["fieldFilter"] = filter;
    QJsonObject coll;   coll["collectionId"] = "presence";
    QJsonObject q;      q["from"] = QJsonArray{coll}; q["where"] = where;

    m_fs->runQuery(q, [this](bool ok, const QJsonArray& docs, const QString&) {
        if (!ok || !m_active) return;
        QList<OnlineUser> list;
        for (const auto& d : docs) {
            const QJsonObject o = d.toObject();
            const QString status = o.value("status").toString();
            if (status != "online" && status != "playing") continue;
            list.append({o.value("_id").toString(), o.value("displayName").toString(), status, o.value("gameId").toString()});
        }
        m_online = list;
        emit onlineChanged();
    });
}
