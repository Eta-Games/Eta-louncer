#include "GameCardWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

GameCardWidget::GameCardWidget(const GameEntry& game, QWidget* parent)
    : QFrame(parent), m_game(game) {
    setObjectName("Card");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(6);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(game.title);
    title->setStyleSheet("font-family: 'Rajdhani', 'Segoe UI', sans-serif; font-size: 14pt; font-weight: 700;");
    header->addWidget(title);
    header->addStretch();
    m_updateBadge = new QLabel;
    m_updateBadge->setStyleSheet("background:#ffc400; color:#111; border-radius:5px; padding:2px 8px; font-weight:600;");
    m_updateBadge->hide();
    header->addWidget(m_updateBadge);
    root->addLayout(header);

    auto* desc = new QLabel(game.desc);
    desc->setObjectName("Muted");
    desc->setWordWrap(true);
    root->addWidget(desc);

    auto* meta = new QLabel(QString("%1 · %2 · v%3").arg(game.engine, game.size, game.version));
    meta->setObjectName("Muted");
    root->addWidget(meta);

    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("Muted");
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);

    auto* btnRow = new QHBoxLayout;
    m_actionBtn = new QPushButton("Installa");
    connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
        if (m_installed) emit launchRequested(m_game.id);
        else emit installRequested(m_game);
    });
    btnRow->addWidget(m_actionBtn);

    m_manageBtn = new QPushButton("Gestisci");
    m_manageBtn->setObjectName("Secondary");
    m_manageBtn->setVisible(false);
    connect(m_manageBtn, &QPushButton::clicked, this, [this]() { emit manageRequested(m_game.id); });
    btnRow->addWidget(m_manageBtn);

    root->addLayout(btnRow);
}

void GameCardWidget::setInstalled(bool installed) {
    m_installed = installed;
    m_actionBtn->setText(installed ? "Avvia" : "Installa");
    m_manageBtn->setVisible(installed);
}

void GameCardWidget::setBusy(bool busy, const QString& label) {
    m_actionBtn->setEnabled(!busy);
    m_statusLabel->setVisible(busy && !label.isEmpty());
    if (busy) m_statusLabel->setText(label);
}

void GameCardWidget::setUpdateAvailable(bool available, const QString& versionLabel) {
    m_updateBadge->setVisible(available);
    if (available) m_updateBadge->setText("Aggiornamento " + versionLabel);
}
