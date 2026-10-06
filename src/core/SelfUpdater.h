#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

struct SelfUpdateInfo {
    QString tag, assetName, assetUrl;
    qint64 assetId = 0;
    qint64 size = 0;
    QDateTime updated;
};

// Auto-aggiornamento del launcher: cerca l'ultima release GitHub con target "master" (non bozza, non
// pre-release), scarica l'allegato (.zip con la cartella del launcher, oppure ETALauncher.exe), poi uno
// script .bat attende la chiusura dell'app, sostituisce i file e la riavvia.
// Solo Windows. Se la cartella non è scrivibile (es. Program Files) l'aggiornamento viene rifiutato.
class SelfUpdater : public QObject {
    Q_OBJECT
public:
    explicit SelfUpdater(QObject* parent = nullptr);

    void start();                       // controllo subito + ogni 2 ore
    void check(bool manual = false);    // manual=true: segnala anche "nessun aggiornamento" ed errori
    void download(const SelfUpdateInfo& info);
    void cancel();
    // Prepara lo script e lo avvia. true = ora chiudi l'app (lo script la sostituisce e la riavvia).
    bool apply(const QString& file, const SelfUpdateInfo& info, QString* error);

signals:
    void updateAvailable(SelfUpdateInfo info);
    void upToDate();
    void checkFailed(QString error);
    void progress(qint64 received, qint64 total);
    void downloaded(QString file, SelfUpdateInfo info);
    void downloadFailed(QString error);

private:
    QNetworkAccessManager* m_nam;
    QTimer* m_timer;
    QNetworkReply* m_reply = nullptr;
    bool m_checking = false;
    qint64 m_announced = 0;   // asset già segnalato in questa sessione
};
