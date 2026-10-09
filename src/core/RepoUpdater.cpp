#include "RepoUpdater.h"
#include "GameManager.h"
#include "GameCatalog.h"
#include "Config.h"
#include "I18n.h"
#include <QNetworkAccessManager>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <functional>
#include <memory>

#ifndef ETA_BUILD_COMMIT
#define ETA_BUILD_COMMIT ""
#endif

static const char* LAUNCHER_OWNER  = "Eta-Games";
static const char* LAUNCHER_REPO   = "Eta-louncer";
static const char* LAUNCHER_BRANCH = "master";   // il launcher si confronta con questo branch

// "https://github.com/Eta-Games/Dante-s-Revenge.git" → {"Eta-Games", "Dante-s-Revenge"}
static bool parseRepo(const QString& url, QString* owner, QString* repo) {
    QString u = url;
    if (u.endsWith(".git")) u.chop(4);
    const QStringList parts = u.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;
    *repo = parts.last();
    *owner = parts.at(parts.size() - 2);
    return true;
}

// ── Helper git (nessuna chiamata all'API REST di GitHub, quindi nessun limite orario) ──────────
using GitDone = std::function<void(bool ok, const QString& out, const QString& err)>;

// Esegue git senza bloccare la UI. In caso di errore err contiene "git_missing", "git_failed" o l'output di git.
static void gitRun(QObject* ctx, const QStringList& args, const QString& workDir, int timeoutMs, GitDone done) {
    const QString exe = QStandardPaths::findExecutable("git");
    if (exe.isEmpty()) {
        QTimer::singleShot(0, ctx, [done]() { done(false, QString(), QStringLiteral("git_missing")); });
        return;
    }
    auto* p = new QProcess(ctx);
    if (!workDir.isEmpty()) p->setWorkingDirectory(workDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0"); // mai restare appesi in attesa di credenziali
    p->setProcessEnvironment(env);

    auto finished = std::make_shared<bool>(false);
    QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), ctx,
                     [p, done, finished](int code, QProcess::ExitStatus st) {
        if (*finished) return;
        *finished = true;
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        const bool ok = (st == QProcess::NormalExit && code == 0);
        done(ok, out, ok ? QString() : (err.isEmpty() ? QStringLiteral("git_failed") : err));
    });
    QObject::connect(p, &QProcess::errorOccurred, ctx, [p, done, finished](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || *finished) return;
        *finished = true;
        p->deleteLater();
        done(false, QString(), QStringLiteral("git_missing"));
    });
    QTimer::singleShot(timeoutMs, p, [p]() {   // se git resta appeso lo chiudiamo
        if (p->state() != QProcess::NotRunning) p->kill();
    });
    p->start(exe, args);
}

static QStringList joinArgs(const QStringList& a, const QStringList& b) { QStringList r = a; r << b; return r; }

static QString launcherUrl() {
    return QString("https://github.com/%1/%2.git").arg(LAUNCHER_OWNER, LAUNCHER_REPO);
}

// Il launcher non ha un clone sul PC: teniamo una copia "bare" senza file (solo la storia dei commit, pochi KB)
static QString launcherCacheDir() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("launcher-repo.git");
}

// Argomenti/cartella con cui lanciare git per un target
static QStringList baseArgs(bool isLauncher) {
    return isLauncher ? QStringList{"--git-dir=" + launcherCacheDir()} : QStringList();
}
static QString workDirFor(bool isLauncher, const QString& id) {
    return isLauncher ? QString() : Config::instance().gameDir(id);
}

// Crea (o aggiorna) la copia locale della storia del branch del launcher
static void ensureLauncherCache(QObject* ctx, std::function<void(bool ok, const QString& err)> done) {
    const QString dir = launcherCacheDir();
    const QString branch = LAUNCHER_BRANCH;
    if (!QFileInfo::exists(QDir(dir).filePath("HEAD"))) {
        QDir().mkpath(QFileInfo(dir).absolutePath());
        gitRun(ctx, {"clone", "--bare", "--filter=blob:none", "--quiet", "--single-branch", "--branch", branch,
                     launcherUrl(), dir}, QString(), 120000,
               [dir, done](bool ok, const QString&, const QString& err) {
            if (!ok) QDir(dir).removeRecursively();   // niente cache a metà
            done(ok, err);
        });
    } else {
        gitRun(ctx, {"--git-dir=" + dir, "fetch", "--quiet", "origin", "+refs/heads/" + branch + ":refs/heads/" + branch},
               QString(), 60000, [done](bool ok, const QString&, const QString& err) { done(ok, err); });
    }
}

