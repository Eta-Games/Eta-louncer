#include "GameManager.h"
#include "Config.h"
#include "I18n.h"
#include "ProcessUtil.h"
#include "PlayStats.h"
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
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <functional>
#include <memory>

static const QStringList WAD_EXTS = {".wad", ".pk3", ".pk7", ".ipk3"};
static const QStringList WAD_SKIP = {"doom2.wad", "doom.wad", "heretic.wad", "hexen.wad", "strife1.wad", "gzdoom.pk3",
                                 // file di supporto di GZDoom: li carica da solo l'exe, non vanno passati con -file
                                 "game_support.pk3", "game_widescreen_gfx.pk3", "brightmaps.pk3", "lights.pk3"};

// Statistiche di gioco: controlla ogni 5 s se GZDoom è vivo (indipendente da login/presenza).
// Se il launcher viene chiuso durante la partita, la sessione si chiude con il tempo fino a quel momento.
static void trackPlaySession(QObject* parent, const QString& id, qint64 pid) {
    if (pid <= 0) return;
    const qint64 start = QDateTime::currentSecsSinceEpoch();
    auto done = std::make_shared<bool>(false);
    auto finish = [id, start, done]() {
        if (*done) return;
        *done = true;
        const qint64 end = QDateTime::currentSecsSinceEpoch();
        PlayStats::instance().addSession(id, end, end - start);
    };
    auto* t = new QTimer(parent);
    t->setInterval(5000);
    QObject::connect(t, &QTimer::timeout, t, [t, finish, pid]() {
        if (!isProcessRunning(pid)) { finish(); t->deleteLater(); }
    });
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, t, finish);
    t->start();
}

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

void GameManager::cancelInstall() {
    if (m_cloner) m_cloner->cancel();
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
    qint64 pid = 0;
    if (!proc.startDetached(&pid)) return "Impossibile avviare GZDoom";
    emit gameStarted(id, pid); // serve allo stato online ("in gioco" finché il processo vive)
    trackPlaySession(this, id, pid);
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

QString GameManager::localCommit(const QString& id) const {
    const QString dir = Config::instance().gameDir(id);
    const QString gitExe = QStandardPaths::findExecutable("git");
    if (gitExe.isEmpty() || !QDir(QDir(dir).filePath(".git")).exists()) return QString();

    QProcess p;
    p.setWorkingDirectory(dir);
    p.start(gitExe, {"rev-parse", "HEAD"});
    if (!p.waitForFinished(5000) || p.exitCode() != 0) return QString();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

void GameManager::updateGame(const QString& id) {
    const QString dir = Config::instance().gameDir(id);
    const QString gitExe = QStandardPaths::findExecutable("git");
    if (gitExe.isEmpty()) {
        emit updateFinished(id, false, "git non trovato nel PATH. Installa Git for Windows e riprova.");
        return;
    }

    auto* p = new QProcess(this);
    p->setWorkingDirectory(dir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0"); // mai restare appesi in attesa di credenziali
    p->setProcessEnvironment(env);

    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p, id, dir](int code, QProcess::ExitStatus st) {
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        if (st != QProcess::NormalExit || code != 0) {
            emit updateFinished(id, false, err.isEmpty() ? "git pull non è andato a buon fine" : err);
            return;
        }
        // i file del gioco possono essere cambiati/aggiunti: aggiorniamo l'elenco dei WAD
        GameMeta meta = loadMeta(id);
        if (meta.installed) {
            meta.gameDir = dir;
            meta.wadFiles = findGameWads(dir);
            writeMeta(id, meta);
        }
        emit updateFinished(id, true, QString());
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, id](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        p->deleteLater();
        emit updateFinished(id, false, "Impossibile avviare git");
    });

    p->start(gitExe, {"pull", "--ff-only"});
}

// Esegue git nella cartella del gioco senza bloccare la UI; done(ok, stdout, stderr/codice errore)
static void runGit(QObject* parent, const QString& dir, const QStringList& args,
                   std::function<void(bool, const QByteArray&, const QString&)> done) {
    const QString gitExe = QStandardPaths::findExecutable("git");
    if (gitExe.isEmpty()) {
        QTimer::singleShot(0, parent, [done]() { done(false, QByteArray(), QStringLiteral("git_missing")); });
        return;
    }
    auto* p = new QProcess(parent);
    p->setWorkingDirectory(dir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    p->setProcessEnvironment(env);

    auto finished = std::make_shared<bool>(false);
    QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), parent,
                     [p, done, finished](int code, QProcess::ExitStatus st) {
        if (*finished) return;
        *finished = true;
        const QByteArray out = p->readAllStandardOutput();
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        const bool ok = (st == QProcess::NormalExit && code == 0);
        done(ok, out, ok ? QString() : (err.isEmpty() ? QStringLiteral("git_failed") : err));
    });
    QObject::connect(p, &QProcess::errorOccurred, parent, [p, done, finished](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || *finished) return;
        *finished = true;
        p->deleteLater();
        done(false, QByteArray(), QStringLiteral("git_missing"));
    });
    p->start(gitExe, args);
}

