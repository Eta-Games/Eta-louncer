#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include "GitCloner.h"
#include "GameCatalog.h"

struct GameMeta {
    QString title, version, desc, engine, size, page, gameDir, doom2WadPath;
    QStringList wadFiles;
    bool installed = false;
};

class GameManager : public QObject {
    Q_OBJECT
public:
    explicit GameManager(QObject* parent = nullptr);

    bool isInstalled(const QString& id) const;
    GameMeta loadMeta(const QString& id) const;

    // Avvia l'installazione clonando il repo del gioco (invece di scaricare uno zip).
    // La progress bar viene guidata dall'output di `git clone` (vedi GitCloner).
    void installGame(const GameEntry& game, const QString& customInstallDir = QString());

    void cancelInstall(); // ferma il git clone in corso

    QString launchGame(const QString& id); // ritorna stringa vuota se ok, altrimenti messaggio d'errore
    void removeGame(const QString& id);
    bool openGameFolder(const QString& id);
    int clearGameSaves(const QString& id);   // elimina i .gzd, ritorna quanti ne ha trovati
    bool resetGameConfig(const QString& id); // elimina i .ini

    void setDoom2WadForGame(const QString& id, const QString& path);

    // Aggiornamento tramite git: commit attuale della cartella del gioco (vuoto se non è un clone git)
    QString localCommit(const QString& id) const;
    // `git pull --ff-only` nella cartella del gioco; alla fine emette updateFinished
    void updateGame(const QString& id);
    // Aggiornamento forzato: `git fetch --prune origin` + `git reset --hard @{u}`.
    // Riporta i file tracciati all'ultima versione; i file non tracciati (salvataggi, config) restano.
    // Alla fine emette updateFinished.
    void forceUpdateGame(const QString& id);

    // Verifica e ripara (git status + git checkout, senza reinstallare): cerca i file del gioco
    // modificati o cancellati rispetto all'ultimo commit. Salvataggi (.gzd), configurazioni (.ini)
    // e meta.json vengono ignorati. Il risultato arriva con verifyFinished().
    void verifyGame(const QString& id);
    // Ripristina i file indicati (percorsi relativi alla cartella del gioco) con `git checkout HEAD -- ...`
    void repairGame(const QString& id, const QStringList& files);

signals:
    void installPhase(const QString& phase, int phasePct, const QString& rawLine);
    void installOverallProgress(int pct);
    void installFinished(const QString& id, bool success, const QString& error);
    void gameStarted(const QString& id, qint64 pid);
    void updateFinished(const QString& id, bool success, const QString& error);
    // error: vuoto se ok, oppure "git_missing" / "not_git" / testo di git
    void verifyFinished(const QString& id, bool success, QStringList modified, QStringList deleted, QString error);
    void repairFinished(const QString& id, bool success, int repaired, QString error);

private:
    QString findFileRecursive(const QString& dir, const QString& nameOrExt, bool isExt) const;
    QStringList findGameWads(const QString& dir) const;
    void writeMeta(const QString& id, const GameMeta& meta) const;

    GitCloner* m_cloner = nullptr;
    QString m_installingId;
    GameEntry m_installingGame;
    QString m_installingDir;
};