static QString netError(const QString& err) {
    if (err == QLatin1String("git_missing"))
        return T("git non trovato nel PATH. Installa Git for Windows e riprova.");
    if (err.contains("couldn't find remote ref") || err.contains("Remote branch") || err.contains("not found in upstream"))
        return T("Il branch del launcher non esiste su GitHub.");
    const QString line = err.section('\n', -1).trimmed();
    QString msg = T("Impossibile contattare GitHub (rete assente o repo non raggiungibile).");
    if (!line.isEmpty() && line != QLatin1String("git_failed")) msg += "\n\n" + line;
    return msg;
}

// ──────────────────────────────────────────────────────────────────────────────────────────────
RepoUpdater::RepoUpdater(GameManager* games, QObject* parent) : QObject(parent), m_games(games) {
    m_nam = new QNetworkAccessManager(this);   // non più usato: resta solo per non cambiare l'header
    m_timer = new QTimer(this);
    m_timer->setInterval(5 * 60 * 1000);
    connect(m_timer, &QTimer::timeout, this, &RepoUpdater::refresh);
}

QString RepoUpdater::launcherPageUrl() {
    return QString("https://github.com/%1/%2/releases/latest").arg(LAUNCHER_OWNER, LAUNCHER_REPO);
}

QString RepoUpdater::buildCommit() { return QString::fromLatin1(ETA_BUILD_COMMIT); }

// Titoli dei commit tra due versioni, letti da git (per i giochi dal loro clone, per il launcher dalla copia bare)
void RepoUpdater::fetchCommitTitles(const QString& id, const QString& fromSha, const QString& toSha) {
    if (fromSha.isEmpty() || toSha.isEmpty() || fromSha == toSha) { emit commitTitlesReady(id, QStringList(), 0); return; }

    const bool isLauncher = (id == "launcher");
    if (!isLauncher && !findGame(id)) { emit commitTitlesReady(id, QStringList(), 0); return; }

    auto runLog = [this, id, fromSha, toSha, isLauncher]() {
        const QString range = fromSha + ".." + toSha;
        const QStringList base = baseArgs(isLauncher);
        const QString wd = workDirFor(isLauncher, id);
        gitRun(this, joinArgs(base, {"log", "--format=%s", "-n", "10", range}), wd, 20000,
               [this, id, base, wd, range](bool ok, const QString& out, const QString&) {
            if (!ok) { emit commitTitlesReady(id, QStringList(), 0); return; }
            QStringList titles;
            for (const QString& l : out.split('\n', Qt::SkipEmptyParts)) titles << l.trimmed();   // dal più recente
            gitRun(this, joinArgs(base, {"rev-list", "--count", range}), wd, 20000,
                   [this, id, titles](bool ok2, const QString& cnt, const QString&) {
                emit commitTitlesReady(id, titles, ok2 ? cnt.toInt() : int(titles.size()));
            });
        });
    };

    if (isLauncher) {
        ensureLauncherCache(this, [this, id, runLog](bool ok, const QString&) {
            if (!ok) { emit commitTitlesReady(id, QStringList(), 0); return; }
            runLog();
        });
    } else {
        runLog();
    }
}

void RepoUpdater::start() {
    m_timer->start();
    refresh();
}

void RepoUpdater::refresh() {
    for (const Target& t : targets()) checkTarget(t);
}

void RepoUpdater::checkNow(const QString& id) {
    for (const Target& t : targets()) {
        if (t.id == id) { checkTarget(t, true); return; }
    }
    emit manualCheckDone(id, false, T("Controllo non possibile: il gioco non è un clone git o git non è disponibile."));
}

QList<RepoUpdater::Target> RepoUpdater::targets() const {
    QList<Target> list;

    // Launcher: si può controllare solo se sappiamo con quale commit è stato compilato
    const QString buildCommit = QString::fromLatin1(ETA_BUILD_COMMIT);
    if (!buildCommit.isEmpty())
        list.append({"launcher", "ETA Launcher", LAUNCHER_OWNER, LAUNCHER_REPO, LAUNCHER_BRANCH, buildCommit, true});

    // Giochi installati
    for (const auto& g : gameCatalog()) {
        if (!m_games->isInstalled(g.id)) continue;
        QString owner, repo;
        if (!parseRepo(g.repoUrl, &owner, &repo)) continue;
        const QString local = m_games->localCommit(g.id);
        if (local.isEmpty()) continue; // la cartella non è un clone git: niente da confrontare
        list.append({g.id, g.title, owner, repo, g.branch, local, false});
    }
    return list;
}

