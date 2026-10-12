#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

struct SelfUpdateFile {
    QString path;       // percorso relativo nella repo (= nella cartella del launcher)
    QString sha;        // sha git del blob (serve a verificare il download)
    qint64 size = 0;
};

struct SelfUpdateInfo {
    QString tag;        // sha corto del commit su master
    QString sha;        // sha completo del commit su master
    QString title;      // prima riga del messaggio di commit
    QString body;       // resto del messaggio di commit
    QDateTime updated;  // data del commit
    QList<SelfUpdateFile> files;   // SOLO i file che sul PC sono diversi da master
    qint64 size = 0;               // byte totali da scaricare
};

// Auto-aggiornamento del launcher dal branch "master" della repo (dove si pubblica la cartella già compilata).
// Non servono GitHub Releases: si legge l'ultimo commit di master, si confronta l'elenco dei file di master
// con quelli accanto all'exe e si scaricano solo i file diversi (di solito solo ETALauncher.exe).
// Poi uno script .bat attende la chiusura dell'app, copia i file e la riavvia.
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
    // "stageDir" è la cartella con i file scaricati (quella ricevuta dal segnale downloaded).
    bool apply(const QString& stageDir, const SelfUpdateInfo& info, QString* error);

signals:
    void updateAvailable(SelfUpdateInfo info);
    void upToDate();
    void checkFailed(QString error);
    void progress(qint64 received, qint64 total);
    void downloaded(QString stageDir, SelfUpdateInfo info);
    void downloadFailed(QString error);

private:
    void checkTree(const QString& sha, const QString& title, const QString& body, const QDateTime& date, bool manual);
    void downloadNext();
    void failDownload(const QString& error);

    QNetworkAccessManager* m_nam;
    QTimer* m_timer;
    QNetworkReply* m_reply = nullptr;
    bool m_checking = false;
    bool m_cancelled = false;
    QString m_announced;          // commit già segnalato in questa sessione

    SelfUpdateInfo m_info;        // download in corso
    QString m_stageDir;
    int m_index = 0;
    qint64 m_doneBytes = 0;
};
