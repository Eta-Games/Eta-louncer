#pragma once
#include <QWidget>
#include "../core/AuthManager.h"

class QLabel;
class QPushButton;
class QStackedWidget;

// Sezione "Profilo", equivalente a profilo.html / login.html sul sito:
// mostra i dati dell'utente se autenticato, altrimenti un invito ad accedere.
class ProfileWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProfileWidget(AuthManager* auth, QWidget* parent = nullptr);

    void refresh(); // richiama dopo login/logout

signals:
    void goToLoginRequested();

private:
    AuthManager* m_auth;
    QStackedWidget* m_stack;
    // pagina "non autenticato"
    // pagina "autenticato"
    QLabel* m_avatarLabel;
    QLabel* m_nameLabel;
    QLabel* m_emailLabel;
    QPushButton* m_logoutBtn;
};
