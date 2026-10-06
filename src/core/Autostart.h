#pragma once
#include <QString>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

// Avvio con Windows: voce in HKCU\...\Run (nessun permesso di amministratore).
// L'exe parte con --background: resta nascosto nella tray.
namespace Autostart {

#ifdef Q_OS_WIN
inline const char* runKey() { return "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"; }
inline bool isEnabled() {
    QSettings r(runKey(), QSettings::NativeFormat);
    return r.contains("ETAGamesLauncher");
}
inline void set(bool on) {
    QSettings r(runKey(), QSettings::NativeFormat);
    if (on) r.setValue("ETAGamesLauncher",
                       QString("\"%1\" --background").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else    r.remove("ETAGamesLauncher");
}
#else
inline bool isEnabled() { return false; }
inline void set(bool) {}
#endif

} // namespace Autostart
