#include "MainWindow.h"
#include "LoginWidget.h"
#include "ProfileWidget.h"
#include "GameCardWidget.h"
#include "InstallProgressDialog.h"
#include "SettingsDialog.h"
#include "ManageDialog.h"
#include "../core/Config.h"
#include "../core/LauncherSettings.h"
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
#include <QTimer>
#include <QCoreApplication>
#include <initializer_list>

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

    // 0: Negozio e Libreria (unite)
    buildGamesPage();
    m_pages->addWidget(m_gamesPage);

    // 1: Profilo (da qui si accede / si modificano le impostazioni dell'account)
    m_profile = new ProfileWidget(m_auth);
    connect(m_profile, &ProfileWidget::goToLoginRequested, this, [this]() { showLogin(); });
    m_pages->addWidget(m_profile);

    // 2: Login — obbligatorio all'avvio, senza "continua senza account"
    m_login = new LoginWidget(m_auth);
    connect(m_login, &LoginWidget::loggedIn, this, [this](AuthUser) {
        m_loggedIn = true;
        m_nav->show();
        goToPage(0);
        if (LauncherSettings::checkUpdatesOnStart()) checkUpdatesForAll();
    });
    m_pages->addWidget(m_login);

    setCentralWidget(central);
    for (QWidget* page : std::initializer_list<QWidget*>{m_gamesPage, m_profile, m_login}) {
        page->setObjectName("Page");
        page->setAttribute(Qt::WA_StyledBackground, true);
    }
    applyTheme(ThemeManager::normalizeThemeId(
        LauncherSettings::theme().isEmpty() ? Config::instance().theme() : LauncherSettings::theme()));

    connect(m_games, &GameManager::installFinished, this, [this](const QString&, bool, const QString&) {
        refreshStates();
    });

    // Risultati del controllo aggiornamenti: collegati una volta sola
    connect(m_updateChecker, &UpdateChecker::result, this, [this](const QString& id, UpdateInfo info) {
        if (m_cards.contains(id)) m_cards[id]->setUpdateAvailable(info.hasUpdate, info.latestVersion);
    });

    showLogin(); // si parte sempre dal login
}

void MainWindow::buildTitleBar(QWidget* host, QVBoxLayout* hostLayout) {
    auto* bar = new QWidget;
    bar->setObjectName("TitleBar");
    bar->setFixedHeight(32);
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(10, 0, 6, 0);
    l->addStretch();

    auto* settingsBtn = new QPushButton("⚙");
    settingsBtn->setFixedSize(28, 24);
    settingsBtn->setObjectName("TitleBtn");
    settingsBtn->setToolTip("Impostazioni");
    connect(settingsBtn, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(this);
        connect(&dlg, &SettingsDialog::themeChanged, this, &MainWindow::applyTheme);
        dlg.exec();
    });
    l->addWidget(settingsBtn);

    auto* minBtn = new QPushButton("—");
    minBtn->setFixedSize(28, 24);
    minBtn->setObjectName("TitleBtn");
    minBtn->setToolTip("Riduci a icona");
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    l->addWidget(minBtn);

    auto* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(28, 24);
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setToolTip("Chiudi");
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::close);
    l->addWidget(closeBtn);

    hostLayout->addWidget(bar);
}

// Riprende la navbar di eta-games.github.io: logo + "ETA Games" / "Studio"
// a sinistra, voci di sezione a destra (qui solo le 4 richieste invece delle
// 5 del sito: Negozio e Libreria, Profilo).
void MainWindow::buildSiteNav(QWidget* host, QVBoxLayout* hostLayout) {
    auto* nav = new QWidget;
    nav->setObjectName("SiteNav");
    m_nav = nav;
    nav->setFixedHeight(72);
    auto* l = new QHBoxLayout(nav);
    l->setContentsMargins(24, 10, 24, 10);
    l->setSpacing(14);

    auto* logo = new QLabel;
    QPixmap pix(":/logo.png"); // incorporato nell'exe
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

    const QStringList labels = {"Negozio e Libreria", "Profilo"};
    for (int i = 0; i < labels.size(); ++i) {
        auto* btn = new QPushButton(labels[i]);
        btn->setObjectName("NavLink");
        btn->setCheckable(true);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        m_navButtons[i] = btn;
        l->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { goToPage(i); });
    }
    m_navButtons[0]->setChecked(true);

    hostLayout->addWidget(nav);
}

