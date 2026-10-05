#include "GameManager.h"
#include "Config.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>

static const QStringList WAD_EXTS = {".wad", ".pk3", ".pk7", ".ipk3"};
static const QStringList WAD_SKIP = {"doom2.wad", "doom.wad", "heretic.wad", "hexen.wad", "strife1.wad", "gzdoom.pk3",
                                 // file di supporto di GZDoom: li carica da solo l'exe, non vanno passati con -file
                                 "game_support.pk3", "game_widescreen_gfx.pk3", "brightmaps.pk3", "lights.pk3"};

GameManager::GameManager(QObject* parent) : QObject(parent) {}

bool GameManager::isInstalled(const QString& id) const {
    QString dir = Config::instance().gameDir(id);
    return QFile::exists(QDir(dir).filePath("meta.json"));
}

GameMeta GameManager::loadMeta(const QString& id) const {
    GameMeta meta;
    meta.gameDir = Config::instance().gameDir(id);
    QFile f(QDir(meta.gameDir).filePath("meta.json"));
    if (!f.open(QIODevice::ReadOnly)) return meta;
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    meta.title    = o.value("title").toString();
    meta.version  = o.value("version").toString();
    meta.desc     = o.value("desc").toString();
    meta.engine   = o.value("engine").toString();
    meta.size     = o.value("size").toString();
    meta.page     = o.value("page").toString();
    meta.doom2WadPath = o.value("doom2WadPath").toString();
    for (const auto& v : o.value("wadFile").toArray()) meta.wadFiles << v.toString();
    meta.installed = true;
    return meta;
}

