#pragma once
#include <QObject>
#include <functional>

class QSystemTrayIcon;
class QMenu;

// Icona nella tray: clic = apri il launcher, menu = Apri / Esci, più notifiche (es. nuovo broadcast).
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject* parent = nullptr);

    void setVisible(bool on);        // l'icona si crea al primo utilizzo
    bool isVisible() const;
    // Notifica di sistema; onClick viene eseguito se l'utente ci clicca sopra
    void notify(const QString& title, const QString& message, std::function<void()> onClick = nullptr);

signals:
    void openRequested();
    void quitRequested();

private:
    QSystemTrayIcon* m_icon = nullptr;
    QMenu* m_menu = nullptr;
    std::function<void()> m_onClick;
    void ensureIcon();
};
