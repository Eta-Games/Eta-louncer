#include "AuthManager.h"
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>

static const QMap<QString, QString> ERR_MESSAGES = {
    {"EMAIL_NOT_FOUND",      "Account non trovato."},
    {"INVALID_PASSWORD",     "Password errata."},
    {"INVALID_EMAIL",        "Email non valida."},
    {"TOO_MANY_ATTEMPTS_TRY_LATER", "Troppi tentativi. Riprova più tardi."},
    {"INVALID_LOGIN_CREDENTIALS",   "Email o password errati."},
};

AuthManager::AuthManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void AuthManager::signInWithPassword(const QString& email, const QString& password) {
    QUrl url("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword");
    QUrlQuery q; q.addQueryItem("key", FIREBASE_API_KEY);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["email"] = email;
    body["password"] = password;
    body["returnSecureToken"] = true;

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError) {
            QString code = resp.value("error").toObject().value("message").toString();
            emit loginFailed(ERR_MESSAGES.value(code, code.isEmpty() ? reply->errorString() : code));
            reply->deleteLater();
            return;
        }
        m_user.uid = resp.value("localId").toString();
        m_user.email = resp.value("email").toString();
        m_user.displayName = resp.value("displayName").toString();
        m_user.idToken = resp.value("idToken").toString();
        emit loginSucceeded(m_user);
        reply->deleteLater();
    });
}

void AuthManager::startGoogleLogin() {
    // Stessa pagina hosted dell'app originale: esegue il login Google con
    // Firebase lato browser (dove signInWithPopup funziona davvero, a
    // differenza di un contesto file:///nativo) e poi reindirizza a
    // etagames://auth?idToken=...&uid=...
    QDesktopServices::openUrl(QUrl("https://eta-games.github.io/auth/"));
}

void AuthManager::handleDeepLink(const QString& url) {
    QUrl u(url);
    if (u.host() != "auth") return;
    QUrlQuery q(u);
    QString idToken = q.queryItemValue("idToken", QUrl::FullyDecoded);
    QString uid     = q.queryItemValue("uid", QUrl::FullyDecoded);
    if (idToken.isEmpty() || uid.isEmpty()) {
        emit loginFailed("Accesso Google annullato o non riuscito.");
        return;
    }
    m_user.uid = uid;
    m_user.idToken = idToken;
    m_user.email = q.queryItemValue("email", QUrl::FullyDecoded);
    m_user.displayName = q.queryItemValue("displayName", QUrl::FullyDecoded);
    m_user.photoUrl = q.queryItemValue("photoURL", QUrl::FullyDecoded);
    emit loginSucceeded(m_user);
}

void AuthManager::signOut() {
    m_user = AuthUser{};
}
