#include "MainWindow.h"
#include "LoginWidget.h"
#include "ProfileWidget.h"
#include "GameCardWidget.h"
#include "InstallProgressDialog.h"
#include "SettingsDialog.h"
#include "../core/Config.h"
#include "../core/ThemeManager.h"
#include "../core/GameCatalog.h"
#include "../core/ShortcutManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QScrollArea>
#include <QPixmap>
#include <QButtonGroup>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlag(Qt::FramelessWindowHint);
    resize(1000, 680);
    setMinimumSize(820, 520);

    m_auth = new AuthManager(this);
    m_games = new GameManager(this);
    m_updateChecker = new UpdateChecker(this);

    auto* central = new QWidget;
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    buildTitleBar(central, rootLayout);
    buildSiteNav(central, rootLayout);

    m_pages = new QStackedWidget;
    rootLayout->addWidget(m_pages, 1);

    // 0: Login
    m_login = new LoginWidget(m_auth);
    connect(m_login, &LoginWidget::skipped, this, [this]() { goToPage(2); checkUpdatesForAll(); });
    connect(m_login, &LoginWidget::loggedIn, this, [this](AuthUser) { goToPage(1); checkUpdatesForAll(); });
    m_pages->addWidget(m_login);

    // 1: Profilo
    m_profile = new ProfileWidget(m_auth);
    connect(m_profile, &ProfileWidget::goToLoginRequested, this, [this]() { goToPage(0); });
    m_pages->addWidget(m_profile);

    // 2: Libreria (installati)
    buildLibraryPage();
    m_pages->addWidget(m_libraryPage);

    // 3: Negozio (catalogo)
    buildStorePage();
    m_pages->addWidget(m_storePage);

    setCentralWidget(central);
    applyTheme(ThemeManager::normalizeThemeId(Config::instance().theme()));

    connect(m_games, &GameManager::installFinished, this, [this](const QString&, bool, const QString&) {
        refreshLibrary();
    });

    goToPage(0);
}

void MainWindow::buildTitleBar(QWidget* host, QVBoxLayout* hostLayout) {
    auto* bar = new QWidget;
    bar->setObjectName("TitleBar");
    bar->setFixedHeight(32);
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(10, 0, 6, 0);
    l->addStretch();

    auto* settingsBtn = new QPushButton("⚙");
    settingsBtn->setFixedSize(24, 24);
    settingsBtn->setObjectName("Secondary");
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this);
        connect(&dlg, &SettingsDialog::themeChanged, this, &MainWindow::applyTheme);
        dlg.exec();
    });
    l->addWidget(settingsBtn);

    auto* minBtn = new QPushButton("—");
    minBtn->setFixedSize(24, 24);
    minBtn->setObjectName("Secondary");
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    l->addWidget(minBtn);

    auto* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(24, 24);
    closeBtn->setObjectName("Secondary");
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::close);
    l->addWidget(closeBtn);

    hostLayout->addWidget(bar);
}

// Riprende la navbar di eta-games.github.io: logo + "ETA Games" / "Studio"
// a sinistra, voci di sezione a destra (qui solo le 4 richieste invece delle
// 5 del sito: Login, Profilo, Libreria, Negozio).
void MainWindow::buildSiteNav(QWidget* host, QVBoxLayout* hostLayout) {
    auto* nav = new QWidget;
    nav->setObjectName("SiteNav");
    nav->setFixedHeight(72);
    auto* l = new QHBoxLayout(nav);
    l->setContentsMargins(24, 10, 24, 10);
    l->setSpacing(14);

    auto* logo = new QLabel;
    QPixmap pix("logo.png");
    if (!pix.isNull()) logo->setPixmap(pix.scaledToHeight(44, Qt::SmoothTransformation));
    l->addWidget(logo);

    auto* brandBox = new QVBoxLayout;
    brandBox->setSpacing(0);
    auto* brand = new QLabel("ETA Games");
    brand->setObjectName("Brand");
    auto* sub = new QLabel("Studio");
    sub->setObjectName("Muted");
    brandBox->addWidget(brand);
    brandBox->addWidget(sub);
    l->addLayout(brandBox);

    l->addStretch();

    const QStringList labels = {"Login", "Profilo", "Libreria", "Negozio"};
    auto* group = new QButtonGroup(this);
    for (int i = 0; i < labels.size(); ++i) {
        auto* btn = new QPushButton(labels[i]);
        btn->setObjectName("NavLink");
        btn->setCheckable(true);
        btn->setFlat(true);
        group->addButton(btn, i);
        m_navButtons[i] = btn;
        l->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { goToPage(i); });
    }
    m_navButtons[0]->setChecked(true);

    hostLayout->addWidget(nav);
}

