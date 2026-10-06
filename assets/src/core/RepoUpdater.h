#pragma once
#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;
class GameManager;

// Un aggiornamento trovato: la repo su GitHub è più avanti di quello che hai in locale.
struct RepoUpdate {
    QString id;            // "launcher" oppure l'id del gioco
    QString name;          // nome mostrato all'utente
    QString localSha, remoteSha;
    int aheadBy = 0;       // quanti commit nuovi ci sono sulla repo
    QStringList recent;    // titoli degli ultimi commit (dal più recente)
    bool isLauncher = false;
};

// Controlla ogni 5 minuti se la repo del launcher e quelle dei giochi installati
// sono più avanti della copia locale:
//   - gioco:    commit locale = `git rev-parse HEAD` nella cartella del gioco
//   - launcher: commit con cui è stato compilato (ETA_BUILD_COMMIT, scritto da CMake)
// Confronta con GitHub (ETag: se non c'è niente di nuovo la richiesta non conta nel limite)
// e, se la repo è davvero avanti, emette updateAvailable() → la UI chiede se aggiornare.
// Ogni aggiornamento viene proposto una sola volta per sessione (se rispondi "Più tardi"
// te lo richiede al prossimo avvio, o quando arriva un commit ancora più nuovo).
class RepoUpdater : public QObject {
    Q_OBJECT
public:
    explicit RepoUpdater(GameManager* games, QObject* parent = nullptr);

    void start();    // primo controllo + timer
    void refresh();

    static QString launcherPageUrl(); // dove scaricare il nuovo launcher

signals:
    void updateAvailable(RepoUpdate update);

private:
    struct Target {
        QString id, name, owner, repo, branch, localSha;
        bool isLauncher = false;
    };

    QList<Target> targets() const;
    void checkTarget(const Target& t);
    void compare(const Target& t, const QString& remoteSha);

    QNetworkAccessManager* m_nam;
    QTimer* m_timer;
    GameManager* m_games;
    QSet<QString> m_inFlight; // confronti in corso
    QSet<QString> m_done;     // confronti già conclusi (id:locale:remoto)
};
