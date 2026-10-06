#pragma once
#include <QWidget>
#include "../core/AuthManager.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(AuthManager* auth, QWidget* parent = nullptr);

    // true mentre si ripristina la sessione salvata (campi bloccati + messaggio)
    void setRestoring(bool on);

signals:
    void skipped();      // "continua senza account"
    void loggedIn(AuthUser user);

private:
    AuthManager* m_auth;
    QLineEdit* m_email;
    QLineEdit* m_pass;
    QLabel* m_error;
    QLabel* m_status;
    QCheckBox* m_remember;
    QPushButton* m_loginBtn;
    QPushButton* m_googleBtn;
    bool m_restoring = false;

    void doLogin();
    void showError(const QString& msg);
    void resetButtons();
};
