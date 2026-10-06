#include "FriendsManager.h"
#include "AuthManager.h"
#include "FirestoreClient.h"
#include "PresenceManager.h"
#include "I18n.h"
#include <QJsonObject>
#include <QJsonArray>

FriendsManager::FriendsManager(AuthManager* auth, FirestoreClient* fs, PresenceManager* presence, QObject* parent)
    : QObject(parent), m_auth(auth), m_fs(fs), m_presence(presence) {}

void FriendsManager::reload() {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    const QString uid = u.uid;
    m_fs->getDocument("users/" + uid, [this, uid](bool ok, const QJsonObject& f, const QString&) {
        if (!ok || m_auth->currentUser().uid != uid) return;   // risposta arrivata dopo un logout/cambio account
        QList<Friend> list;
        for (const auto& e : f.value("friends").toArray()) {
            const QJsonObject o = e.toObject();
            const QString fuid = o.value("uid").toString();
            if (!fuid.isEmpty()) list.append({fuid, o.value("name").toString()});
        }
        m_list = list;
        emit changed();
    });
}

void FriendsManager::clear() {
    m_list.clear();
    emit changed();
}

void FriendsManager::commit(const QList<Friend>& next, Done cb) {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) { if (cb) cb(T("Devi accedere per gestire gli amici.")); return; }
    QJsonArray arr;
    for (const auto& fr : next) {
        QJsonObject o;
        o["uid"] = fr.uid;
        o["name"] = fr.name;
        arr.append(o);
    }
    QJsonObject fields;
    fields["friends"] = arr;
    m_fs->mergeFields("users/" + u.uid, fields, [this, next, cb](bool ok, const QJsonObject&, const QString& err) {
        if (ok) { m_list = next; emit changed(); }
        if (cb) cb(ok ? QString() : err);
    });
}

void FriendsManager::add(const QString& uid, const QString& name, Done cb) {
    if (uid == m_auth->currentUser().uid) { if (cb) cb(T("Non puoi aggiungere te stesso.")); return; }
    for (const auto& f : m_list)
        if (f.uid == uid) { if (cb) cb(T("È già tra i tuoi amici.")); return; }
    QList<Friend> next = m_list;
    next.append({uid, name});
    commit(next, cb);
}

void FriendsManager::remove(const QString& uid, Done cb) {
    QList<Friend> next;
    for (const auto& f : m_list) if (f.uid != uid) next.append(f);
    commit(next, cb);
}

void FriendsManager::addByName(const QString& rawName, Done cb) {
    const QString name = rawName.trimmed();
    if (name.isEmpty()) { if (cb) cb(T("Scrivi il nome utente dell'amico.")); return; }

    QJsonObject field;  field["fieldPath"] = "displayName";
    QJsonObject value;  value["stringValue"] = name;
    QJsonObject filter; filter["field"] = field; filter["op"] = "EQUAL"; filter["value"] = value;
    QJsonObject where;  where["fieldFilter"] = filter;
    QJsonObject coll;   coll["collectionId"] = "presence";
    QJsonObject q;      q["from"] = QJsonArray{coll}; q["where"] = where; q["limit"] = 3;

    m_fs->runQuery(q, [this, cb](bool ok, const QJsonArray& docs, const QString& err) {
        if (!ok) { if (cb) cb(err); return; }
        const QString me = m_auth->currentUser().uid;
        for (const auto& d : docs) {
            const QJsonObject o = d.toObject();
            const QString uid = o.value("_id").toString();
            if (uid.isEmpty() || uid == me) continue;
            add(uid, o.value("displayName").toString(), cb);
            return;
        }
        if (cb) cb(T("Nessun giocatore trovato con questo nome (maiuscole comprese)."));
    });
}
