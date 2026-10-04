#include "LoginWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>

LoginWidget::LoginWidget(AuthManager* auth, QWidget* parent)
    : QWidget(parent), m_auth(auth) {

    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);
    root->setSpacing(14);

    auto* logo = new QLabel;
    QPixmap pix("logo.png");
    if (!pix.isNull()) logo->setPixmap(pix.scaledToWidth(96, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    auto* title = new QLabel("Accesso riservato");
    title->setObjectName("Heading");
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* subtitle = new QLabel("Accedi per i contenuti esclusivi ETA Games e gli update dei giochi.");
    subtitle->setObjectName("Muted");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);
    subtitle->setFixedWidth(300);
    root->addWidget(subtitle, 0, Qt::AlignCenter);

    m_email = new QLineEdit; m_email->setPlaceholderText("Email");
    m_email->setFixedWidth(280);
    m_pass = new QLineEdit; m_pass->setPlaceholderText("Password");
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setFixedWidth(280);
    root->addWidget(m_email, 0, Qt::AlignCenter);
    root->addWidget(m_pass, 0, Qt::AlignCenter);

    m_error = new QLabel;
    m_error->setStyleSheet("color: #ff5566;");
    m_error->setAlignment(Qt::AlignCenter);
    m_error->hide();
    root->addWidget(m_error);

    m_loginBtn = new QPushButton("Accedi");
    m_loginBtn->setFixedWidth(280);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::doLogin);
    root->addWidget(m_loginBtn, 0, Qt::AlignCenter);

    m_googleBtn = new QPushButton("Accedi con Google");
    m_googleBtn->setObjectName("Secondary");
    m_googleBtn->setFixedWidth(280);
    connect(m_googleBtn, &QPushButton::clicked, this, [this]() {
        m_googleBtn->setEnabled(false);
        m_googleBtn->setText("Apertura browser…");
        m_auth->startGoogleLogin();
        // Il risultato arriva in modo asincrono via deep link (etagames://auth?...),
        // intercettato da MainWindow/main.cpp e inoltrato ad AuthManager.
    });
    root->addWidget(m_googleBtn, 0, Qt::AlignCenter);

    auto* skipBtn = new QPushButton("Continua senza account");
    skipBtn->setObjectName("Secondary");
    skipBtn->setFixedWidth(280);
    connect(skipBtn, &QPushButton::clicked, this, [this]() { emit skipped(); });
    root->addWidget(skipBtn, 0, Qt::AlignCenter);

    connect(m_auth, &AuthManager::loginSucceeded, this, [this](AuthUser u) {
        m_loginBtn->setEnabled(true);
        m_googleBtn->setEnabled(true);
        m_googleBtn->setText("Accedi con Google");
        emit loggedIn(u);
    });
    connect(m_auth, &AuthManager::loginFailed, this, [this](const QString& msg) {
        m_loginBtn->setEnabled(true);
        m_googleBtn->setEnabled(true);
        m_googleBtn->setText("Accedi con Google");
        showError(msg);
    });
}

void LoginWidget::doLogin() {
    m_error->hide();
    QString email = m_email->text().trimmed();
    QString pass = m_pass->text();
    if (email.isEmpty() || pass.isEmpty()) { showError("Inserisci email e password."); return; }
    m_loginBtn->setEnabled(false);
    m_loginBtn->setText("Accesso in corso…");
    m_auth->signInWithPassword(email, pass);
}

void LoginWidget::showError(const QString& msg) {
    m_error->setText(msg);
    m_error->show();
    m_loginBtn->setText("Accedi");
}
