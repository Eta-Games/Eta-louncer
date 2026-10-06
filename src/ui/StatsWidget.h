#pragma once
#include <QFrame>

class QVBoxLayout;

// Sezione "Statistiche di gioco" da mettere nella pagina Profilo:
// classifica dei giochi per ore giocate + ultima partita.
class StatsWidget : public QFrame {
    Q_OBJECT
public:
    explicit StatsWidget(QWidget* parent = nullptr);
    void refresh();

private:
    QVBoxLayout* m_rows = nullptr;
};
