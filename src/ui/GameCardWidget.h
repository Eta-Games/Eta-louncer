#pragma once
#include <QFrame>
#include "../core/GameCatalog.h"

class QLabel;
class QPushButton;

class GameCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit GameCardWidget(const GameEntry& game, QWidget* parent = nullptr);

    void setInstalled(bool installed);
    void setBusy(bool busy, const QString& label = QString());
    void setUpdateAvailable(bool available, const QString& versionLabel = QString());

    const GameEntry& game() const { return m_game; }

signals:
    void installRequested(GameEntry game);
    void launchRequested(QString id);
    void manageRequested(QString id);

private:
    GameEntry m_game;
    QLabel* m_statusLabel;
    QLabel* m_updateBadge;
    QPushButton* m_actionBtn; // Installa / Avvia
    QPushButton* m_manageBtn;
    bool m_installed = false;
};
