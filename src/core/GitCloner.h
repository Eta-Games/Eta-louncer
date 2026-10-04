#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QMap>

// Clona un repository git in un processo esterno e trasforma le righe di
// progresso stampate da `git` (via stderr, terminate da \r) in una
// percentuale complessiva utilizzabile per una progress bar nativa.
//
// Esempio di righe che git stampa durante un clone (quelle che usiamo):
//   remote: Enumerating objects: 23, done.
//   remote: Counting objects: 100% (23/23), done.
//   remote: Compressing objects: 100% (19/19), done.
//   Receiving objects: 100% (23/23), 164.72 KiB | 1.26 MiB/s, done.
//   Resolving deltas: 100% (3/3), done.
class GitCloner : public QObject {
    Q_OBJECT
public:
    explicit GitCloner(QObject* parent = nullptr);

    // Avvia `git clone [--branch ref] --progress <url> <destDir>`
    // destDir NON deve esistere ancora (git lo crea lui).
    void start(const QString& repoUrl, const QString& destDir, const QString& ref = QString());

    void cancel();

signals:
    // phase: "counting" | "compressing" | "receiving" | "resolving" | "enumerating"
    // phasePct: percentuale della singola fase (0-100), -1 se la fase non riporta %
    void phaseProgress(const QString& phase, int phasePct, const QString& rawLine);
    // percentuale complessiva 0-100, calcolata come media pesata delle fasi
    void overallProgress(int pct);
    void finished(bool success, const QString& errorMessage);

private slots:
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessErrorOccurred(QProcess::ProcessError error);

private:
    void handleLine(const QString& line);
    void recomputeOverall();

    QProcess* m_process = nullptr;
    QByteArray m_stderrBuf;
    QMap<QString, int> m_phasePct; // ultimo valore noto per ciascuna fase
    QString m_lastError;
    bool m_cancelled = false;

    // Pesi delle fasi nel totale. "enumerating" non ha una percentuale propria
    // (git mostra solo il conteggio oggetti), quindi non entra nel calcolo pesato:
    // la consideriamo "fase 0" e la fase "counting" parte da lì.
    static constexpr double W_COUNTING    = 0.05;
    static constexpr double W_COMPRESSING = 0.05;
    static constexpr double W_RECEIVING   = 0.70;
    static constexpr double W_RESOLVING   = 0.20;
};
