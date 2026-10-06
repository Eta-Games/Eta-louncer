#include "MainWindow.h"
#include "LoginWidget.h"
#include "ProfileWidget.h"
#include "GameCardWidget.h"
#include "InstallProgressDialog.h"
#include "SettingsDialog.h"
#include "ManageDialog.h"
#include "ChangelogDialog.h"
#include "BroadcastWidget.h"
#include "FriendsWidget.h"
#include "TrayController.h"
#include "../core/FirestoreClient.h"
#include "../core/PresenceManager.h"
#include "../core/BroadcastManager.h"
#include "../core/FriendsManager.h"
#include "../core/Autostart.h"
#include "../core/Config.h"
#include "../core/LauncherSettings.h"
#include "../core/ThemeManager.h"
#include "../core/GameCatalog.h"
#include "../core/ShortcutManager.h"
#include "../core/I18n.h"

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
#include <QCloseEvent>
#include <QProgressDialog>
#include <QEventLoop>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>
#include <QSettings>
#include <QSystemTrayIcon>
#include <initializer_list>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlag(Qt::FramelessWindowHint);
    resize(1000, 680);
    setMinimumSize(820, 520);

    m_auth = new AuthManager(this);
    m_games = new GameManager(this);
    m_updateChecker = new UpdateChecker(this);
    m_fs = new FirestoreClient(m_auth, this);
    m_presence = new PresenceManager(m_auth, m_fs, this);
    m_friends = new FriendsManager(m_auth, m_fs, m_presence, this);
    m_tray = new TrayController(this);
    m_selfUpdater = new SelfUpdater(this);
    m_broadcast = new BroadcastManager(this);
    m_repoUpdater = new RepoUpdater(m_games, this);

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
    m_profile = new ProfileWidget(m_auth, m_fs);
    connect(m_profile, &ProfileWidget::goToLoginRequested, this, [this]() { showLogin(); });
    connect(m_profile, &ProfileWidget::loggedOut, this, [this]() {
        m_presence->stop(false);   // segna offline
        m_friends->clear();
        m_sessionStarted = false;
        showLogin();
    });
    connect(m_profile, &ProfileWidget::themeSelected, this, [this](const QString& id) {
        LauncherSettings::setTheme(id);
        applyTheme(id);
    });
    connect(m_profile, &ProfileWidget::gameActionRequested, this, [this](const QString& id) {
        const GameEntry* g = findGame(id);
        if (!g) return;
        goToPage(0);
        if (m_games->isInstalled(id)) onLaunchRequested(id);
        else onInstallRequested(*g);
    });
    m_profile->addExtraSection(T("Amici"), T("Amici"), new FriendsWidget(m_friends, m_presence));
    m_pages->addWidget(m_profile);

    // 2: Login — obbligatorio all'avvio, senza "continua senza account"
    m_login = new LoginWidget(m_auth);
    connect(m_login, &LoginWidget::loggedIn, this, [this](AuthUser) {
        m_loggedIn = true;
        m_nav->show();
        goToPage(0);
        startSession();
        if (LauncherSettings::checkUpdatesOnStart()) checkUpdatesForAll();
    });
    m_pages->addWidget(m_login);

    // 3: Broadcast (notifiche da broadcast.json delle repo)
    m_broadcastPage = new BroadcastWidget(m_broadcast);
    m_pages->addWidget(m_broadcastPage);

    setCentralWidget(central);
    for (QWidget* page : std::initializer_list<QWidget*>{m_gamesPage, m_profile, m_login, m_broadcastPage}) {
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

    // Stato online: quando parte un gioco il launcher lo segnala (e controlla quando il processo finisce)
    connect(m_games, &GameManager::gameStarted, m_presence, &PresenceManager::gameStarted);
    connect(m_presence, &PresenceManager::onlineChanged, this, &MainWindow::updateOnline);
    connect(m_presence, &PresenceManager::ownStatusChanged, this, &MainWindow::updateOwnStatus);
    connect(m_broadcast, &BroadcastManager::changed, this, &MainWindow::updateBroadcastNav);

    // Tray: il launcher può restare in background; i broadcast nuovi arrivano come notifica di sistema
    // Auto-aggiornamento del launcher (release GitHub su master): scarica, chiude, sostituisce, riavvia
    connect(m_selfUpdater, &SelfUpdater::updateAvailable, this, [this](const SelfUpdateInfo& info) {
        m_selfInfo = info;
        m_hasSelfUpdate = true;
        if (m_tray->isVisible() && (!isVisible() || isMinimized())) {   // in background: notifica, la domanda arriva all'apertura
            m_tray->notify(T("ETA Launcher"), T("Nuova versione disponibile: %1").arg(info.tag), [this]() { showFromTray(); });
            return;
        }
        QTimer::singleShot(0, this, &MainWindow::askSelfUpdate);
    });
    connect(m_tray, &TrayController::openRequested, this, &MainWindow::showFromTray);
    // spegnimento / logoff di Windows: la finestra non deve rifiutare la chiusura nascondendosi nella tray
    connect(qApp, &QGuiApplication::commitDataRequest, this, [this]() { m_quitting = true; });
    connect(m_tray, &TrayController::quitRequested, this, [this]() { m_quitting = true; close(); qApp->quit(); });
    connect(m_broadcast, &BroadcastManager::newMessages, this, [this](const QList<BroadcastMessage>& fresh) {
        if (fresh.isEmpty() || !m_tray->isVisible() || isActiveWindow()) return;   // se lo stai guardando lo vedi già
        const BroadcastMessage& m = fresh.first();
        const QString title = m.sourceName.isEmpty() ? T("Nuovo messaggio") : m.sourceName;
        const QString what = m.title.isEmpty() ? m.message : m.title;
        m_tray->notify(title, fresh.size() == 1 ? what : T("%1 nuovi messaggi").arg(fresh.size()),
                       [this]() { showFromTray(); goToPage(3); });
    });
    applyTraySetting();
    connect(m_repoUpdater, &RepoUpdater::updateAvailable, this, &MainWindow::onRepoUpdateAvailable);
    connect(m_repoUpdater, &RepoUpdater::manualCheckDone, this, [this](const QString& id, bool found, const QString& error) {
        if (auto* c = m_cards.value(id)) c->setChecking(false);
        const GameEntry* g = findGame(id);
        const QString name = g ? g->title : id;
        if (!error.isEmpty())
            QMessageBox::warning(this, T("Controlla aggiornamenti"), T("Non sono riuscito a controllare gli aggiornamenti di %1.\n\n%2").arg(name, error));
        else if (!found)
            QMessageBox::information(this, T("Controlla aggiornamenti"), T("%1 è già aggiornato.").arg(name));
        // se found: la richiesta di aggiornamento è già comparsa da sola
    });

    showLogin(); // si parte dal login...
    if (m_auth->restoreSession()) m_login->setRestoring(true); // ...ma se c'è una sessione salvata si entra da soli
}

