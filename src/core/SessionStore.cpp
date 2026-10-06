#include "SessionStore.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
static wchar_t TARGET_NAME[] = L"ETA-Games-Launcher/session";
static wchar_t USER_NAME[]   = L"ETA Games";
#endif

static QByteArray toJson(const SavedSession& s) {
    QJsonObject o;
    o["uid"] = s.uid;
    o["email"] = s.email;
    o["displayName"] = s.displayName;
    o["refreshToken"] = s.refreshToken;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

static SavedSession fromJson(const QByteArray& data) {
    SavedSession s;
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    s.uid = o.value("uid").toString();
    s.email = o.value("email").toString();
    s.displayName = o.value("displayName").toString();
    s.refreshToken = o.value("refreshToken").toString();
    return s;
}

void SessionStore::save(const SavedSession& s) {
    if (!s.isValid()) return;
    const QByteArray blob = toJson(s);
#ifdef Q_OS_WIN
    if (blob.size() > 2500) return; // limite del Gestore credenziali (2560 byte)
    CREDENTIALW c = {};
    c.Type = CRED_TYPE_GENERIC;
    c.TargetName = TARGET_NAME;
    c.UserName = USER_NAME;
    c.CredentialBlobSize = static_cast<DWORD>(blob.size());
    c.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(blob.constData()));
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&c, 0);
#else
    QSettings().setValue("session/data", blob.toBase64());
#endif
}

SavedSession SessionStore::load() {
#ifdef Q_OS_WIN
    PCREDENTIALW pc = nullptr;
    if (!CredReadW(TARGET_NAME, CRED_TYPE_GENERIC, 0, &pc) || !pc) return SavedSession();
    const QByteArray blob(reinterpret_cast<const char*>(pc->CredentialBlob), static_cast<int>(pc->CredentialBlobSize));
    CredFree(pc);
    return fromJson(blob);
#else
    return fromJson(QByteArray::fromBase64(QSettings().value("session/data").toByteArray()));
#endif
}

void SessionStore::clear() {
#ifdef Q_OS_WIN
    CredDeleteW(TARGET_NAME, CRED_TYPE_GENERIC, 0);
#else
    QSettings().remove("session/data");
#endif
}
