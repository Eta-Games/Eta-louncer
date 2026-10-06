#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <functional>

class QNetworkAccessManager;

struct AuthUser {
    QString uid, email, displayName, photoUrl, idToken;
    QString refreshToken;      // presente col login email/password (serve a rinnovare l'idToken, che dura 1 ora)
    qint64 tokenTimeMs = 0;    // quando è stato ottenuto/rinnovato l'idToken
    bool isValid() const { return !uid.isEmpty(); }
};

// Sostituisce la parte "auth" di firebase-auth-compat.js nel renderer.
// Login email/password: chiamata diretta alle REST API di Identity Toolkit
// (stessa chiave web API già pubblica nel progetto Firebase dell'app originale).
// Login Google: delega al browser di sistema sulla stessa pagina hosted
// (eta-games.github.io/auth) già usata dall'app Electron, che poi reindirizza
// a etagames://auth?idToken=...&uid=...  — il deep link viene intercettato
// dal sistema operativo e inoltrato a questa istanza (vedi main.cpp).
class AuthManager : public QObject {
    Q_OBJECT
public:
    using DoneCb = std::function<void(bool ok, const QString& error)>;
    using JsonCb = std::function<void(bool ok, const QJsonObject& data, const QString& error)>;

    explicit AuthManager(QObject* parent = nullptr);

    void signInWithPassword(const QString& email, const QString& password);
    void startGoogleLogin(); // apre il browser di sistema

    // Chiamato da main.cpp quando arriva etagames://auth?...
    void handleDeepLink(const QString& url);

    AuthUser currentUser() const { return m_user; }
    void signOut();

    // ── Accesso automatico ──
    // "Ricordami": se attivo, dopo il login si salva il refresh token (mai la password) e al prossimo
    // avvio la sessione si ripristina da sola. restoreSession() ritorna false se non c'è nulla di salvato.
    bool rememberSession() const { return m_remember; }
    void setRememberSession(bool on);
    bool restoreSession();
    QString lastEmail() const;

    // Restituisce un idToken valido (lo rinnova se sta per scadere). Se non c'è utente → stringa vuota.
    void withFreshToken(std::function<void(const QString& idToken)> cb);

    // ── Operazioni sull'account (come profilo.html) ──
    void accountLookup(JsonCb cb);                         // creazione account, ultimo accesso, ecc.
    void updateDisplayName(const QString& name, DoneCb cb);
    void updateEmail(const QString& email, DoneCb cb);
    void updatePassword(const QString& password, DoneCb cb);
    void deleteAccount(DoneCb cb);

    static QString apiKey() { return QString::fromLatin1(FIREBASE_API_KEY); }

signals:
    void loginSucceeded(AuthUser user);
    void loginFailed(QString errorMessage);
    void userUpdated(AuthUser user); // nome/email cambiati
    void sessionRestoreFailed(QString reason);

private:
    QNetworkAccessManager* m_nam;
    AuthUser m_user;
    bool m_remember = true;
    void saveSession();
    static constexpr const char* FIREBASE_API_KEY = "AIzaSyCiWCjKAsNYLnDJT1o3r_E3tGLyYTasDPc";

    void postIdentity(const QString& endpoint, const QJsonObject& body, JsonCb cb);
    void accountsUpdate(const QJsonObject& extra, DoneCb cb);
    static QString friendlyError(const QString& code, const QString& fallback);
};
