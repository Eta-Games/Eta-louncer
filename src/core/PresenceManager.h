#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <functional>

class AuthManager;
class FirestoreClient;
class QTimer;

// Stato online: ogni utente scrive presence/<uid> (status, gameId, lastSeen) e ogni 60 s lo rinnova.
// "Online" = lastSeen negli ultimi 2.5 minuti. Quando parte un gioco il launcher ne controlla il PID:
// finché il processo vive lo stato è "playing", poi torna "online".
class PresenceManager : public QObject {
    Q_OBJECT
public:
    struct OnlineUser { QString uid, name, status, gameId; };

    PresenceManager(AuthManager* auth, FirestoreClient* fs, QObject* parent = nullptr);

    void start();                    // dopo il login
    void stop(bool waitForNetwork);  // logout / chiusura: segna offline
    void gameStarted(const QString& gameId, qint64 pid);
    void settingsChanged();          // l'utente ha attivato/disattivato "mostra il mio stato"

    QString ownStatus() const;       // "offline" | "online" | "playing"
    QString ownGameId() const { return m_gameId; }
    QList<OnlineUser> online() const { return m_online; }

signals:
    void onlineChanged();
    void ownStatusChanged();

private:
    AuthManager* m_auth;
    FirestoreClient* m_fs;
    QTimer* m_tick;
    QTimer* m_watch;
    bool m_active = false;
    QString m_gameId;
    qint64 m_pid = 0;
    QList<OnlineUser> m_online;

    void tick();
    void writeOwn(std::function<void()> done = nullptr);
    void refreshOthers();
    void watchGame();
};