void MainWindow::goToPage(int index) {
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < 4; ++i) m_navButtons[i]->setChecked(i == index);
    if (index == 1) m_profile->refresh();
    if (index == 2) refreshLibrary();
}

void MainWindow::buildStorePage() {
    m_storePage = new QWidget;
    auto* outer = new QVBoxLayout(m_storePage);

    auto* heading = new QLabel("Negozio");
    heading->setObjectName("Heading");
    outer->addWidget(heading);
    auto* sub = new QLabel("Scarica e installa i giochi ETA Games — l'installazione clona il repository del gioco.");
    sub->setObjectName("Muted");
    outer->addWidget(sub);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* grid = new QWidget;
    auto* gridLayout = new QGridLayout(grid);
    gridLayout->setSpacing(16);
    gridLayout->setContentsMargins(4, 16, 4, 16);

    int row = 0, col = 0;
    for (const auto& game : gameCatalog()) {
        auto* card = new GameCardWidget(game);
        card->setInstalled(m_games->isInstalled(game.id));
        connect(card, &GameCardWidget::installRequested, this, &MainWindow::onInstallRequested);
        connect(card, &GameCardWidget::launchRequested, this, &MainWindow::onLaunchRequested);
        connect(card, &GameCardWidget::manageRequested, this, &MainWindow::onManageRequested);
        m_storeCards[game.id] = card;
        gridLayout->addWidget(card, row, col);
        if (++col >= 3) { col = 0; row++; }
    }
    gridLayout->setRowStretch(row + 1, 1);
    scroll->setWidget(grid);
    outer->addWidget(scroll, 1);
}

void MainWindow::buildLibraryPage() {
    m_libraryPage = new QWidget;
    auto* outer = new QVBoxLayout(m_libraryPage);

    auto* heading = new QLabel("Libreria");
    heading->setObjectName("Heading");
    outer->addWidget(heading);
    auto* sub = new QLabel("I tuoi giochi installati.");
    sub->setObjectName("Muted");
    outer->addWidget(sub);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* grid = new QWidget;
    auto* gridLayout = new QGridLayout(grid);
    gridLayout->setSpacing(16);
    gridLayout->setContentsMargins(4, 16, 4, 16);

    int row = 0, col = 0;
    for (const auto& game : gameCatalog()) {
        auto* card = new GameCardWidget(game);
        card->setInstalled(m_games->isInstalled(game.id));
        connect(card, &GameCardWidget::launchRequested, this, &MainWindow::onLaunchRequested);
        connect(card, &GameCardWidget::manageRequested, this, &MainWindow::onManageRequested);
        m_libraryCards[game.id] = card;
        gridLayout->addWidget(card, row, col);
        if (++col >= 3) { col = 0; row++; }
    }
    gridLayout->setRowStretch(row + 1, 1);
    scroll->setWidget(grid);
    outer->addWidget(scroll, 1);
}

void MainWindow::refreshLibrary() {
    for (auto it = m_storeCards.constBegin(); it != m_storeCards.constEnd(); ++it) {
        bool installed = m_games->isInstalled(it.key());
        it.value()->setInstalled(installed);
        it.value()->setBusy(false);
    }
    for (auto it = m_libraryCards.constBegin(); it != m_libraryCards.constEnd(); ++it) {
        bool installed = m_games->isInstalled(it.key());
        it.value()->setInstalled(installed);
        it.value()->setVisible(installed); // la Libreria mostra solo gli installati
    }
}