void GameManager::writeMeta(const QString& id, const GameMeta& meta) const {
    QJsonObject o;
    o["title"] = meta.title;
    o["version"] = meta.version;
    o["desc"] = meta.desc;
    o["engine"] = meta.engine;
    o["size"] = meta.size;
    o["page"] = meta.page;
    o["gameDir"] = meta.gameDir;
    if (!meta.doom2WadPath.isEmpty()) o["doom2WadPath"] = meta.doom2WadPath;
    QJsonArray wads;
    for (const auto& w : meta.wadFiles) wads.append(w);
    o["wadFile"] = wads;

    QFile f(QDir(meta.gameDir).filePath("meta.json"));
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void GameManager::installGame(const GameEntry& game, const QString& customInstallDir) {
    QString baseDir = !customInstallDir.isEmpty() ? customInstallDir : Config::instance().gamesBaseDir();
    QString gameDir = QDir(baseDir).filePath(game.id);

    m_installingId = game.id;
    m_installingGame = game;
    m_installingDir = gameDir;

    m_cloner = new GitCloner(this);
    connect(m_cloner, &GitCloner::phaseProgress, this, &GameManager::installPhase);
    connect(m_cloner, &GitCloner::overallProgress, this, &GameManager::installOverallProgress);
    connect(m_cloner, &GitCloner::finished, this, [this](bool success, const QString& error) {
        if (success) {
            GameMeta meta;
            meta.title = m_installingGame.title;
            meta.version = m_installingGame.version;
            meta.desc = m_installingGame.desc;
            meta.engine = m_installingGame.engine;
            meta.size = m_installingGame.size;
            meta.page = m_installingGame.pageUrl;
            meta.gameDir = m_installingDir;
            meta.wadFiles = findGameWads(m_installingDir);
            writeMeta(m_installingId, meta);
            Config::instance().setGameDir(m_installingId, m_installingDir);
        }
        emit installFinished(m_installingId, success, error);
        m_cloner->deleteLater();
        m_cloner = nullptr;
    });

    m_cloner->start(game.repoUrl, gameDir, game.branch);
}

QString GameManager::launchGame(const QString& id) {
    QString gameDir = Config::instance().gameDir(id);
    QString metaPath = QDir(gameDir).filePath("meta.json");
    if (!QFile::exists(metaPath)) return "Gioco non installato";

    GameMeta meta = loadMeta(id);

    // GZDoom è già dentro la repo del gioco: lo cerchiamo lì (il path nelle impostazioni resta solo come ripiego)
#ifdef Q_OS_WIN
    const QString exeName = "gzdoom.exe";
#else
    const QString exeName = "gzdoom";
#endif
    QString gzdoomExe = QDir(gameDir).filePath(exeName);
    if (!QFile::exists(gzdoomExe)) gzdoomExe = findFileRecursive(gameDir, exeName, false);
    if (gzdoomExe.isEmpty() || !QFile::exists(gzdoomExe)) gzdoomExe = Config::instance().gzdoomPath();
    if (gzdoomExe.isEmpty() || !QFile::exists(gzdoomExe)) return "gzdoom_missing";

    // doom2.wad: prima quello scelto nella gestione del gioco, poi il vecchio path globale, poi la cartella del gioco
    QString doom2wad = meta.doom2WadPath.isEmpty() ? Config::instance().doom2WadPath() : meta.doom2WadPath;
    if (doom2wad.isEmpty() || !QFile::exists(doom2wad)) doom2wad = findFileRecursive(gameDir, "doom2.wad", false);
    if (doom2wad.isEmpty() || !QFile::exists(doom2wad)) return "doom2_missing";

    QStringList wads = meta.wadFiles;
    bool allExist = !wads.isEmpty();
    for (const auto& w : wads) if (!QFile::exists(w)) { allExist = false; break; }
    if (!allExist) {
        wads = findGameWads(gameDir);
        if (wads.isEmpty()) return "Nessun file WAD o PK3 trovato per questo gioco";
        meta.wadFiles = wads;
        writeMeta(id, meta);
    }

    QStringList args;
    args << "-iwad" << doom2wad;
    if (!wads.isEmpty()) {
        args << "-file";
        args << wads;
    }

    QProcess proc;
    proc.setProgram(gzdoomExe);
    proc.setArguments(args);
    proc.setWorkingDirectory(QFileInfo(gzdoomExe).absolutePath());
    if (!proc.startDetached()) return "Impossibile avviare GZDoom";
    return QString();
}

void GameManager::removeGame(const QString& id) {
    QString gameDir = Config::instance().gameDir(id);
    if (QDir(gameDir).exists()) QDir(gameDir).removeRecursively();
    Config::instance().removeGame(id);
}

bool GameManager::openGameFolder(const QString& id) {
    QString gameDir = Config::instance().gameDir(id);
    if (!QDir(gameDir).exists()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(gameDir));
}

int GameManager::clearGameSaves(const QString& id) {
    QString gameDir = Config::instance().gameDir(id);
    int count = 0;
    QDirIterator it(gameDir, QStringList() << "*.gzd", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { QFile::remove(it.next()); count++; }
    return count;
}

bool GameManager::resetGameConfig(const QString& id) {
    QString gameDir = Config::instance().gameDir(id);
    QDirIterator it(gameDir, QStringList() << "*.ini", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) QFile::remove(it.next());
    return true;
}

void GameManager::setDoom2WadForGame(const QString& id, const QString& path) {
    GameMeta meta = loadMeta(id);
    meta.doom2WadPath = path;
    if (meta.gameDir.isEmpty()) meta.gameDir = Config::instance().gameDir(id);
    writeMeta(id, meta);
}

QString GameManager::findFileRecursive(const QString& dir, const QString& nameOrExt, bool isExt) const {
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString f = it.next();
        QFileInfo fi(f);
        if (isExt) {
            if (fi.suffix().compare(nameOrExt.mid(1), Qt::CaseInsensitive) == 0) return f;
        } else {
            if (fi.fileName().compare(nameOrExt, Qt::CaseInsensitive) == 0) return f;
        }
    }
    return QString();
}

QStringList GameManager::findGameWads(const QString& dir) const {
    QStringList found;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString f = it.next();
        QFileInfo fi(f);
        QString ext = "." + fi.suffix().toLower();
        if (WAD_EXTS.contains(ext) && !WAD_SKIP.contains(fi.fileName().toLower()))
            found << f;
    }
    return found;
}
