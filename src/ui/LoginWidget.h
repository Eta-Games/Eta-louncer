#pragma once
#include <QWidget>
#include "../core/AuthManager.h"

class QLineEdit;
class QLabel;
class QPushButton;

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(AuthManager* auth, QWidget* parent = nullptr);

signals:
    void skipped();      // "continua senza account"
    void loggedIn(AuthUser user);

private:
    AuthManager* m_auth;
    QLineEdit* m_email;
    QLineEdit* m_pass;
    QLabel* m_error;
    QPushButton* m_loginBtn;
    QPushButton* m_googleBtn;

    void doLogin();
    void showError(const QString& msg);
};
