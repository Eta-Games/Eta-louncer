#include "ShortcutManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QTextStream>
#include <QFileInfo>
#include <QFile>

bool ShortcutManager::createDesktopShortcut(const QString& gameId, const QString& title) {
#ifdef Q_OS_WIN
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString lnkPath = QDir(desktop).filePath(title + ".lnk");
    QString exePath = QCoreApplication::applicationFilePath();
    exePath.replace('/', '\\');
    QString lnkPathWin = lnkPath; lnkPathWin.replace('/', '\\');

    QString ps = QString(
        "$W = New-Object -ComObject WScript.Shell;"
        "$S = $W.CreateShortcut('%1');"
        "$S.TargetPath = '%2';"
        "$S.Arguments = '--launch %3';"
        "$S.WorkingDirectory = '%4';"
        "$S.IconLocation = '%2';"
        "$S.Save();"
    ).arg(lnkPathWin, exePath, gameId, QFileInfo(exePath).absolutePath().replace('/', '\\'));

    QTemporaryFile tmp(QDir::temp().filePath("eta-shortcut-XXXXXX.ps1"));
    tmp.setAutoRemove(false);
    if (!tmp.open()) return false;
    QTextStream(&tmp) << ps;
    QString scriptPath = tmp.fileName();
    tmp.close();

    QProcess p;
    p.start("powershell.exe",
            {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", scriptPath});
    p.waitForFinished(10000);
    QFile::remove(scriptPath);
    return p.exitCode() == 0;
#else
    Q_UNUSED(gameId); Q_UNUSED(title);
    return false; // scorciatoie desktop supportate solo su Windows, come l'originale
#endif
}
