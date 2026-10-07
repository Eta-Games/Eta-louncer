#include "AuthManager.h"
#include "I18n.h"
#include "SessionStore.h"
#include <QSettings>
#include <QMap>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
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
    {"EMAIL_EXISTS",         "Questa email è già usata da un altro account."},
    {"WEAK_PASSWORD",        "Password troppo debole (minimo 6 caratteri)."},
    {"CREDENTIAL_TOO_OLD_LOGIN_REQUIRED", "Per questa operazione devi uscire e rientrare nell'account."},
    {"TOKEN_EXPIRED",        "Sessione scaduta: esci e rientra nell'account."},
    {"INVALID_ID_TOKEN",     "Sessione scaduta: esci e rientra nell'account."},
    {"OPERATION_NOT_ALLOWED","Operazione non consentita per questo account."},
    {"USER_NOT_FOUND",       "Account non trovato."},
};

static const qint64 TOKEN_MAX_AGE_MS = 50ll * 60 * 1000; // l'idToken Firebase dura 60 minuti

AuthManager::AuthManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_remember = QSettings().value("auth/remember", true).toBool();
}

void AuthManager::saveSession() {
    if (!m_remember || !m_user.isValid() || m_user.refreshToken.isEmpty()) return;
    SessionStore::save({m_user.uid, m_user.email, m_user.displayName, m_user.refreshToken});
    if (!m_user.email.isEmpty()) QSettings().setValue("auth/lastEmail", m_user.email);
}

void AuthManager::setRememberSession(bool on) {
    m_remember = on;
    QSettings().setValue("auth/remember", on);
    if (!on) SessionStore::clear();
}

QString AuthManager::lastEmail() const {
    return m_remember ? QSettings().value("auth/lastEmail").toString() : QString();
}

// Ripristina la sessione salvata: scambia il refresh token con un idToken nuovo.
bool AuthManager::restoreSession() {
    if (!m_remember) return false;
    const SavedSession s = SessionStore::load();
    if (!s.isValid()) return false;

    QUrl url("https://securetoken.googleapis.com/v1/token");
    QUrlQuery q; q.addQueryItem("key", FIREBASE_API_KEY);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setTransferTimeout(10000);
    QUrlQuery form;
    form.addQueryItem("grant_type", "refresh_token");
    form.addQueryItem("refresh_token", s.refreshToken);

    QNetworkReply* reply = m_nam->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, s]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        const QString idToken = resp.value("id_token").toString();
        if (reply->error() == QNetworkReply::NoError && !idToken.isEmpty()) {
            m_user = AuthUser{};
            m_user.uid = s.uid;
            m_user.email = s.email;
            m_user.displayName = s.displayName;
            m_user.idToken = idToken;
            const QString rt = resp.value("refresh_token").toString();
            m_user.refreshToken = rt.isEmpty() ? s.refreshToken : rt;
            m_user.tokenTimeMs = QDateTime::currentMSecsSinceEpoch();
            saveSession(); // il refresh token può cambiare: teniamo quello nuovo
            emit loginSucceeded(m_user);
            return;
        }
        // 4xx = token revocato/scaduto (o utente cancellato): si cancella. Altrimenti (offline) lo teniamo.
        const bool rejected = status >= 400 && status < 500;
        if (rejected) SessionStore::clear();
        emit sessionRestoreFailed(rejected
            ? T("La sessione salvata è scaduta: accedi di nuovo.")
            : T("Non riesco a ripristinare la sessione (sei offline?). Accedi manualmente."));
    });
    return true;
}

QString AuthManager::friendlyError(const QString& code, const QString& fallback) {
    // Firebase a volte risponde "WEAK_PASSWORD : Password should be at least 6 characters"
    const QString key = code.section(" : ", 0, 0).trimmed();
    if (ERR_MESSAGES.contains(key)) return I18n::tr(ERR_MESSAGES.value(key));
    return code.isEmpty() ? fallback : code;
}

// POST generico verso Identity Toolkit (accounts:update, accounts:lookup, ...)
void AuthManager::postIdentity(const QString& endpoint, const QJsonObject& body, JsonCb cb) {
    QUrl url("https://identitytoolkit.googleapis.com/v1/" + endpoint);
    QUrlQuery q; q.addQueryItem("key", FIREBASE_API_KEY);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb]() {
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError) {
            QString code = resp.value("error").toObject().value("message").toString();
            cb(false, resp, friendlyError(code, reply->errorString()));
        } else {
            cb(true, resp, QString());
        }
        reply->deleteLater();
    });
}