void MainWindow::applyTheme(const QString& themeId) {
    qApp->setStyleSheet(ThemeManager::stylesheetFor(themeId));
}

void MainWindow::onInstallRequested(GameEntry game) {
    auto* card = m_storeCards.value(game.id);
    if (card) card->setBusy(true, "In coda…");

    auto* dlg = new InstallProgressDialog(game.title, this);
    connect(m_games, &GameManager::installPhase, dlg, &InstallProgressDialog::onPhaseProgress);
    connect(m_games, &GameManager::installOverallProgress, dlg, &InstallProgressDialog::onOverallProgress);
    connect(m_games, &GameManager::installFinished, dlg,
            [dlg, card](const QString&, bool success, const QString& error) {
                dlg->onFinished(success, error);
                if (card) card->setBusy(false);
            });

    m_games->installGame(game);
    dlg->exec();
    refreshLibrary();
}

void MainWindow::onLaunchRequested(const QString& id) {
    QString err = m_games->launchGame(id);
    if (err == "doom2_missing") {
        QMessageBox::warning(this, "File mancanti",
            "Non trovo GZDoom o doom2.wad. Impostali dal pannello Impostazioni.");
    } else if (!err.isEmpty()) {
        QMessageBox::warning(this, "Errore avvio", err);
    }
}

void MainWindow::onManageRequested(const QString& id) {
    QMessageBox box(this);
    box.setWindowTitle("Gestisci gioco");
    box.setText("Cosa vuoi fare?");
    auto* openFolder = box.addButton("Apri cartella", QMessageBox::ActionRole);
    auto* shortcut    = box.addButton("Crea collegamento desktop", QMessageBox::ActionRole);
    auto* clearSaves  = box.addButton("Cancella salvataggi", QMessageBox::ActionRole);
    auto* resetCfg    = box.addButton("Ripristina config (.ini)", QMessageBox::ActionRole);
    auto* remove      = box.addButton("Disinstalla", QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    GameMeta meta = m_games->loadMeta(id);
    auto* clicked = box.clickedButton();
    if (clicked == openFolder) {
        m_games->openGameFolder(id);
    } else if (clicked == shortcut) {
        bool ok = ShortcutManager::createDesktopShortcut(id, meta.title.isEmpty() ? id : meta.title);
        QMessageBox::information(this, "Collegamento",
            ok ? "Collegamento creato sul Desktop." : "Impossibile creare il collegamento.");
    } else if (clicked == clearSaves) {
        int n = m_games->clearGameSaves(id);
        QMessageBox::information(this, "Salvataggi", QString("Eliminati %1 file di salvataggio.").arg(n));
    } else if (clicked == resetCfg) {
        m_games->resetGameConfig(id);
        QMessageBox::information(this, "Config", "File .ini ripristinati.");
    } else if (clicked == remove) {
        if (QMessageBox::question(this, "Conferma", "Disinstallare " + meta.title + "?") == QMessageBox::Yes) {
            m_games->removeGame(id);
            refreshLibrary();
        }
    }
}

void MainWindow::checkUpdatesForAll() {
    for (const auto& game : gameCatalog()) {
        if (!m_games->isInstalled(game.id) || game.releasesApi.isEmpty()) continue;
        GameMeta meta = m_games->loadMeta(game.id);
        connect(m_updateChecker, &UpdateChecker::result, this,
                [this](const QString& id, UpdateInfo info) {
                    if (m_storeCards.contains(id)) m_storeCards[id]->setUpdateAvailable(info.hasUpdate, info.latestVersion);
                    if (m_libraryCards.contains(id)) m_libraryCards[id]->setUpdateAvailable(info.hasUpdate, info.latestVersion);
                }, Qt::UniqueConnection);
        m_updateChecker->check(game.id, game.releasesApi, meta.version);
    }
}

void MainWindow::handleDeepLink(const QString& url) {
    m_auth->handleDeepLink(url);
}

void MainWindow::launchDirectly(const QString& gameId) {
    onLaunchRequested(gameId);
}

void MainWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && e->position().y() <= 32) {
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
        e->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(e->globalPosition().toPoint() - m_dragPos);
        e->accept();
    }
}
