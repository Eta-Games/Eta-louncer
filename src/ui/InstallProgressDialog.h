#pragma once
#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QPlainTextEdit;

// Mostra l'avanzamento dell'installazione: fase corrente (Enumerating /
// Counting / Compressing / Receiving / Resolving deltas) con la relativa
// percentuale cosi' come stampata da `git clone`, piu' una barra complessiva
// calcolata da GitCloner::overallProgress.
class InstallProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit InstallProgressDialog(const QString& gameTitle, QWidget* parent = nullptr);

public slots:
    void onPhaseProgress(const QString& phase, int phasePct, const QString& rawLine);
    void onOverallProgress(int pct);
    void onFinished(bool success, const QString& error);

signals:
    void cancelRequested();

private:
    QLabel* m_phaseLabel;
    QProgressBar* m_bar;
    QPlainTextEdit* m_log;
    QPushButton* m_cancelBtn;

    static QString phaseDisplayName(const QString& phase);
};
