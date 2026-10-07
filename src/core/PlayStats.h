#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QHash>
#include <QJsonObject>

struct GameStats {
    qint64 totalSecs = 0;
    qint64 lastPlayed = 0;   // epoch in secondi, 0 = mai giocato
    int sessions = 0;
};

// Statistiche di gioco locali, salvate in <AppData>/playstats.json.
// Singleton: GameManager scrive, schede gioco e pagina Profilo leggono.
class PlayStats : public QObject {
    Q_OBJECT
public:
    static PlayStats& instance();

    GameStats get(const QString& id) const;
    QList<QPair<QString, GameStats>> ranking() const;   // solo giochi con almeno 1 partita, per ore decrescenti
    QJsonObject toJson() const;                    // per Firestore: {gameId: {secs,last,n}}
    void mergeCloud(const QJsonObject& cloud);     // per ogni gioco tiene il valore maggiore
    void addSession(const QString& id, qint64 endEpoch, qint64 secs);

    static QString formatDuration(qint64 secs);
    static QString formatLast(qint64 epoch);

signals:
    void changed();

private:
    PlayStats();
    QString path() const;
    void load();
    void save() const;
    QHash<QString, GameStats> m_data;
};
