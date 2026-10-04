#pragma once
#include <QString>
#include <QMap>
#include <QJsonObject>

// Sostituisce config.json + electron-store dell'app Electron.
// Vive in: %APPDATA%/ETA-Games-Launcher/config.json (Windows)
class Config {
public:
    static Config& instance();

    QString dataDir() const;
    QString gamesBaseDir() const; // defaultInstallDir oppure dataDir()/games

    // Percorsi globali
    QString defaultInstallDir() const { return m_defaultInstallDir; }
    void setDefaultInstallDir(const QString& dir);

    QString gzdoomPath() const { return m_gzdoomPath; }
    void setGzdoomPath(const QString& p);

    QString doom2WadPath() const { return m_doom2WadPath; }
    void setDoom2WadPath(const QString& p);

    QString theme() const { return m_theme; }
    void setTheme(const QString& t);

    // Giochi installati: id -> cartella di installazione
    QMap<QString, QString> games() const { return m_games; }
    void setGameDir(const QString& id, const QString& dir);
    void removeGame(const QString& id);
    QString gameDir(const QString& id) const; // calcola default se non presente

    void load();
    void save() const;

private:
    Config() { load(); }
    QString m_defaultInstallDir;
    QString m_gzdoomPath;
    QString m_doom2WadPath;
    QString m_theme = "night-theme";
    QMap<QString, QString> m_games;
};