void AuthManager::signInWithPassword(const QString& email, const QString& password) {
    QJsonObject body;
    body["email"] = email;
    body["password"] = password;
    body["returnSecureToken"] = true;

    postIdentity("accounts:signInWithPassword", body, [this](bool ok, const QJsonObject& resp, const QString& err) {
        if (!ok) { emit loginFailed(err); return; }
        m_user = AuthUser{};
        m_user.uid = resp.value("localId").toString();
        m_user.email = resp.value("email").toString();
        m_user.displayName = resp.value("displayName").toString();
        m_user.photoUrl = resp.value("profilePicture").toString();
        m_user.idToken = resp.value("idToken").toString();
        m_user.refreshToken = resp.value("refreshToken").toString();
        m_user.tokenTimeMs = QDateTime::currentMSecsSinceEpoch();
        saveSession();
        emit loginSucceeded(m_user);
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
        emit loginFailed(T("Accesso Google annullato o non riuscito."));
        return;
    }
    m_user = AuthUser{};
    m_user.uid = uid;
    m_user.idToken = idToken;
    // Opzionale: se la pagina /auth aggiunge &refreshToken=... la sessione si rinnova da sola
    m_user.refreshToken = q.queryItemValue("refreshToken", QUrl::FullyDecoded);
    m_user.tokenTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_user.email = q.queryItemValue("email", QUrl::FullyDecoded);
    m_user.displayName = q.queryItemValue("displayName", QUrl::FullyDecoded);
    m_user.photoUrl = q.queryItemValue("photoURL", QUrl::FullyDecoded);
    saveSession(); // solo se la pagina /auth ha passato anche un refreshToken
    emit loginSucceeded(m_user);
}

void AuthManager::signOut() {
    m_user = AuthUser{};
    SessionStore::clear(); // uscire dall'account = niente accesso automatico
}

void AuthManager::withFreshToken(std::function<void(const QString&)> cb) {
    if (!m_user.isValid()) { cb(QString()); return; }
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_user.tokenTimeMs;
    if (m_user.refreshToken.isEmpty() || age < TOKEN_MAX_AGE_MS) { cb(m_user.idToken); return; }

    QUrl url("https://securetoken.googleapis.com/v1/token");
    QUrlQuery q; q.addQueryItem("key", FIREBASE_API_KEY);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem("grant_type", "refresh_token");
    form.addQueryItem("refresh_token", m_user.refreshToken);

    const QString uid = m_user.uid;
    QNetworkReply* reply = m_nam->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, uid]() {
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() == QNetworkReply::NoError && m_user.uid == uid) {
            const QString id = resp.value("id_token").toString();
            if (!id.isEmpty()) {
                m_user.idToken = id;
                m_user.tokenTimeMs = QDateTime::currentMSecsSinceEpoch();
                const QString rt = resp.value("refresh_token").toString();
                if (!rt.isEmpty()) { m_user.refreshToken = rt; saveSession(); }
            }
        }
        cb(m_user.idToken); // se il rinnovo fallisce proviamo comunque col token attuale
        reply->deleteLater();
    });
}

void AuthManager::accountLookup(JsonCb cb) {
    withFreshToken([this, cb](const QString& token) {
        if (token.isEmpty()) { cb(false, QJsonObject(), T("Non hai effettuato l'accesso.")); return; }
        QJsonObject body; body["idToken"] = token;
        postIdentity("accounts:lookup", body, [cb](bool ok, const QJsonObject& resp, const QString& err) {
            if (!ok) { cb(false, QJsonObject(), err); return; }
            cb(true, resp.value("users").toArray().first().toObject(), QString());
        });
    });
}

// accounts:update con idToken fresco; aggiorna i dati locali con la risposta (nuovo token incluso)
void AuthManager::accountsUpdate(const QJsonObject& extra, DoneCb cb) {
    withFreshToken([this, extra, cb](const QString& token) {
        if (token.isEmpty()) { cb(false, T("Non hai effettuato l'accesso.")); return; }
        QJsonObject body = extra;
        body["idToken"] = token;
        body["returnSecureToken"] = true;
        postIdentity("accounts:update", body, [this, cb](bool ok, const QJsonObject& resp, const QString& err) {
            if (!ok) { cb(false, err); return; }
            if (resp.contains("idToken")) {
                m_user.idToken = resp.value("idToken").toString();
                m_user.tokenTimeMs = QDateTime::currentMSecsSinceEpoch();
            }
            if (resp.contains("refreshToken")) m_user.refreshToken = resp.value("refreshToken").toString();
            if (resp.contains("email")) m_user.email = resp.value("email").toString();
            if (resp.contains("displayName")) m_user.displayName = resp.value("displayName").toString();
            saveSession();
            emit userUpdated(m_user);
            cb(true, QString());
        });
    });
}

void AuthManager::updateDisplayName(const QString& name, DoneCb cb) {
    QJsonObject o; o["displayName"] = name;
    accountsUpdate(o, cb);
}

void AuthManager::updateEmail(const QString& email, DoneCb cb) {
    QJsonObject o; o["email"] = email;
    accountsUpdate(o, cb);
}

void AuthManager::updatePassword(const QString& password, DoneCb cb) {
    QJsonObject o; o["password"] = password;
    accountsUpdate(o, cb);
}

void AuthManager::deleteAccount(DoneCb cb) {
    withFreshToken([this, cb](const QString& token) {
        if (token.isEmpty()) { cb(false, T("Non hai effettuato l'accesso.")); return; }
        QJsonObject body; body["idToken"] = token;
        postIdentity("accounts:delete", body, [cb](bool ok, const QJsonObject&, const QString& err) { cb(ok, err); });
    });
}
