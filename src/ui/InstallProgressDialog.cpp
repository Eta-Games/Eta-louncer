#include "InstallProgressDialog.h"
#include <QMap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>

InstallProgressDialog::InstallProgressDialog(const QString& gameTitle, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Installazione — " + gameTitle);
    setMinimumWidth(420);
    setModal(true);

    auto* root = new QVBoxLayout(this);

    auto* title = new QLabel("Installazione di " + gameTitle);
    title->setStyleSheet("font-size: 13pt; font-weight: 700;");
    root->addWidget(title);

    m_phaseLabel = new QLabel("Avvio clone del repository…");
    m_phaseLabel->setObjectName("Muted");
    root->addWidget(m_phaseLabel);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setFormat("%p%");
    root->addWidget(m_bar);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    m_log->setStyleSheet("font-family: Consolas, monospace; font-size: 9pt;");
    root->addWidget(m_log);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_cancelBtn = new QPushButton("Annulla");
    m_cancelBtn->setObjectName("Secondary");
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        emit cancelRequested();
        m_cancelBtn->setEnabled(false);
        m_phaseLabel->setText("Annullamento in corso…");
    });
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);
}

QString InstallProgressDialog::phaseDisplayName(const QString& phase) {
    static const QMap<QString, QString> names = {
        {"enumerating", "Enumerazione oggetti"},
        {"counting",    "Conteggio oggetti"},
        {"compressing", "Compressione oggetti"},
        {"receiving",   "Ricezione oggetti (download)"},
        {"resolving",   "Risoluzione delta"},
    };
    return names.value(phase, phase);
}

void InstallProgressDialog::onPhaseProgress(const QString& phase, int phasePct, const QString& rawLine) {
    QString label = phaseDisplayName(phase);
    if (phasePct >= 0) label += QString(": %1%").arg(phasePct);
    m_phaseLabel->setText(label);
    m_log->appendPlainText(rawLine);
}

void InstallProgressDialog::onOverallProgress(int pct) {
    m_bar->setValue(pct);
}

void InstallProgressDialog::onFinished(bool success, const QString& error) {
    m_cancelBtn->setText("Chiudi");
    m_cancelBtn->setEnabled(true);
    disconnect(m_cancelBtn, &QPushButton::clicked, nullptr, nullptr);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::accept);
    if (success) {
        m_phaseLabel->setText("Installazione completata ✔");
        m_bar->setValue(100);
    } else {
        m_phaseLabel->setText("Installazione fallita: " + error);
    }
}
