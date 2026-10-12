#include "SelfUpdater.h"
#include "I18n.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <memory>
#ifdef Q_OS_WIN
  #include <windows.h>
#endif

static const char* OWNER  = "Eta-Games";
static const char* REPO   = "Eta-louncer";
static const char* BRANCH = "master";   // su master c'è la cartella già compilata (exe + DLL)
static const int RECHECK_MS = 2 * 60 * 60 * 1000;

static QNetworkRequest makeReq(const QUrl& url, const char* accept) {
    QNetworkRequest r(url);
    r.setRawHeader("User-Agent", "ETA-Launcher");
    r.setRawHeader("Accept", accept);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return r;
}

// Su master finiscono anche i file di build di CMake: non servono per far girare il launcher.
static bool isBuildJunk(const QString& p) {
    static const QStringList dirs = {"CMakeFiles/", "ETALauncher_autogen/", "Testing/", ".qt/", ".cmake/",
                                     ".qtc_clangd/", ".qtcreator/", ".git/", ".github/"};
    for (const QString& d : dirs) if (p.startsWith(d)) return true;
    static const QStringList names = {"CMakeCache.txt", "CMakeCache.txt.prev", "Makefile", "cmake_install.cmake",
                                      "qtcsettings.cmake", "README.md", ".gitignore", ".gitattributes"};
    return names.contains(p);
}

// I percorsi arrivano da internet e finiscono su disco: niente "..", percorsi assoluti o lettere di unità.
static bool isSafeRelPath(const QString& p) {
    if (p.isEmpty() || p.startsWith('/') || p.startsWith('\\') || p.contains(':') || p.contains('\\')) return false;
    for (const QString& part : p.split('/'))
        if (part.isEmpty() || part == "." || part == "..") return false;
    return true;
}

static QByteArray blobHeader(qint64 size) {
    return QByteArray("blob ") + QByteArray::number(size) + QByteArray(1, '\0');
}

// Stesso sha che git calcola per un file: sha1("blob <dimensione>\0" + contenuto)
static QString gitBlobSha(const QString& path, qint64 size) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(blobHeader(size));
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}

SelfUpdater::SelfUpdater(QObject* parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) {
    m_timer = new QTimer(this);
    m_timer->setInterval(RECHECK_MS);
    connect(m_timer, &QTimer::timeout, this, [this]() { check(false); });
}

void SelfUpdater::start() {
    check(false);
    m_timer->start();
}

// Passo 1: ultimo commit del branch master (sha + messaggio, che poi mostriamo come "novità")
void SelfUpdater::check(bool manual) {
#ifndef Q_OS_WIN
    if (manual) emit checkFailed(T("L'aggiornamento automatico è disponibile solo su Windows."));
    return;
#else
    if (m_checking) return;
    m_checking = true;
    const QUrl url(QString("https://api.github.com/repos/%1/%2/commits/%3").arg(OWNER, REPO, BRANCH));
    QNetworkReply* reply = m_nam->get(makeReq(url, "application/vnd.github+json"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            m_checking = false;
            if (!manual) return;
            if (http == 403 || http == 429) emit checkFailed(T("Limite richieste GitHub raggiunto, riprova più tardi."));
            else if (http == 404)           emit checkFailed(T("Il branch master non esiste su GitHub."));
            else                            emit checkFailed(reply->errorString());
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject c = o.value("commit").toObject();
        const QString sha = o.value("sha").toString();
        if (sha.size() < 40) {
            m_checking = false;
            if (manual) emit checkFailed(T("Risposta di GitHub non valida."));
            return;
        }
        const QString msg = c.value("message").toString();
        const QDateTime date = QDateTime::fromString(c.value("committer").toObject().value("date").toString(), Qt::ISODate);

        // già verificato che i file locali coincidono con questo commit: niente da fare
        if (QSettings().value("updater/okSha").toString() == sha) {
            m_checking = false;
            if (manual) emit upToDate();
            return;
        }
        checkTree(sha, msg.section('\n', 0, 0).trimmed(), msg.section('\n', 1).trimmed(), date, manual);
    });
#endif
}

