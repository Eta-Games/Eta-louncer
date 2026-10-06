#pragma once
#include <QDialog>
#include <QElapsedTimer>
#include <QPixmap>

class QLabel;
class QProgressBar;
class QPushButton;
class QPlainTextEdit;
class QTimer;

// Finestra di installazione con 3 passaggi, ognuno con la sua barra:
//   1. Preparazione  (git: enumerazione, conteggio, compressione — lavoro del server)
//   2. Download      (git: ricezione oggetti, con dimensione e velocità)
//   3. Installazione (git: risoluzione delta + scrittura dei file su disco)
// In alto c'è l'avanzamento totale (GitCloner::overallProgress) e il tempo trascorso.
class InstallProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit InstallProgressDialog(const QString& gameTitle, QWidget* parent = nullptr);

    void setCover(const QPixmap& cover);

public slots:
    void onPhaseProgress(const QString& phase, int phasePct, const QString& rawLine);
    void onOverallProgress(int pct);
    void onFinished(bool success, const QString& error);

signals:
    void cancelRequested();

protected:
    void reject() override; // Esc / X non annullano per sbaglio un download in corso

private:
    enum State { Pending, Active, Done, Error };
    struct Step {
        QLabel* badge = nullptr;
        QLabel* name = nullptr;
        QLabel* detail = nullptr;
        QLabel* pct = nullptr;
        QProgressBar* bar = nullptr;
        int value = 0;
        State state = Pending;
    };

    Step m_steps[3];
    QLabel* m_cover;
    QLabel* m_title;
    QLabel* m_subtitle;
    QLabel* m_total;
    QProgressBar* m_totalBar;
    QLabel* m_error;
    QPlainTextEdit* m_log;
    QPushButton* m_detailsBtn;
    QPushButton* m_cancelBtn;
    QTimer* m_clock;
    QElapsedTimer m_elapsed;
    bool m_finished = false;
    int m_counting = 0, m_compressing = 0, m_resolving = 0, m_checkout = 0;

    Step makeStep(QLayout* into, int number, const QString& name, const QString& detail);
    void setStep(int i, int pct, const QString& detail = QString());
    void finishStepsBefore(int i);
    void applyState(Step& s, State st, int index);
    void updateSubtitle();
};
