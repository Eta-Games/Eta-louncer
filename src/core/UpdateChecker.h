#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>

class QNetworkAccessManager;

// Info sull'aggiornamento di un singolo gioco (GitHub Releases)
struct UpdateInfo {
    bool    hasUpdate     = false;
    QString latestVersion;
    QString releaseName;
    QString releaseNotes;
    QString error;
};

// Info sull'ultimo commit del branch master (usato come broadcast + aggiornamento launcher)
struct CommitInfo {
    QString sha;      // ID del commit = versione
    QString title;    // prima riga del messaggio di commit
    QString body;     // resto del messaggio di commit
    QString date;
    QString url;
    QString error;
    bool    hasUpdate = false;
};

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    UpdateChecker(QObject* parent = nullptr);

    // Giochi (Releases API)
    void check(const QString& gameId, const QString& releasesApi, const QString& installedVersion);

    // Launcher: ultimo commit di master
    void checkLauncher(const QString& installedSha);

signals:
    void result(const QString& gameId, const UpdateInfo& info);
    void launcherResult(const CommitInfo& info);

private:
    QNetworkAccessManager* m_nam = nullptr;
    QByteArray m_etag;
    CommitInfo m_cached;
};
