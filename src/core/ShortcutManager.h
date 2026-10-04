#pragma once
#include <QString>

// Crea un collegamento .lnk sul Desktop che rilancia ETALauncher.exe
// passando l'id del gioco (es: ETALauncher.exe --launch dantes_revenge),
// cosi' l'utente può avviare il gioco senza riaprire il launcher completo.
namespace ShortcutManager {
    bool createDesktopShortcut(const QString& gameId, const QString& title);
}
