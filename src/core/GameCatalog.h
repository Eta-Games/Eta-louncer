#pragma once
#include <QString>
#include <QList>

struct GameEntry {
    QString id;
    QString title;
    QString version;
    QString desc;
    QString imgUrl;
    QString repoUrl;     // repo git da clonare (es. https://github.com/Eta-Games/Dante-s-Revenge.git)
    QString size;        // solo informativo, mostrato in UI
    QString engine;
    QString pageUrl;
    QString releasesApi; // GitHub API per il check aggiornamenti
    QString branch;      // branch/tag da clonare (vuoto = default del repo)
    bool needsDoom2Wad = false; // true = nella gestione del gioco compare la selezione di doom2.wad
};

// Sostituisce l'oggetto CATALOG definito in src/index.html.
// In futuro questa lista potrà essere scaricata da un endpoint remoto
// invece di stare hardcoded nel binario.
inline const QList<GameEntry>& gameCatalog() {
    static const QList<GameEntry> catalog = {
        GameEntry{
            /*id*/          "dantes_revenge",
            /*title*/       "Dante's Revenge",
            /*version*/     "alfa",
            /*desc*/        "WAD di Doom — Divina Commedia",
            /*imgUrl*/      "https://eta-games.github.io/img/TITLEPIC.png",
            /*repoUrl*/     "https://github.com/Eta-Games/Dante-s-Revenge.git",
            /*size*/        "41 MB",
            /*engine*/      "GZDoom",
            /*pageUrl*/     "https://eta-games.github.io/dantes_revenge.html",
            /*releasesApi*/ "https://api.github.com/repos/Eta-Games/Dante-s-Revenge/releases/latest",
            /*branch*/      "main",
            /*needsDoom2Wad*/ true
        }
    };
    return catalog;
}

inline const GameEntry* findGame(const QString& id) {
    for (const auto& g : gameCatalog())
        if (g.id == id) return &g;
    return nullptr;
}
