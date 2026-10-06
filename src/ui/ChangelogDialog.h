#pragma once
#include <QDialog>
#include <QStringList>

class BroadcastManager;
class QVBoxLayout;
class QLabel;

// Finestra "Novità": si apre dopo un aggiornamento (gioco o launcher) e mostra
//  - le note dell'ultima release, lette dal broadcast.json della repo (messaggio con level "update":
//    titolo, messaggio e elenco "notes"),
//  - i titoli dei commit inclusi nell'aggiornamento.
// Se il broadcast viene scaricato mentre la finestra è aperta, le note si aggiornano da sole.
class ChangelogDialog : public QDialog {
    Q_OBJECT
public:
    ChangelogDialog(const QString& sourceId, const QString& name, BroadcastManager* broadcast,
                    QWidget* parent = nullptr);

    void setCaption(const QString& text);                   // riga sotto il titolo
    void setCommits(const QStringList& titles, int total);  // titoli dei commit (dal più recente)

private:
    QString m_sourceId;
    BroadcastManager* m_broadcast;
    QLabel* m_caption = nullptr;
    QVBoxLayout* m_notes = nullptr;
    QVBoxLayout* m_commits = nullptr;
    QStringList m_commitTitles;
    int m_commitTotal = 0;

    void rebuildNotes();
    void rebuildCommits();
    static void clear(QVBoxLayout* l);
};
