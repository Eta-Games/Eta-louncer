#pragma once
#include <QDialog>
#include <QPixmap>
#include <functional>
#include "../core/GameCatalog.h"

class GameManager;
class QLabel;
class QLineEdit;
class QVBoxLayout;

// Finestra "Gestisci gioco": righe in stile card del sito invece del vecchio QMessageBox.
// Per i giochi con needsDoom2Wad mostra anche la selezione di doom2.wad
// (che non sta più nelle impostazioni generali).
class ManageDialog : public QDialog {
    Q_OBJECT
public:
    ManageDialog(GameManager* games, const GameEntry& game, const QPixmap& cover, QWidget* parent = nullptr);

signals:
    void uninstalled(QString id);

private:
    GameManager* m_games;
    GameEntry m_game;
    QLabel* m_status = nullptr;
    QLabel* m_wadState = nullptr;
    QLineEdit* m_wadPath = nullptr;

    void addSection(QVBoxLayout* into, const QString& text);
    void addRow(QVBoxLayout* into, const QString& title, const QString& desc, const QString& btnText,
                std::function<void()> onClick, bool danger = false);
    void addWadRow(QVBoxLayout* into);
    void browseWad();
    void updateWadState();
    void setStatus(const QString& text, bool ok = true);
    bool confirm(const QString& title, const QString& text);
};