// Dopo il login: segnala lo stato online e inizia a leggere i broadcast
void MainWindow::startSession() {
    if (m_sessionStarted) return;
    m_sessionStarted = true;
    m_presence->start();
    m_friends->reload();
    m_broadcast->start();
    m_repoUpdater->start(); // confronta launcher e giochi con le repo su GitHub
    m_selfUpdater->start(); // release del launcher su master
    checkLauncherChangelog();
}

// Finestra "Novità": note della release (dal broadcast.json) + titoli dei commit
void MainWindow::showChangelog(const QString& sourceId, const QString& name, const QString& caption,
                               const QStringList& commits, int totalCommits) {
    m_broadcast->refreshSource(sourceId); // le note potrebbero essere nel commit appena arrivato
    ChangelogDialog dlg(sourceId, name, m_broadcast, this);
    dlg.setCaption(caption);
    dlg.setCommits(commits, totalCommits);
    dlg.exec();
}

// Il launcher non si sostituisce da solo: quando l'utente avvia una versione nuova (commit di build diverso
// dall'ultimo visto) si apre la finestra delle novità, senza bloccare il resto del launcher.
void MainWindow::checkLauncherChangelog() {
    const QString cur = RepoUpdater::buildCommit();
    if (cur.isEmpty()) return;
    QSettings s;
    const QString seen = s.value("changelog/launcherSeen").toString();
    if (seen == cur) return;
    s.setValue("changelog/launcherSeen", cur);
    if (seen.isEmpty()) return; // primo avvio in assoluto: non c'è una versione precedente da confrontare

    m_broadcast->refreshSource("launcher");
    auto* dlg = new ChangelogDialog("launcher", "ETA Launcher", m_broadcast, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setCaption(T("ETA Launcher è stato aggiornato."));
    QPointer<ChangelogDialog> guard(dlg);
    connect(m_repoUpdater, &RepoUpdater::commitTitlesReady, dlg,
            [guard](const QString& id, const QStringList& titles, int total) {
        if (id != "launcher" || !guard) return;
        guard->setCommits(titles, total);
    });
    m_repoUpdater->fetchCommitTitles("launcher", seen, cur);
    dlg->show();
}

void MainWindow::updateOwnStatus() {
    const QString st = m_presence->ownStatus();
    QString text;
    if (st == "playing") {
        const GameEntry* g = findGame(m_presence->ownGameId());
        text = "● In gioco: " + (g ? g->title : m_presence->ownGameId());
    } else if (st == "online") {
        text = "● Online";
    } else {
        text = LauncherSettings::showOnlineStatus() ? QString() : "Stato online nascosto";
    }
    m_profile->setOnlineStatusText(text);
}

// Giocatori online: contatore nella pagina giochi + "N giocatori in partita" sulle card
void MainWindow::updateOnline() {
    const auto users = m_presence->online();
    m_onlinePill->setVisible(!users.isEmpty());
    m_onlinePill->setText(QString("● %1 online").arg(users.size()));

    QStringList lines;
    QMap<QString, int> playing;
    QMap<QString, QStringList> names;
    for (const auto& u : users) {
        const GameEntry* g = (u.status == "playing") ? findGame(u.gameId) : nullptr;
        lines << (g ? QString("%1 — in gioco: %2").arg(u.name, g->title) : QString("%1 — nel launcher").arg(u.name));
        if (g) { playing[g->id]++; names[g->id] << u.name; }
    }
    m_onlinePill->setToolTip(lines.join("\n"));
    for (auto it = m_cards.constBegin(); it != m_cards.constEnd(); ++it)
        it.value()->setOnlinePlayers(playing.value(it.key()), names.value(it.key()).join("\n"));
}

void MainWindow::updateBroadcastNav() {
    const int n = m_broadcast->unreadCount();
    m_navButtons[1]->setText(n > 0 ? QString("Broadcast (%1)").arg(n) : QString("Broadcast"));
}

void MainWindow::askSelfUpdate() {
    if (!m_hasSelfUpdate || m_selfBusy) return;
    m_selfBusy = true;
    const SelfUpdateInfo info = m_selfInfo;
    QMessageBox box(this);
    box.setWindowTitle(T("Aggiornamento del launcher"));
    box.setIcon(QMessageBox::Information);
    box.setText(T("È disponibile una nuova versione di <b>ETA Launcher</b> (%1).<br>Il launcher la scarica, si chiude, si aggiorna e si riavvia da solo.")
                    .arg(info.tag.toHtmlEscaped()));
    auto* yes = box.addButton(T("Aggiorna e riavvia"), QMessageBox::AcceptRole);
    box.addButton(T("Più tardi"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != yes) { m_hasSelfUpdate = false; m_selfBusy = false; return; }   // per questa sessione non lo richiede più
    downloadSelfUpdate(info);
}

void MainWindow::downloadSelfUpdate(const SelfUpdateInfo& info) {
    auto* dlg = new QProgressDialog(T("Scarico ETA Launcher %1…").arg(info.tag), T("Annulla"), 0, 100, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(T("Aggiornamento"));
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(false);
    dlg->setAutoReset(false);
    dlg->setMinimumDuration(0);
    dlg->setValue(0);

    connect(dlg, &QProgressDialog::canceled, m_selfUpdater, &SelfUpdater::cancel);
    connect(dlg, &QProgressDialog::canceled, this, [this]() { m_selfBusy = false; });
    connect(m_selfUpdater, &SelfUpdater::progress, dlg, [dlg](qint64 got, qint64 total) {
        if (total > 0) dlg->setValue(int(got * 100 / total));
    });
    connect(m_selfUpdater, &SelfUpdater::downloadFailed, dlg, [this, dlg](const QString& err) {
        dlg->close();
        m_selfBusy = false;
        QMessageBox::warning(this, T("Aggiornamento"), err);
    });
    connect(m_selfUpdater, &SelfUpdater::downloaded, dlg, [this, dlg](const QString& file, const SelfUpdateInfo& done) {
        dlg->close();
        QString err;
        if (!m_selfUpdater->apply(file, done, &err)) {
            m_selfBusy = false;
            QMessageBox::warning(this, T("Aggiornamento"), err);
            return;
        }
        m_quitting = true;   // lo script sostituisce i file e riavvia il launcher
        close();
        qApp->quit();
    });
    dlg->show();
    m_selfUpdater->download(info);
}

void MainWindow::applyTraySetting() {
    const bool want = QSystemTrayIcon::isSystemTrayAvailable() && (LauncherSettings::trayEnabled() || Autostart::isEnabled());
    m_tray->setVisible(want);
    qApp->setQuitOnLastWindowClosed(!want);   // con la tray la finestra nascosta non deve chiudere l'app
}

void MainWindow::startInBackground() {
    applyTraySetting();
    if (!m_tray->isVisible()) show();   // niente tray disponibile: meglio mostrare la finestra che restare invisibili
}

void MainWindow::showFromTray() {
    if (isMinimized()) showNormal(); else show();
    raise();
    activateWindow();
    flushPendingUpdates();
}

void MainWindow::flushPendingUpdates() {
    if (!m_updateQueue.isEmpty() && !m_updateAsking) QTimer::singleShot(300, this, &MainWindow::processUpdateQueue);
    if (m_hasSelfUpdate && !m_selfBusy) QTimer::singleShot(600, this, &MainWindow::askSelfUpdate);
}

void MainWindow::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange && isVisible() && !isMinimized()) flushPendingUpdates();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!m_quitting && m_tray->isVisible()) {   // la X nasconde la finestra, il launcher resta attivo nella tray
        e->ignore();
        hide();
        QSettings s;
        if (!s.value("tray/hintShown").toBool()) {
            s.setValue("tray/hintShown", true);
            m_tray->notify(T("ETA Launcher è ancora attivo"), T("Resta nella tray: clic destro sull'icona per uscire."));
        }
        return;
    }
    if (m_sessionStarted) m_presence->stop(true); // segna offline prima di uscire (attende al massimo 1.5 s)
    QMainWindow::closeEvent(e);
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
        connect(&dlg, &SettingsDialog::themeChanged, this, [this](const QString& id) {
            applyTheme(id);
            m_profile->syncThemeToCloud(id);
        });
        dlg.exec();
        m_presence->settingsChanged(); // l'utente può aver nascosto/mostrato il proprio stato online
        applyTraySetting();            // ...e attivato/disattivato la tray
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
// a sinistra, voci di sezione a destra (qui solo
// Negozio e Libreria, Broadcast e Profilo).
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

    // voce della navbar → pagina dello stack
    static const int navPages[3] = {0, 3, 1}; // Negozio e Libreria, Broadcast, Profilo
    const QStringList labels = {"Negozio e Libreria", "Broadcast", "Profilo"};
    for (int i = 0; i < labels.size(); ++i) {
        auto* btn = new QPushButton(labels[i]);
        btn->setObjectName("NavLink");
        btn->setCheckable(true);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        m_navButtons[i] = btn;
        l->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { goToPage(navPages[i]); });
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
    if (m_currentPage == 3 && index != 3) m_broadcastPage->markVisibleRead(); // uscendo, risultano letti solo quelli del ramo mostrato
    m_currentPage = index;
    m_pages->setCurrentIndex(index);
    // Il login (2) non ha una voce nella navbar; pagina → voce: 0 giochi, 3 broadcast, 1 profilo
    const int navIndex = (index == 0) ? 0 : (index == 3) ? 1 : (index == 1) ? 2 : -1;
    for (int i = 0; i < 3; ++i) m_navButtons[i]->setChecked(i == navIndex);
    if (index == 0) refreshStates();
    if (index == 1) { m_profile->refresh(); updateOwnStatus(); }
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

    m_onlinePill = new QLabel;
    m_onlinePill->setObjectName("OnlinePill");
    m_onlinePill->hide();
    top->addWidget(m_onlinePill, 0, Qt::AlignBottom);
    top->addSpacing(8);

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
        connect(card, &GameCardWidget::updateCheckRequested, this, [this](const QString& id) {
            if (auto* c = m_cards.value(id)) c->setChecking(true);
            m_repoUpdater->checkNow(id);
        });
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
    if (card) dlg->setCover(card->coverPixmap());
    connect(dlg, &InstallProgressDialog::cancelRequested, m_games, &GameManager::cancelInstall);
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

    ManageDialog dlg(m_games, *game, card ? card->coverPixmap() : QPixmap(), m_broadcast, this);
    connect(&dlg, &ManageDialog::uninstalled, this, [this](const QString&) { refreshStates(); });
    dlg.exec();
    refreshStates();
}

