#include "ProfileWidget.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>

ProfileWidget::ProfileWidget(AuthManager* auth, QWidget* parent)
    : QWidget(parent), m_auth(auth) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);

    m_stack = new QStackedWidget;
    root->addWidget(m_stack);

    // ── Pagina: non autenticato ──────────────────────────────────────────
    auto* guestPage = new QWidget;
    auto* guestLayout = new QVBoxLayout(guestPage);
    guestLayout->setAlignment(Qt::AlignCenter);
    guestLayout->setSpacing(14);

    auto* guestTitle = new QLabel("Accesso riservato");
    guestTitle->setObjectName("Heading");
    guestTitle->setAlignment(Qt::AlignCenter);
    guestLayout->addWidget(guestTitle);

    auto* guestDesc = new QLabel("Effettua il login per vedere il tuo profilo ETA Games e gli update dei giochi.");
    guestDesc->setObjectName("Muted");
    guestDesc->setAlignment(Qt::AlignCenter);
    guestDesc->setWordWrap(true);
    guestLayout->addWidget(guestDesc);

    auto* goLoginBtn = new QPushButton("Vai al login");
    goLoginBtn->setFixedWidth(220);
    connect(goLoginBtn, &QPushButton::clicked, this, [this]() { emit goToLoginRequested(); });
    guestLayout->addWidget(goLoginBtn, 0, Qt::AlignCenter);

    m_stack->addWidget(guestPage);

    // ── Pagina: autenticato ───────────────────────────────────────────────
    auto* userPage = new QWidget;
    auto* userLayout = new QVBoxLayout(userPage);
    userLayout->setAlignment(Qt::AlignCenter);
    userLayout->setSpacing(10);

    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(84, 84);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setObjectName("Avatar");
    userLayout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);

    m_nameLabel = new QLabel;
    m_nameLabel->setObjectName("Heading");
    m_nameLabel->setAlignment(Qt::AlignCenter);
    userLayout->addWidget(m_nameLabel);

    m_emailLabel = new QLabel;
    m_emailLabel->setObjectName("Muted");
    m_emailLabel->setAlignment(Qt::AlignCenter);
    userLayout->addWidget(m_emailLabel);

    m_logoutBtn = new QPushButton("Esci");
    m_logoutBtn->setObjectName("Secondary");
    m_logoutBtn->setFixedWidth(160);
    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        m_auth->signOut();
        refresh();
    });
    userLayout->addWidget(m_logoutBtn, 0, Qt::AlignCenter);

    m_stack->addWidget(userPage);

    connect(m_auth, &AuthManager::loginSucceeded, this, [this](AuthUser) { refresh(); });

    refresh();
}

void ProfileWidget::refresh() {
    AuthUser u = m_auth->currentUser();
    if (!u.isValid()) {
        m_stack->setCurrentIndex(0);
        return;
    }
    QString display = u.displayName.isEmpty() ? u.email : u.displayName;
    m_nameLabel->setText(display.isEmpty() ? "Utente ETA Games" : display);
    m_emailLabel->setText(u.email);
    QString initial = display.isEmpty() ? "?" : display.left(1).toUpper();
    m_avatarLabel->setText(initial);
    m_stack->setCurrentIndex(1);
}