// ── Passo 1: qual è il commit più recente della repo? ─────────────────────
//   gioco:    git fetch origin, poi git rev-parse @{u}
//   launcher: git ls-remote (se è cambiato, aggiorna la copia bare)
void RepoUpdater::checkTarget(const Target& t, bool manual) {
    const QString flight = "chk:" + t.id;
    if (m_inFlight.contains(flight)) {             // un controllo è già in corso
        if (manual) QTimer::singleShot(1500, this, [this, t]() { checkTarget(t, true); });
        return;
    }
    m_inFlight.insert(flight);

    auto fail = [this, t, flight, manual](const QString& err) {
        m_inFlight.remove(flight);
        if (manual) emit manualCheckDone(t.id, false, netError(err));   // in automatico riproviamo al prossimo giro
    };

    // Ho lo sha remoto: se è diverso dal locale passo al confronto
    auto haveRemote = [this, t, flight, manual](const QString& remoteSha) {
        if (remoteSha.size() < 40 || remoteSha == t.localSha) {      // uguali (o risposta strana)
            m_inFlight.remove(flight);
            if (manual) emit manualCheckDone(t.id, false, QString());
            return;
        }
        if (!manual && m_done.contains(t.id + ":" + t.localSha + ":" + remoteSha)) {   // già proposto
            m_inFlight.remove(flight);
            return;
        }
        compare(t, remoteSha, manual);
    };

    if (!t.isLauncher) {
        const QString dir = workDirFor(false, t.id);
        gitRun(this, {"fetch", "--quiet", "origin"}, dir, 60000,
               [this, dir, fail, haveRemote](bool ok, const QString&, const QString& err) {
            if (!ok) { fail(err); return; }
            gitRun(this, {"rev-parse", "@{u}"}, dir, 15000,
                   [fail, haveRemote](bool ok2, const QString& sha, const QString& err2) {
                if (!ok2) { fail(err2); return; }
                haveRemote(sha.trimmed());
            });
        });
        return;
    }

    // launcher
    gitRun(this, {"ls-remote", launcherUrl(), QString("refs/heads/") + LAUNCHER_BRANCH}, QString(), 30000,
           [this, t, fail, haveRemote](bool ok, const QString& out, const QString& err) {
        if (!ok) { fail(err); return; }
        const QString remoteSha = out.section('\t', 0, 0).trimmed();
        if (remoteSha.isEmpty()) { fail(QStringLiteral("couldn't find remote ref")); return; }
        if (remoteSha == t.localSha) { haveRemote(remoteSha); return; }          // niente di nuovo
        ensureLauncherCache(this, [fail, haveRemote, remoteSha](bool ok2, const QString& err2) {
            if (!ok2) { fail(err2); return; }
            haveRemote(remoteSha);
        });
    });
}

// ── Passo 2: la repo è davvero AVANTI? (evita falsi allarmi se hai commit tuoi non pubblicati) ──
void RepoUpdater::compare(const Target& t, const QString& remoteSha, bool manual) {
    const QString key = t.id + ":" + t.localSha + ":" + remoteSha;
    const QString flight = "chk:" + t.id;
    const QStringList base = baseArgs(t.isLauncher);
    const QString wd = workDirFor(t.isLauncher, t.id);

    // "L R": L = commit solo tuoi, R = commit solo della repo
    gitRun(this, joinArgs(base, {"rev-list", "--left-right", "--count", t.localSha + "..." + remoteSha}), wd, 20000,
           [this, t, remoteSha, key, flight, manual, base, wd](bool ok, const QString& out, const QString&) {
        if (!ok) {   // il commit locale non esiste nella repo: la tua copia è diversa
            m_inFlight.remove(flight);
            m_done.insert(key);
            if (manual) emit manualCheckDone(t.id, false, T("La tua copia ha modifiche diverse da quelle della repo: aggiorna a mano o reinstalla."));
            return;
        }
        const QStringList parts = out.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        const int localOnly = parts.value(0).toInt();
        const int remoteOnly = parts.value(1).toInt();
        m_done.insert(key);
        if (localOnly > 0 || remoteOnly <= 0) {   // identica, indietro o divergente: niente richiesta
            m_inFlight.remove(flight);
            if (manual) emit manualCheckDone(t.id, false, QString());
            return;
        }

        gitRun(this, joinArgs(base, {"log", "--format=%s", "-n", "10", t.localSha + ".." + remoteSha}), wd, 20000,
               [this, t, remoteSha, flight, manual, remoteOnly](bool ok2, const QString& logOut, const QString&) {
            m_inFlight.remove(flight);
            RepoUpdate u;
            u.id = t.id;
            u.name = t.name;
            u.localSha = t.localSha;
            u.remoteSha = remoteSha;
            u.isLauncher = t.isLauncher;
            u.aheadBy = remoteOnly;
            if (ok2)
                for (const QString& l : logOut.split('\n', Qt::SkipEmptyParts)) u.recent << l.trimmed();   // dal più recente
            emit updateAvailable(u);
            if (manual) emit manualCheckDone(t.id, true, QString());
        });
    });
}
