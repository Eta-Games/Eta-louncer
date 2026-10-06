#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;

struct BroadcastMessage {
    QString key;         // "<sorgente>:<id>", univoco e usato per ricordare i letti
    QString sourceId;    // "launcher" oppure l'id del gioco
    QString sourceName;  // "ETA Launcher" / titolo del gioco
    QString id, title, message, level, url;
    QString version;     // opzionale: versione a cui si riferisce il messaggio (es. "1.2.0")
    QStringList notes;   // opzionale: elenco di "note di rilascio" mostrato nel changelog
    QDateTime date;
    bool read = false;
};

// Notifiche "broadcast": ogni repo (launcher + giochi) può contenere un file broadcast.json.
// Il launcher guarda l'ultimo COMMIT che ha toccato quel file (GitHub API, con ETag: se non è cambiato
// non conta nel limite di richieste) e, solo se è nuovo, scarica il JSON e mostra i messaggi non letti.
//
// Formato di broadcast.json:
// { "messages": [ { "id": "...", "title": "...", "message": "...", "date": "2026-10-05",
//                   "level": "info|update|warning", "url": "(opzionale)", "expires": "(opzionale, data)",
//                   "version": "(opzionale)", "notes": ["(opzionale) nota 1", "nota 2"] } ] }
// title / message / notes possono essere stringhe oppure oggetti per lingua: {"it": "...", "en": "...", "de": "..."}.
// I messaggi con level "update" e le loro "notes" formano il changelog mostrato dopo un aggiornamento.
class BroadcastManager : public QObject {
    Q_OBJECT
public:
    struct Source { QString id, name, owner, repo; };

    explicit BroadcastManager(QObject* parent = nullptr);

    void start();      // primo controllo + timer ogni 5 minuti
    void refresh();
    void refreshSource(const QString& sourceId); // controlla subito una sola sorgente (es. dopo un aggiornamento)

    // L'ultimo messaggio di tipo "update" di una sorgente (le note dell'ultima release); false se non ce n'è
    bool latestUpdate(const QString& sourceId, BroadcastMessage* out) const;

    QList<BroadcastMessage> messages() const; // dal più recente
    int unreadCount() const;
    int unreadCount(const QString& sourceId) const;
    void markAllRead();
    void markRead(const QString& sourceId); // solo i messaggi di un ramo (launcher o gioco)
    QList<Source> sources() const { return m_sources; }

    static const QString FILE_NAME;

signals:
    void changed();
    void newMessages(QList<BroadcastMessage> fresh);

private:
    QNetworkAccessManager* m_nam;
    QTimer* m_timer;
    QList<Source> m_sources;
    QMap<QString, QList<BroadcastMessage>> m_bySource;
    bool m_initialLoadDone = false;

    void buildSources();
    void checkSource(const Source& s);
    void fetchFile(const Source& s, const QString& commitSha, const QString& etag);
    void applyJson(const Source& s, const QByteArray& json);
    void loadCache();
    QStringList readKeys() const;
};