// Passo 2: elenco dei file di master. Si scaricano solo quelli che sul PC mancano o sono diversi.
void SelfUpdater::checkTree(const QString& sha, const QString& title, const QString& body, const QDateTime& date, bool manual) {
    const QUrl url(QString("https://api.github.com/repos/%1/%2/git/trees/%3?recursive=1").arg(OWNER, REPO, sha));
    QNetworkReply* reply = m_nam->get(makeReq(url, "application/vnd.github+json"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, sha, title, body, date, manual]() {
        reply->deleteLater();
        m_checking = false;
        if (reply->error() != QNetworkReply::NoError) {
            if (manual) emit checkFailed(reply->errorString());
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray tree = root.value("tree").toArray();
        if (tree.isEmpty() || root.value("truncated").toBool()) {
            if (manual) emit checkFailed(T("Impossibile leggere l'elenco dei file di master."));
            return;
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        SelfUpdateInfo info;
        info.sha = sha;
        info.tag = sha.left(7);
        info.title = title;
        info.body = body;
        info.updated = date;

        bool exeDiffers = false;
        const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
        for (const auto& v : tree) {
            const QJsonObject e = v.toObject();
            if (e.value("type").toString() != "blob") continue;
            SelfUpdateFile f;
            f.path = e.value("path").toString();
            f.sha = e.value("sha").toString();
            f.size = qint64(e.value("size").toDouble());
            if (!isSafeRelPath(f.path) || isBuildJunk(f.path)) continue;

            const QFileInfo fi(QDir(appDir).filePath(f.path));
            // prima la dimensione (veloce), l'hash solo se la dimensione coincide
            const bool same = fi.exists() && fi.size() == f.size && gitBlobSha(fi.filePath(), f.size) == f.sha;
            if (same) continue;
            if (f.path == exeName) exeDiffers = true;
            info.files.append(f);
            info.size += f.size;
        }

        // build locale più recente del commit di master (sviluppo / già aggiornato a mano): non la sovrascrivo
        if (exeDiffers && date.isValid() &&
            QFileInfo(QCoreApplication::applicationFilePath()).lastModified() >= date) {
            QSettings().setValue("updater/okSha", sha);
            if (manual) emit upToDate();
            return;
        }
        if (info.files.isEmpty()) {   // i file coincidono già con master
            QSettings().setValue("updater/okSha", sha);
            if (manual) emit upToDate();
            return;
        }
        if (!manual && info.sha == m_announced) return;
        m_announced = info.sha;
        emit updateAvailable(info);
    });
}

// Scarica uno alla volta i file diversi in una cartella temporanea, verificando dimensione e sha di ognuno.
void SelfUpdater::download(const SelfUpdateInfo& info) {
    m_cancelled = false;
    m_info = info;
    m_index = 0;
    m_doneBytes = 0;
    m_stageDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                     .filePath("ETALauncher-update/" + info.tag);
    QDir(m_stageDir).removeRecursively();
    if (!QDir().mkpath(m_stageDir)) {
        emit downloadFailed(T("Non riesco a scrivere in %1.").arg(QDir::toNativeSeparators(m_stageDir)));
        return;
    }
    downloadNext();
}

void SelfUpdater::failDownload(const QString& error) {
    m_reply = nullptr;
    QDir(m_stageDir).removeRecursively();
    emit downloadFailed(error);
}

void SelfUpdater::downloadNext() {
    if (m_cancelled) return;
    if (m_index >= m_info.files.size()) {
        emit downloaded(m_stageDir, m_info);
        return;
    }
    const SelfUpdateFile f = m_info.files.at(m_index);
    const QString dest = QDir(m_stageDir).filePath(f.path);
    QDir().mkpath(QFileInfo(dest).absolutePath());
    auto* file = new QFile(dest, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        failDownload(T("Non riesco a scrivere in %1.").arg(QDir::toNativeSeparators(m_stageDir)));
        return;
    }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha1);
    hash->addData(blobHeader(f.size));

    // il file si prende dal commit preciso (non dal "ramo"), così coincide con l'elenco letto prima
    const QByteArray base = QString("https://raw.githubusercontent.com/%1/%2/%3/").arg(OWNER, REPO, m_info.sha).toUtf8();
    const QUrl url = QUrl::fromEncoded(base + QUrl::toPercentEncoding(f.path, "/"));

    QNetworkReply* reply = m_nam->get(makeReq(url, "application/octet-stream"));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, hash]() {
        const QByteArray d = reply->readAll();
        file->write(d);
        hash->addData(d);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64) {
        emit progress(m_doneBytes + got, m_info.size);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash, f, dest]() {
        const QByteArray rest = reply->readAll();
        file->write(rest);
        hash->addData(rest);
        file->close();
        const bool noError = reply->error() == QNetworkReply::NoError;
        const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
        const QString err = reply->errorString();
        reply->deleteLater();
        file->deleteLater();
        m_reply = nullptr;

        if (aborted || m_cancelled) {   // annullato dall'utente
            QDir(m_stageDir).removeRecursively();
            return;
        }
        if (!noError) { failDownload(err); return; }
        if (QFileInfo(dest).size() != f.size || QString::fromLatin1(hash->result().toHex()) != f.sha) {
            failDownload(T("Download incompleto o corrotto (%1), riprova.").arg(f.path));
            return;
        }
        m_doneBytes += f.size;
        ++m_index;
        downloadNext();
    });
}

