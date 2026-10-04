#include "SettingsDialog.h"
#include "../core/Config.h"
#include "../core/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Impostazioni");
    setMinimumWidth(420);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_themeCombo = new QComboBox;
    for (const auto& t : ThemeManager::themes()) m_themeCombo->addItem(t.label, t.id);
    int idx = m_themeCombo->findData(Config::instance().theme());
    if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        QString id = m_themeCombo->itemData(i).toString();
        Config::instance().setTheme(id);
        emit themeChanged(id);
    });
    form->addRow("Tema:", m_themeCombo);

    m_installDirLabel = new QLabel;
    auto* installBtn = new QPushButton("Scegli cartella installazione predefinita…");
    installBtn->setObjectName("Secondary");
    connect(installBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Cartella installazione giochi");
        if (!dir.isEmpty()) { Config::instance().setDefaultInstallDir(dir); refreshLabels(); }
    });
    form->addRow(installBtn);
    form->addRow("Cartella corrente:", m_installDirLabel);

    m_gzdoomLabel = new QLabel;
    auto* gzdoomBtn = new QPushButton("Seleziona gzdoom.exe…");
    gzdoomBtn->setObjectName("Secondary");
    connect(gzdoomBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Seleziona gzdoom.exe", QString(), "GZDoom (*.exe)");
        if (!f.isEmpty()) { Config::instance().setGzdoomPath(f); refreshLabels(); }
    });
    form->addRow(gzdoomBtn);
    form->addRow("GZDoom:", m_gzdoomLabel);

    m_doom2WadLabel = new QLabel;
    auto* wadBtn = new QPushButton("Seleziona doom2.wad…");
    wadBtn->setObjectName("Secondary");
    connect(wadBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Seleziona doom2.wad", QString(), "WAD (*.wad)");
        if (!f.isEmpty()) { Config::instance().setDoom2WadPath(f); refreshLabels(); }
    });
    form->addRow(wadBtn);
    form->addRow("doom2.wad:", m_doom2WadLabel);

    root->addLayout(form);

    auto* openDataBtn = new QPushButton("Apri cartella dati launcher");
    openDataBtn->setObjectName("Secondary");
    connect(openDataBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Config::instance().dataDir()));
    });
    root->addWidget(openDataBtn);

    auto* closeBtn = new QPushButton("Chiudi");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeBtn);

    refreshLabels();
}

void SettingsDialog::refreshLabels() {
    auto& cfg = Config::instance();
    m_installDirLabel->setText(cfg.defaultInstallDir().isEmpty() ? "(predefinita)" : cfg.defaultInstallDir());
    m_gzdoomLabel->setText(cfg.gzdoomPath().isEmpty() ? "(non impostato)" : cfg.gzdoomPath());
    m_doom2WadLabel->setText(cfg.doom2WadPath().isEmpty() ? "(non impostato)" : cfg.doom2WadPath());
}
