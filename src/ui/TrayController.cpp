#include "TrayController.h"
#include "../core/I18n.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>

TrayController::TrayController(QObject* parent) : QObject(parent) {}

void TrayController::ensureIcon() {
    if (m_icon) return;
    m_icon = new QSystemTrayIcon(QIcon(":/logo.png"), this);
    m_icon->setToolTip("ETA Games Launcher");

    m_menu = new QMenu;   // senza parent: il menu della tray non appartiene a nessuna finestra
    QAction* open = m_menu->addAction(T("Apri ETA Launcher"));
    m_menu->addSeparator();
    QAction* quit = m_menu->addAction(T("Esci"));
    connect(open, &QAction::triggered, this, &TrayController::openRequested);
    connect(quit, &QAction::triggered, this, &TrayController::quitRequested);
    m_icon->setContextMenu(m_menu);

    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) emit openRequested();
    });
    connect(m_icon, &QSystemTrayIcon::messageClicked, this, [this]() {
        if (m_onClick) m_onClick(); else emit openRequested();
    });
    connect(this, &QObject::destroyed, m_menu, &QObject::deleteLater);
}

void TrayController::setVisible(bool on) {
    if (on) ensureIcon();
    if (m_icon) m_icon->setVisible(on);
}

bool TrayController::isVisible() const { return m_icon && m_icon->isVisible(); }

void TrayController::notify(const QString& title, const QString& message, std::function<void()> onClick) {
    if (!isVisible()) return;
    m_onClick = onClick;
    m_icon->showMessage(title, message, QSystemTrayIcon::Information, 8000);
}
