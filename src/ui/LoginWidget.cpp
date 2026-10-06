#include "LoginWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QCheckBox>
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
    m_email->setText(m_auth->lastEmail()); // ultimo account usato
    m_pass = new QLineEdit; m_pass->setPlaceholderText("Password");
    m_pass->setEchoMode(QLineEdit::Password);
    connect(m_pass, &QLineEdit::returnPressed, this, &LoginWidget::doLogin);
    root->addWidget(m_email);
    root->addWidget(m_pass);

    m_remember = new QCheckBox("Ricordami su questo PC");
    m_remember->setChecked(m_auth->rememberSession());
    m_remember->setCursor(Qt::PointingHandCursor);
    m_remember->setToolTip("La password non viene salvata: si conserva solo un token di accesso\n"
                           "(nel Gestore credenziali di Windows). Esci dal profilo per cancellarlo.");
    root->addWidget(m_remember);

    m_status = new QLabel;
    m_status->setObjectName("Muted");
    m_status->setAlignment(Qt::AlignCenter);
    m_status->hide();
    root->addWidget(m_status);

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
        m_auth->setRememberSession(m_remember->isChecked());
        m_auth->startGoogleLogin();
        // Il risultato arriva in modo asincrono via deep link (etagames://auth?...),
        // intercettato da MainWindow/main.cpp e inoltrato ad AuthManager.
    });
    root->addWidget(m_googleBtn);

    connect(m_auth, &AuthManager::loginSucceeded, this, [this](AuthUser u) {
        m_restoring = false;
        m_status->hide();
        m_pass->clear();
        resetButtons();
        emit loggedIn(u);
    });
    connect(m_auth, &AuthManager::loginFailed, this, [this](const QString& msg) {
        resetButtons();
        showError(msg);
    });
    connect(m_auth, &AuthManager::sessionRestoreFailed, this, [this](const QString& msg) {
        setRestoring(false);
        showError(msg);
    });
}

void LoginWidget::setRestoring(bool on) {
    m_restoring = on;
    m_status->setText("Ripristino della sessione…");
    m_status->setVisible(on);
    if (on) m_error->hide();
    for (QWidget* w : {static_cast<QWidget*>(m_email), static_cast<QWidget*>(m_pass), static_cast<QWidget*>(m_remember),
                       static_cast<QWidget*>(m_loginBtn), static_cast<QWidget*>(m_googleBtn)})
        w->setEnabled(!on);
}

void LoginWidget::resetButtons() {
    m_loginBtn->setEnabled(true);
    m_loginBtn->setText("Accedi");
    m_googleBtn->setEnabled(true);
    m_googleBtn->setText("Accedi con Google");
}

void LoginWidget::doLogin() {
    if (m_restoring) return;
    m_error->hide();
    QString email = m_email->text().trimmed();
    QString pass = m_pass->text();
    if (email.isEmpty() || pass.isEmpty()) { showError("Inserisci email e password."); return; }
    m_loginBtn->setEnabled(false);
    m_loginBtn->setText("Accesso in corso…");
    m_auth->setRememberSession(m_remember->isChecked());
    m_auth->signInWithPassword(email, pass);
}

void LoginWidget::showError(const QString& msg) {
    m_error->setText(msg);
    m_error->show();
    m_loginBtn->setText("Accedi");
}
