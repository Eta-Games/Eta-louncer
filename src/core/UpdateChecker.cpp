#include "UpdateChecker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void UpdateChecker::check(const QString& gameId, const QString& releasesApi, const QString& installedVersion) {
    if (releasesApi.isEmpty()) {
        emit result(gameId, UpdateInfo{});
        return;
    }
    QNetworkRequest req((QUrl(releasesApi)));
    req.setRawHeader("User-Agent", "ETA-Launcher-CPP");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        UpdateInfo info;
        if (reply->error() != QNetworkReply::NoError) {
            info.error = reply->errorString();
            emit result(gameId, info);
            reply->deleteLater();
            return;
        }
        QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QString latestTag = o.value("tag_name").toString();
        info.latestVersion = latestTag;
        info.releaseName = o.value("name").toString(latestTag);
        info.releaseNotes = o.value("body").toString();
        info.hasUpdate = !latestTag.isEmpty() && latestTag != installedVersion;
        emit result(gameId, info);
        reply->deleteLater();
    });
}
