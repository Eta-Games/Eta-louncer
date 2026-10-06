#pragma once
#include <QWidget>
#include <QString>
#include <QList>
#include <QColor>

class BroadcastManager;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QButtonGroup;

// Pagina "Broadcast": notifiche lette dal file broadcast.json delle repo (launcher + giochi).
// Mostrate come il grafico dei rami di GitHub: ogni repo è un "ramo" con il suo colore,
// i messaggi sono i commit. In alto si sceglie quale ramo vedere: tutto, solo il launcher,
// oppure un singolo gioco.
class BroadcastWidget : public QWidget {
    Q_OBJECT
public:
    explicit BroadcastWidget(BroadcastManager* manager, QWidget* parent = nullptr);

    // Segna come letti i messaggi del ramo attualmente mostrato (tutti se è selezionato "Tutto")
    void markVisibleRead();

private:
    BroadcastManager* m_manager;
    QVBoxLayout* m_list = nullptr;
    QHBoxLayout* m_chips = nullptr;
    QButtonGroup* m_group = nullptr;
    QList<QPushButton*> m_chipButtons; // [0] = Tutto, poi uno per ogni sorgente
    QString m_filter;                  // vuoto = tutto, altrimenti id della sorgente

    void buildChips();
    void updateChipLabels();
    void rebuild();
    QColor colorForSource(const QString& sourceId) const;
};