void SelfUpdater::cancel() {
    m_cancelled = true;
    if (m_reply) m_reply->abort();
    else QDir(m_stageDir).removeRecursively();
}

bool SelfUpdater::apply(const QString& stageDir, const SelfUpdateInfo& info, QString* error) {
#ifdef Q_OS_WIN
    const QString appDir = QCoreApplication::applicationDirPath();
    QFile probe(QDir(appDir).filePath(".eta_write_test"));
    if (!probe.open(QIODevice::WriteOnly)) {
        if (error) *error = T("Non posso scrivere in %1. Sposta il launcher in una cartella tua (es. Documenti) e riprova.")
                                .arg(QDir::toNativeSeparators(appDir));
        return false;
    }
    probe.close();
    probe.remove();

    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString dir = QDir::toNativeSeparators(appDir);
    const QString src = QDir::toNativeSeparators(stageDir);
    const QString bat = QDir(QFileInfo(stageDir).absolutePath()).filePath("apply-update.bat");

    // Il launcher è ancora aperto (exe/DLL bloccati): lo script riprova ogni secondo finché la copia riesce.
    // robocopy: codici di uscita 0-7 = ok, 8 o più = qualche file non copiato.
    QString s;
    s += "@echo off\r\nchcp 65001 >nul\r\nset TRIES=0\r\n:apply\r\nset /a TRIES+=1\r\n";
    s += QString("robocopy \"%1\" \"%2\" /E /R:0 /W:0 /NFL /NDL /NJH /NJS /NP >nul\r\n").arg(src, dir);
    s += "if errorlevel 8 if %TRIES% LSS 30 (ping -n 2 127.0.0.1 >nul & goto apply)\r\n";
    s += QString("start \"\" \"%1\"\r\n").arg(exe);
    s += QString("rmdir /s /q \"%1\" >nul 2>&1\r\n").arg(src);
    s += "(goto) 2>nul & del \"%~f0\"\r\n";

    QFile f(bat);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = T("Non riesco a preparare lo script di aggiornamento.");
        return false;
    }
    f.write(s.toUtf8());
    f.close();

    QProcess p;
    p.setProgram("cmd.exe");
    p.setArguments({"/c", QDir::toNativeSeparators(bat)});
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a) {
        a->flags |= CREATE_NO_WINDOW;
        a->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        a->startupInfo->wShowWindow = SW_HIDE;
    });
    if (!p.startDetached()) {
        if (error) *error = T("Non riesco ad avviare lo script di aggiornamento.");
        return false;
    }
    QSettings().setValue("updater/okSha", info.sha);   // dopo il riavvio non richiede lo stesso aggiornamento
    return true;
#else
    Q_UNUSED(stageDir); Q_UNUSED(info);
    if (error) *error = T("L'aggiornamento automatico è disponibile solo su Windows.");
    return false;
#endif
}