// ── Aggiornamenti da repo (launcher + giochi) ─────────────────────────────
void MainWindow::onRepoUpdateAvailable(RepoUpdate update) {
    if (update.isLauncher) return;   // il launcher si aggiorna con SelfUpdater (release su master), non con i commit di main
    m_updateQueue.append(update);
    if (m_tray->isVisible() && (!isVisible() || isMinimized())) {
        // in background niente finestre a sorpresa: notifica dalla tray, la domanda arriva quando apri il launcher
        m_tray->notify(update.isLauncher ? T("ETA Launcher") : update.name,
                       update.isLauncher ? T("Nuova versione del launcher disponibile.") : T("Aggiornamento disponibile."),
                       [this]() { showFromTray(); });
        return;
    }
    if (!m_updateAsking) QTimer::singleShot(0, this, &MainWindow::processUpdateQueue);
}

void MainWindow::processUpdateQueue() {
    if (m_updateAsking) return;
    m_updateAsking = true;

    while (!m_updateQueue.isEmpty()) {
        const RepoUpdate u = m_updateQueue.takeFirst();

        QString text = u.isLauncher
            ? QString("È disponibile una nuova versione di <b>ETA Launcher</b>.")
            : QString("È disponibile un aggiornamento per <b>%1</b>.").arg(u.name.toHtmlEscaped());
        text += QString("<br>La repo ha %1 commit in più rispetto alla tua copia.").arg(u.aheadBy);
        if (!u.recent.isEmpty()) {
            text += "<br><br>Ultime modifiche:<ul style='margin-top:2px'>";
            for (int i = 0; i < u.recent.size() && i < 5; ++i) text += "<li>" + u.recent.at(i).toHtmlEscaped() + "</li>";
            text += "</ul>";
        }
        text += u.isLauncher ? "Vuoi aprire la pagina per scaricarla?" : "Vuoi aggiornare adesso?";

        QMessageBox box(this);
        box.setWindowTitle(u.isLauncher ? "Aggiornamento del launcher" : "Aggiornamento del gioco");
        box.setIcon(QMessageBox::Information);
        box.setTextFormat(Qt::RichText);
        box.setText(text);
        auto* yes = box.addButton(u.isLauncher ? "Apri download" : "Aggiorna ora", QMessageBox::AcceptRole);
        box.addButton("Più tardi", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != yes) continue;

        if (u.isLauncher) {
            // il launcher non può sovrascrivere se stesso mentre gira: si scarica la nuova versione
            QDesktopServices::openUrl(QUrl(RepoUpdater::launcherPageUrl()));
        } else {
            updateGameNow(u);
        }
    }
    m_updateAsking = false;
}

