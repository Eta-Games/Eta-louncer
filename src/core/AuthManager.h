#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;

struct AuthUser {
    QString uid, email, displayName, photoUrl, idToken;
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
    explicit AuthManager(QObject* parent = nullptr);

    void signInWithPassword(const QString& email, const QString& password);
    void startGoogleLogin(); // apre il browser di sistema

    // Chiamato da main.cpp quando arriva etagames://auth?...
    void handleDeepLink(const QString& url);

    AuthUser currentUser() const { return m_user; }
    void signOut();

signals:
    void loginSucceeded(AuthUser user);
    void loginFailed(QString errorMessage);

private:
    QNetworkAccessManager* m_nam;
    AuthUser m_user;
    static constexpr const char* FIREBASE_API_KEY = "AIzaSyCiWCjKAsNYLnDJT1o3r_E3tGLyYTasDPc";
};
