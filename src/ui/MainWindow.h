#pragma once
#include <QMainWindow>
#include <QPoint>
#include <QMap>
#include "../core/AuthManager.h"
#include "../core/GameManager.h"
#include "../core/UpdateChecker.h"

class QStackedWidget;
class QWidget;
class QPushButton;
class QVBoxLayout;
class LoginWidget;
class ProfileWidget;
class GameCardWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void handleDeepLink(const QString& url);
    void launchDirectly(const QString& gameId);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    AuthManager* m_auth;
    GameManager* m_games;
    UpdateChecker* m_updateChecker;

    QStackedWidget* m_pages;     // 0=Login 1=Profilo 2=Libreria 3=Negozio
    LoginWidget* m_login;
    ProfileWidget* m_profile;
    QWidget* m_libraryPage;      // giochi installati
    QWidget* m_storePage;        // catalogo completo / installazione

    QMap<QString, GameCardWidget*> m_storeCards;
    QMap<QString, GameCardWidget*> m_libraryCards;

    QPushButton* m_navButtons[4];

    QPoint m_dragPos;

    void buildTitleBar(QWidget* host, QVBoxLayout* hostLayout);
    void buildSiteNav(QWidget* host, QVBoxLayout* hostLayout);
    void buildStorePage();
    void buildLibraryPage();
    void goToPage(int index);

    void applyTheme(const QString& themeId);
    void refreshLibrary();
    void onInstallRequested(GameEntry game);
    void onLaunchRequested(const QString& id);
    void onManageRequested(const QString& id);
    void checkUpdatesForAll();
};
