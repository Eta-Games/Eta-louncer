#pragma once
#include <QFrame>
#include <QPixmap>
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
    const QPixmap& coverPixmap() const { return m_coverPix; }

signals:
    void installRequested(GameEntry game);
    void launchRequested(QString id);
    void manageRequested(QString id);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    GameEntry m_game;
    QLabel* m_cover;
    QLabel* m_statusLabel;
    QLabel* m_updateBadge;
    QLabel* m_installedTag;
    QPushButton* m_actionBtn; // Installa / Avvia
    QPushButton* m_manageBtn;
    QPixmap m_coverPix;
    bool m_installed = false;

    void updateCover();
};