void MainWindow::showLogin() {
    m_loggedIn = false;
    m_nav->hide(); // niente navigazione finché non si è dentro
    goToPage(2);
}

void MainWindow::goToPage(int index) {
    if (!m_loggedIn && index != 2) index = 2; // blocca l'accesso alle altre pagine senza login
    m_pages->setCurrentIndex(index);
    // Il login (2) non ha una voce nella navbar
    const int navIndex = (index == 2) ? -1 : index;
    for (int i = 0; i < 2; ++i) m_navButtons[i]->setChecked(i == navIndex);
    if (index == 0) refreshStates();
    if (index == 1) m_profile->refresh();
}

// Pagina unica "Negozio e Libreria": tutti i giochi del catalogo, con filtro Tutti / Installati.
// Non installato → "Installa"; installato → "Avvia" + "Gestisci".
void MainWindow::buildGamesPage() {
    m_gamesPage = new QWidget;
    m_onlyInstalled = LauncherSettings::openOnInstalled();
    auto* outer = new QVBoxLayout(m_gamesPage);
    outer->setContentsMargins(32, 24, 32, 0);
    outer->setSpacing(4);

    auto* top = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto* heading = new QLabel("Giochi");
    heading->setObjectName("Heading");
    titles->addWidget(heading);
    auto* sub = new QLabel("Installa i giochi ETA Games e avviali da qui.");
    sub->setObjectName("Muted");
    titles->addWidget(sub);
    top->addLayout(titles, 1);

    auto* filterGroup = new QButtonGroup(this);
    const QStringList filters = {"Tutti", "Installati"};
    for (int i = 0; i < filters.size(); ++i) {
        auto* b = new QPushButton(filters[i]);
        b->setObjectName("Seg");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == (m_onlyInstalled ? 1 : 0));
        filterGroup->addButton(b, i);
        top->addWidget(b, 0, Qt::AlignBottom);
    }
    connect(filterGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_onlyInstalled = (id == 1);
        reflowGrid();
    });
    outer->addLayout(top);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);

    m_gridHost = new QWidget;
    auto* hostLayout = new QVBoxLayout(m_gridHost);
    hostLayout->setContentsMargins(0, 16, 0, 24);
    hostLayout->setSpacing(16);

    m_grid = new QGridLayout;
    m_grid->setSpacing(20);
    hostLayout->addLayout(m_grid);

    m_emptyLabel = new QLabel("Nessun gioco installato. Passa a «Tutti» per installarne uno.");
    m_emptyLabel->setObjectName("Muted");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();
    hostLayout->addWidget(m_emptyLabel);
    hostLayout->addStretch(1);

    for (const auto& game : gameCatalog()) {
        auto* card = new GameCardWidget(game, m_gridHost);
        card->setInstalled(m_games->isInstalled(game.id));
        connect(card, &GameCardWidget::installRequested, this, &MainWindow::onInstallRequested);
        connect(card, &GameCardWidget::launchRequested, this, &MainWindow::onLaunchRequested);
        connect(card, &GameCardWidget::manageRequested, this, &MainWindow::onManageRequested);
        m_cards[game.id] = card;
    }
    reflowGrid();

    m_scroll->setWidget(m_gridHost);
    outer->addWidget(m_scroll, 1);
}

