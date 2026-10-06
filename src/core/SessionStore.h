#pragma once
#include <QString>

// Sessione salvata per l'accesso automatico. La PASSWORD NON viene mai salvata:
// si conserva solo il refresh token di Firebase (revocabile, serve a ottenere un nuovo idToken).
struct SavedSession {
    QString uid, email, displayName, refreshToken;
    bool isValid() const { return !uid.isEmpty() && !refreshToken.isEmpty(); }
};

// Windows: Gestore credenziali di Windows (Credential Manager), legato all'utente del PC.
// Altri sistemi: QSettings (non cifrato, usato solo per sviluppo/test).
class SessionStore {
public:
    static void save(const SavedSession& s);
    static SavedSession load();
    static void clear();
};
