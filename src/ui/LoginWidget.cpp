#include "LoginWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>

LoginWidget::LoginWidget(AuthManager* auth, QWidget* parent)
    : QWidget(parent), m_auth(auth) {

    auto* outer = new QVBoxLayout(this);
    outer->setAlignment(Qt::AlignCenter);

    // Pannello centrale in stile card del sito
    auto* panel = new QFrame;
    panel->setObjectName("Panel");
    panel->setFixedWidth(380);
    outer->addWidget(panel, 0, Qt::AlignCenter);

    auto* root = new QVBoxLayout(panel);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(12);

    auto* logo = new QLabel;
    QPixmap pix(":/logo.png");
    if (!pix.isNull()) logo->setPixmap(pix.scaledToWidth(88, Qt::SmoothTransformation));
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
    root->addWidget(subtitle);
    root->addSpacing(6);

    m_email = new QLineEdit; m_email->setPlaceholderText("Email");
    m_pass = new QLineEdit; m_pass->setPlaceholderText("Password");
    m_pass->setEchoMode(QLineEdit::Password);
    connect(m_pass, &QLineEdit::returnPressed, this, &LoginWidget::doLogin);
    root->addWidget(m_email);
    root->addWidget(m_pass);

    m_error = new QLabel;
    m_error->setStyleSheet("color: #ff5566;");
    m_error->setAlignment(Qt::AlignCenter);
    m_error->setWordWrap(true);
    m_error->hide();
    root->addWidget(m_error);

    m_loginBtn = new QPushButton("Accedi");
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::doLogin);
    root->addWidget(m_loginBtn);

    m_googleBtn = new QPushButton("Accedi con Google");
    m_googleBtn->setObjectName("Secondary");
    m_googleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_googleBtn, &QPushButton::clicked, this, [this]() {
        m_googleBtn->setEnabled(false);
        m_googleBtn->setText("Apertura browser…");
        m_auth->startGoogleLogin();
        // Il risultato arriva in modo asincrono via deep link (etagames://auth?...),
        // intercettato da MainWindow/main.cpp e inoltrato ad AuthManager.
    });
    root->addWidget(m_googleBtn);

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