// Rimette le card in griglia: numero di colonne in base alla larghezza, rispettando il filtro.
void MainWindow::reflowGrid() {
    if (!m_grid) return;
    while (QLayoutItem* it = m_grid->takeAt(0)) delete it; // toglie solo gli item, non i widget
    for (int r = 0; r < m_grid->rowCount(); ++r) m_grid->setRowStretch(r, 0);

    QList<GameCardWidget*> visible;
    for (const auto& game : gameCatalog()) {
        GameCardWidget* c = m_cards.value(game.id);
        if (!c) continue;
        const bool show = !m_onlyInstalled || m_games->isInstalled(game.id);
        c->setVisible(show);
        if (show) visible << c;
    }

    const int cardMin = 300, gap = 20;
    const int cols = qBound(1, (m_scroll->viewport()->width() + gap) / (cardMin + gap), 4);
    int i = 0;
    for (GameCardWidget* c : visible) {
        m_grid->addWidget(c, i / cols, i % cols);
        ++i;
    }
    for (int c = 0; c < 4; ++c) m_grid->setColumnStretch(c, c < cols ? 1 : 0);
    m_emptyLabel->setVisible(visible.isEmpty());
}

void MainWindow::refreshStates() {
    for (auto it = m_cards.constBegin(); it != m_cards.constEnd(); ++it) {
        it.value()->setInstalled(m_games->isInstalled(it.key()));
        it.value()->setBusy(false);
    }
    reflowGrid();
}

void MainWindow::applyTheme(const QString& themeId) {
    qApp->setStyleSheet(ThemeManager::stylesheetFor(themeId));
}

void MainWindow::onInstallRequested(GameEntry game) {
    auto* card = m_cards.value(game.id);
    if (card) card->setBusy(true, "In coda…");

    auto* dlg = new InstallProgressDialog(game.title, this);
    connect(m_games, &GameManager::installPhase, dlg, &InstallProgressDialog::onPhaseProgress);
    connect(m_games, &GameManager::installOverallProgress, dlg, &InstallProgressDialog::onOverallProgress);
    connect(m_games, &GameManager::installFinished, dlg,
            [dlg, card](const QString&, bool success, const QString& error) {
                dlg->onFinished(success, error);
                if (card) card->setBusy(false);
            });

    m_games->installGame(game, LauncherSettings::gamesDir()); // vuoto = cartella predefinita
    dlg->exec();
    dlg->deleteLater();
    refreshStates();
}

void MainWindow::onLaunchRequested(const QString& id) {
    QString err = m_games->launchGame(id);
    if (err.isEmpty()) {
        if (LauncherSettings::closeOnLaunch()) close();
        return;
    }

    if (err == "doom2_missing") {
        QMessageBox box(this);
        box.setWindowTitle("doom2.wad mancante");
        box.setText("Per avviare questo gioco serve doom2.wad.\nSelezionalo dalla gestione del gioco.");
        auto* open = box.addButton("Apri gestione", QMessageBox::AcceptRole);
        box.addButton("Annulla", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == open) onManageRequested(id);
    } else if (err == "gzdoom_missing") {
        QMessageBox::warning(this, "GZDoom non trovato",
            "Non trovo GZDoom nella cartella del gioco. Prova a reinstallare il gioco.");
    } else {
        QMessageBox::warning(this, "Errore avvio", err);
    }
}

void MainWindow::onManageRequested(const QString& id) {
    const GameEntry* game = findGame(id);
    if (!game) return;
    GameCardWidget* card = m_cards.value(id);

    ManageDialog dlg(m_games, *game, card ? card->coverPixmap() : QPixmap(), this);
    connect(&dlg, &ManageDialog::uninstalled, this, [this](const QString&) { refreshStates(); });
    dlg.exec();
    refreshStates();
}

void MainWindow::checkUpdatesForAll() {
    for (const auto& game : gameCatalog()) {
        if (!m_games->isInstalled(game.id) || game.releasesApi.isEmpty()) continue;
        GameMeta meta = m_games->loadMeta(game.id);
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

void MainWindow::mouseReleaseEvent(QMouseEvent* e) {
    m_dragPos = QPoint();
    QMainWindow::mouseReleaseEvent(e);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    reflowGrid();
}