// File che la verifica non deve mai toccare: dati dell'utente e file del launcher
static bool isUserFile(const QString& relPath) {
    const QString l = relPath.toLower();
    return l.endsWith(".gzd") || l.endsWith(".ini") || l.endsWith("meta.json");
}

void GameManager::verifyGame(const QString& id) {
    const QString dir = Config::instance().gameDir(id);
    if (!QFileInfo::exists(QDir(dir).filePath(".git"))) {
        QTimer::singleShot(0, this, [this, id]() {
            emit verifyFinished(id, false, QStringList(), QStringList(), QStringLiteral("not_git"));
        });
        return;
    }
    runGit(this, dir, {"--no-optional-locks", "status", "--porcelain=v1", "-z", "--untracked-files=no"},
           [this, id](bool ok, const QByteArray& out, const QString& err) {
        if (!ok) { emit verifyFinished(id, false, QStringList(), QStringList(), err); return; }

        QStringList modified, deleted;
        const QList<QByteArray> parts = out.split('\0');
        for (int i = 0; i < parts.size(); ++i) {
            const QByteArray& e = parts.at(i);
            if (e.size() < 4) continue;                    // "XY path"
            const char x = e.at(0), y = e.at(1);
            const QString path = QString::fromUtf8(e.mid(3));
            if (x == 'R' || x == 'C') ++i;                 // con -z il vecchio nome è un campo a parte
            if (path.isEmpty() || isUserFile(path)) continue;
            if (x == 'D' || y == 'D') deleted << path;
            else modified << path;
        }
        emit verifyFinished(id, true, modified, deleted, QString());
    });
}

void GameManager::repairGame(const QString& id, const QStringList& files) {
    const QString dir = Config::instance().gameDir(id);
    auto remaining = std::make_shared<QStringList>(files);
    const int total = files.size();
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, id, dir, remaining, total, step]() {
        if (remaining->isEmpty()) {
            // i file ripristinati possono essere WAD: aggiorniamo l'elenco salvato nel meta.json
            GameMeta meta = loadMeta(id);
            if (meta.installed) {
                meta.gameDir = dir;
                meta.wadFiles = findGameWads(dir);
                writeMeta(id, meta);
            }
            emit repairFinished(id, true, total, QString());
            return;
        }
        QStringList args = {"checkout", "HEAD", "--"};
        const int n = qMin(100, int(remaining->size())); // a blocchi: la riga di comando di Windows ha un limite
        for (int i = 0; i < n; ++i) args << remaining->takeFirst();
        runGit(this, dir, args, [this, id, step](bool ok, const QByteArray&, const QString& err) {
            if (!ok) { emit repairFinished(id, false, 0, err); return; }
            (*step)();
        });
    };
    (*step)();
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
