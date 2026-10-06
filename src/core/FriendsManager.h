#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <functional>

class AuthManager;
class FirestoreClient;
class PresenceManager;

// Lista amici salvata in users/<uid>.friends = [{uid, name}, ...] (stesso documento del tema, già scrivibile
// dal proprietario). Lo stato (online / a cosa giocano) NON si salva: arriva da PresenceManager::online().
class FriendsManager : public QObject {
    Q_OBJECT
public:
    struct Friend { QString uid, name; };
    using Done = std::function<void(const QString& error)>;   // error vuoto = ok

    FriendsManager(AuthManager* auth, FirestoreClient* fs, PresenceManager* presence, QObject* parent = nullptr);

    void reload();   // dopo il login
    void clear();    // al logout
    QList<Friend> friends() const { return m_list; }

    void addByName(const QString& name, Done cb);   // cerca in presence/ per displayName esatto
    void add(const QString& uid, const QString& name, Done cb = nullptr);
    void remove(const QString& uid, Done cb = nullptr);

signals:
    void changed();

private:
    AuthManager* m_auth;
    FirestoreClient* m_fs;
    PresenceManager* m_presence;
    QList<Friend> m_list;

    void commit(const QList<Friend>& next, Done cb);
};
