#include "Config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

QString Config::dataDir() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

QString Config::gamesBaseDir() const {
    if (!m_defaultInstallDir.isEmpty()) return m_defaultInstallDir;
    return QDir(dataDir()).filePath("games");
}

void Config::setDefaultInstallDir(const QString& dir) { m_defaultInstallDir = dir; save(); }
void Config::setGzdoomPath(const QString& p) { m_gzdoomPath = p; save(); }
void Config::setDoom2WadPath(const QString& p) { m_doom2WadPath = p; save(); }
void Config::setTheme(const QString& t) { m_theme = t; save(); }

void Config::setGameDir(const QString& id, const QString& dir) {
    m_games[id] = dir;
    save();
}

void Config::removeGame(const QString& id) {
    m_games.remove(id);
    save();
}

QString Config::gameDir(const QString& id) const {
    if (m_games.contains(id)) return m_games.value(id);
    return QDir(gamesBaseDir()).filePath(id);
}

void Config::load() {
    QString path = QDir(dataDir()).filePath("config.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_defaultInstallDir = root.value("defaultInstallDir").toString();
    m_gzdoomPath        = root.value("gzdoomPath").toString();
    m_doom2WadPath      = root.value("doom2WadPath").toString();
    m_theme             = root.value("theme").toString("night-theme");
    m_games.clear();
    QJsonObject g = root.value("games").toObject();
    for (auto it = g.begin(); it != g.end(); ++it)
        m_games[it.key()] = it.value().toString();
}

void Config::save() const {
    QJsonObject root;
    root["defaultInstallDir"] = m_defaultInstallDir;
    root["gzdoomPath"]        = m_gzdoomPath;
    root["doom2WadPath"]      = m_doom2WadPath;
    root["theme"]             = m_theme;
    QJsonObject g;
    for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it)
        g[it.key()] = it.value();
    root["games"] = g;

    QDir().mkpath(dataDir());
    QFile f(QDir(dataDir()).filePath("config.json"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}
