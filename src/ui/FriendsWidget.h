#pragma once
#include <QWidget>

class FriendsManager;
class PresenceManager;
class QLineEdit;
class QLabel;
class QVBoxLayout;

// Sezione "Amici" del profilo: aggiungi per nome utente, vedi chi è online e a cosa sta giocando.
class FriendsWidget : public QWidget {
    Q_OBJECT
public:
    FriendsWidget(FriendsManager* friends, PresenceManager* presence, QWidget* parent = nullptr);
    void refresh();

private:
    FriendsManager* m_fm;
    PresenceManager* m_pm;
    QLineEdit* m_edit = nullptr;
    QLabel* m_msg = nullptr;
    QVBoxLayout* m_rows = nullptr;
};
