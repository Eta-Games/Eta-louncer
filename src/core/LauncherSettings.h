#pragma once
#include <QSettings>
#include <QString>

// Impostazioni del launcher (non dell'account): salvate con QSettings, in modo indipendente da Config.
// Org/app name vengono impostati in main.cpp prima del primo accesso.
namespace LauncherSettings {

inline QSettings& store() { static QSettings s; return s; }

// Tema scelto dall'utente (vuoto = usa quello di Config)
inline QString theme()                  { return store().value("ui/theme").toString(); }
inline void setTheme(const QString& v)  { store().setValue("ui/theme", v); }

inline bool checkUpdatesOnStart()       { return store().value("startup/checkUpdates", true).toBool(); }
inline void setCheckUpdatesOnStart(bool v) { store().setValue("startup/checkUpdates", v); }

inline bool openOnInstalled()           { return store().value("startup/openOnInstalled", false).toBool(); }
inline void setOpenOnInstalled(bool v)  { store().setValue("startup/openOnInstalled", v); }

inline bool closeOnLaunch()             { return store().value("launch/closeLauncher", false).toBool(); }
inline void setCloseOnLaunch(bool v)    { store().setValue("launch/closeLauncher", v); }

// Cartella di installazione personalizzata per i NUOVI giochi (vuoto = predefinita di Config)
inline QString gamesDir()               { return store().value("paths/gamesDir").toString(); }
inline void setGamesDir(const QString& v) { store().setValue("paths/gamesDir", v); }

} // namespace LauncherSettings
