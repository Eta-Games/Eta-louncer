#include "SelfUpdater.h"
#include "I18n.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#ifdef Q_OS_WIN
  #include <windows.h>
#endif

static const char* OWNER  = "Eta-Games";
static const char* REPO   = "Eta-louncer";
static const char* BRANCH = "master";   // si aggiorna solo con le release fatte su master
static const int RECHECK_MS = 2 * 60 * 60 * 1000;

static QNetworkRequest makeReq(const QUrl& url, const char* accept) {
    QNetworkRequest r(url);
    r.setRawHeader("User-Agent", "ETA-Launcher");
    r.setRawHeader("Accept", accept);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return r;
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

void SelfUpdater::check(bool manual) {
    if (m_checking) return;
    m_checking = true;
    const QUrl url(QString("https://api.github.com/repos/%1/%2/releases?per_page=10").arg(OWNER, REPO));
    QNetworkReply* reply = m_nam->get(makeReq(url, "application/vnd.github+json"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
        reply->deleteLater();
        m_checking = false;
        if (reply->error() != QNetworkReply::NoError) {
            if (manual) emit checkFailed(reply->errorString());
            return;
        }
        const QJsonArray releases = QJsonDocument::fromJson(reply->readAll()).array();
        for (const auto& v : releases) {
            const QJsonObject r = v.toObject();
            if (r.value("draft").toBool() || r.value("prerelease").toBool()) continue;
            if (r.value("target_commitish").toString() != QString::fromLatin1(BRANCH)) continue;

            // allegato: meglio lo .zip (aggiorna anche le DLL di Qt), altrimenti l'.exe
            QJsonObject best;
            for (const auto& av : r.value("assets").toArray()) {
                const QJsonObject a = av.toObject();
                const QString n = a.value("name").toString().toLower();
                if (n.endsWith(".zip")) { best = a; break; }
                if (n.endsWith(".exe") && best.isEmpty()) best = a;
            }
            const QString tag = r.value("tag_name").toString();
            if (best.isEmpty()) {
                if (manual) emit checkFailed(T("La release %1 non ha file allegati (.zip o .exe).").arg(tag));
                return;
            }

            SelfUpdateInfo info;
            info.tag = tag;
            info.assetName = best.value("name").toString();
            info.assetUrl = best.value("browser_download_url").toString();
            info.assetId = qint64(best.value("id").toDouble());
            info.size = qint64(best.value("size").toDouble());
            info.updated = QDateTime::fromString(best.value("updated_at").toString(), Qt::ISODate);

            QSettings s;
            const qint64 installed = s.value("updater/installedAssetId").toLongLong();
            if (installed == info.assetId) { if (manual) emit upToDate(); return; }
            if (installed == 0) {
                // primo controllo: se l'exe è più recente dell'allegato (build locale / già aggiornato a mano) non c'è nulla da fare
                const QDateTime exeTime = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
                if (info.updated.isValid() && exeTime >= info.updated) {
                    s.setValue("updater/installedAssetId", info.assetId);
                    if (manual) emit upToDate();
                    return;
                }
            }
            if (!manual && info.assetId == m_announced) return;
            m_announced = info.assetId;
            emit updateAvailable(info);
            return;
        }
        if (manual) emit upToDate();   // nessuna release su master
    });
}

void SelfUpdater::download(const SelfUpdateInfo& info) {
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("ETALauncher-update");
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(info.assetName);
    auto* file = new QFile(path, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        emit downloadFailed(T("Non riesco a scrivere in %1.").arg(QDir::toNativeSeparators(dir)));
        return;
    }
    QNetworkReply* reply = m_nam->get(makeReq(QUrl(info.assetUrl), "application/octet-stream"));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, &SelfUpdater::progress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, info, path]() {
        m_reply = nullptr;
        file->write(reply->readAll());
        file->close();
        const bool noError = reply->error() == QNetworkReply::NoError;
        const bool aborted = reply->error() == QNetworkReply::OperationCanceledError;
        const QString err = reply->errorString();
        reply->deleteLater();
        file->deleteLater();
        if (noError && (info.size <= 0 || QFileInfo(path).size() == info.size)) {
            emit downloaded(path, info);
        } else {
            QFile::remove(path);
            if (!aborted) emit downloadFailed(noError ? T("Download incompleto, riprova.") : err);
        }
    });
}

void SelfUpdater::cancel() {
    if (m_reply) m_reply->abort();
}

bool SelfUpdater::apply(const QString& file, const SelfUpdateInfo& info, QString* error) {
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
    const QString src = QDir::toNativeSeparators(file);
    const bool isZip = file.toLower().endsWith(".zip");
    const QString bat = QDir(QFileInfo(file).absolutePath()).filePath("apply-update.bat");

    // Il launcher è ancora aperto (file bloccati): lo script riprova ogni secondo finché la copia riesce.
    QString s;
    s += "@echo off\r\nchcp 65001 >nul\r\nset TRIES=0\r\n:apply\r\nset /a TRIES+=1\r\n";
    s += isZip ? QString("tar -xf \"%1\" -C \"%2\"\r\n").arg(src, dir)
               : QString("copy /y \"%1\" \"%2\" >nul\r\n").arg(src, exe);
    s += "if errorlevel 1 if %TRIES% LSS 30 (ping -n 2 127.0.0.1 >nul & goto apply)\r\n";
    s += QString("start \"\" \"%1\"\r\n").arg(exe);
    s += QString("del \"%1\" >nul 2>&1\r\n").arg(src);
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
    QSettings().setValue("updater/installedAssetId", info.assetId);   // dopo il riavvio non richiede lo stesso aggiornamento
    return true;
#else
    Q_UNUSED(file); Q_UNUSED(info);
    if (error) *error = T("L'aggiornamento automatico è disponibile solo su Windows.");
    return false;
#endif
}
