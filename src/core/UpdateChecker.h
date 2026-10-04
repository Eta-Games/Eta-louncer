#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;

struct UpdateInfo {
    bool hasUpdate = false;
    QString latestVersion;
    QString releaseName;
    QString releaseNotes;
    QString error;
};

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // releasesApi: es. https://api.github.com/repos/Eta-Games/Dante-s-Revenge/releases/latest
    void check(const QString& gameId, const QString& releasesApi, const QString& installedVersion);

signals:
    void result(const QString& gameId, UpdateInfo info);

private:
    QNetworkAccessManager* m_nam;
};
