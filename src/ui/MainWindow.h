#pragma once
#include <QMainWindow>
#include <QMap>
#include <QPoint>
#include "../core/AuthManager.h"
#include "../core/GameManager.h"
#include "../core/UpdateChecker.h"
#include "../core/GameCatalog.h"
#include "../core/RepoUpdater.h"
#include "../core/SelfUpdater.h"

class QStackedWidget;
class QPushButton;
class QVBoxLayout;
class QGridLayout;
class QScrollArea;
class QLabel;
class FirestoreClient;
class PresenceManager;
class FriendsManager;
class TrayController;
class BroadcastManager;
class BroadcastWidget;
class LoginWidget;
class ProfileWidget;
class GameCardWidget;

// Pagine: 0 = Negozio e Libreria (unite), 1 = Profilo, 2 = Login, 3 = Broadcast.
// Il login è obbligatorio all'avvio: finché non si accede la navbar è nascosta.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void handleDeepLink(const QString& url);
    void launchDirectly(const QString& gameId);
    void showFromTray();          // riporta la finestra davanti (anche se nascosta o ridotta a icona)
    void startInBackground();     // avvio con Windows: nella tray, senza aprire la finestra

public slots:
    void applyTheme(const QString& themeId);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    AuthManager* m_auth = nullptr;
    GameManager* m_games = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    FirestoreClient* m_fs = nullptr;
    PresenceManager* m_presence = nullptr;
    FriendsManager* m_friends = nullptr;
    TrayController* m_tray = nullptr;
    SelfUpdater* m_selfUpdater = nullptr;
    SelfUpdateInfo m_selfInfo;
    bool m_hasSelfUpdate = false;
    bool m_selfBusy = false;
    bool m_quitting = false;
    BroadcastManager* m_broadcast = nullptr;
    RepoUpdater* m_repoUpdater = nullptr;
    QList<RepoUpdate> m_updateQueue;
    bool m_updateAsking = false;
    BroadcastWidget* m_broadcastPage = nullptr;
    bool m_sessionStarted = false;
    int m_currentPage = 2;
    QLabel* m_onlinePill = nullptr;

    QStackedWidget* m_pages = nullptr;
    LoginWidget* m_login = nullptr;
    ProfileWidget* m_profile = nullptr;
    QWidget* m_gamesPage = nullptr;

    QPushButton* m_navButtons[3] = {nullptr, nullptr, nullptr};
    QWidget* m_nav = nullptr;
    bool m_loggedIn = false;

    // Pagina giochi
    QScrollArea* m_scroll = nullptr;
    QWidget* m_gridHost = nullptr;
    QGridLayout* m_grid = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QMap<QString, GameCardWidget*> m_cards;
    bool m_onlyInstalled = false;

    QPoint m_dragPos;

    void buildTitleBar(QWidget* host, QVBoxLayout* hostLayout);
    void buildSiteNav(QWidget* host, QVBoxLayout* hostLayout);
    void buildGamesPage();
    void goToPage(int index);
    void showLogin();
    void refreshStates();
    void reflowGrid();
    void checkUpdatesForAll();
    void startSession();        // dopo il login: stato online + broadcast
    void updateOnline();        // giocatori online sulle card e nella barra
    void updateOwnStatus();     // stato mostrato nel profilo
    void updateBroadcastNav();  // numero di non letti sulla voce Broadcast

    void showChangelog(const QString& sourceId, const QString& name, const QString& caption,
                       const QStringList& commits, int totalCommits);
    void applyTraySetting();
    void askSelfUpdate();
    void downloadSelfUpdate(const SelfUpdateInfo& info);
    void flushPendingUpdates();
    void checkLauncherChangelog();   // dopo un nuovo avvio con una versione diversa: mostra le novità

    void onRepoUpdateAvailable(RepoUpdate update);
    void processUpdateQueue();
    bool updateGameNow(const RepoUpdate& update);

    void onInstallRequested(GameEntry game);
    void onLaunchRequested(const QString& id);
    void onManageRequested(const QString& id);
};
