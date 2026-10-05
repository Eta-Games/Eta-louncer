#pragma once
#include <QMainWindow>
#include <QMap>
#include <QPoint>
#include "../core/AuthManager.h"
#include "../core/GameManager.h"
#include "../core/UpdateChecker.h"
#include "../core/GameCatalog.h"

class QStackedWidget;
class QPushButton;
class QVBoxLayout;
class QGridLayout;
class QScrollArea;
class QLabel;
class LoginWidget;
class ProfileWidget;
class GameCardWidget;

// Pagine: 0 = Negozio e Libreria (unite), 1 = Profilo, 2 = Login.
// Il login è obbligatorio all'avvio: finché non si accede la navbar è nascosta.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void handleDeepLink(const QString& url);
    void launchDirectly(const QString& gameId);

public slots:
    void applyTheme(const QString& themeId);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    AuthManager* m_auth = nullptr;
    GameManager* m_games = nullptr;
    UpdateChecker* m_updateChecker = nullptr;

    QStackedWidget* m_pages = nullptr;
    LoginWidget* m_login = nullptr;
    ProfileWidget* m_profile = nullptr;
    QWidget* m_gamesPage = nullptr;

    QPushButton* m_navButtons[2] = {nullptr, nullptr};
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

    void onInstallRequested(GameEntry game);
    void onLaunchRequested(const QString& id);
    void onManageRequested(const QString& id);
};