// git pull con una finestrella di attesa; ritorna true se è andato a buon fine
bool MainWindow::updateGameNow(const RepoUpdate& u) {
    QProgressDialog wait(QString("Aggiorno %1…").arg(u.name), QString(), 0, 0, this);
    wait.setWindowTitle("Aggiornamento");
    wait.setCancelButton(nullptr);
    wait.setWindowModality(Qt::WindowModal);
    wait.setMinimumDuration(0);
    wait.show();

    bool ok = false;
    QString error;
    QEventLoop loop;
    auto conn = connect(m_games, &GameManager::updateFinished, &loop,
                        [&](const QString& id, bool success, const QString& err) {
        if (id != u.id) return;
        ok = success;
        error = err;
        loop.quit();
    });
    m_games->updateGame(u.id);
    loop.exec();
    disconnect(conn);
    wait.close();

    if (ok) {
        refreshStates();
        showChangelog(u.id, u.name, T("%1 è stato aggiornato.").arg(u.name), u.recent, u.aheadBy);
    } else {
        QMessageBox::warning(this, "Aggiornamento non riuscito",
            QString("Non sono riuscito ad aggiornare %1.\n\n%2\n\n"
                    "Se il gioco è in esecuzione chiudilo e riprova.").arg(u.name, error));
    }
    return ok;
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
