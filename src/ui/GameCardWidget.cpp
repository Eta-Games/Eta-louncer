#include "../core/PlayStats.h"
#include "GameCardWidget.h"
#include "CoverUtil.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include "../core/I18n.h"
#include <QPainter>
#include <QPainterPath>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

GameCardWidget::GameCardWidget(const GameEntry& game, QWidget* parent)
    : QFrame(parent), m_game(game) {
    setObjectName("Card");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Copertina a tutta larghezza in alto (come le card del sito)
    m_cover = new QLabel;
    m_cover->setObjectName("Cover");
    m_cover->setFixedHeight(150);
    m_cover->setAlignment(Qt::AlignCenter);
    m_cover->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    root->addWidget(m_cover);

    auto* body = new QWidget;
    body->setObjectName("CardBody");
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(16, 14, 16, 16);
    bl->setSpacing(6);
    root->addWidget(body);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(game.title);
    title->setObjectName("CardTitle");
    header->addWidget(title);
    header->addStretch();
    m_installedTag = new QLabel(T("Installato"));
    m_installedTag->setObjectName("Tag");
    m_installedTag->hide();
    header->addWidget(m_installedTag);
    header->addSpacing(6);
    m_updateBadge = new QLabel;
    m_updateBadge->setObjectName("Badge");
    m_updateBadge->hide();
    header->addWidget(m_updateBadge);
    bl->addLayout(header);

    auto* desc = new QLabel(game.desc);
    desc->setObjectName("Muted");
    desc->setWordWrap(true);
    bl->addWidget(desc);

    auto* meta = new QLabel(QString("%1 · %2 · v%3").arg(game.engine, game.size, game.version));
    meta->setObjectName("Muted");
    bl->addWidget(meta);

    m_playLabel = new QLabel;
    m_playLabel->setObjectName("Muted");
    m_playLabel->hide();
    bl->addWidget(m_playLabel);
    connect(&PlayStats::instance(), &PlayStats::changed, this, &GameCardWidget::refreshPlaytime);

    m_onlineLabel = new QLabel;
    m_onlineLabel->setObjectName("Online");
    m_onlineLabel->hide();
    bl->addWidget(m_onlineLabel);

    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("Muted");
    m_statusLabel->hide();
    bl->addWidget(m_statusLabel);

    bl->addSpacing(6);
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    m_actionBtn = new QPushButton(T("Installa"));
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
        if (m_installed) emit launchRequested(m_game.id);
        else emit installRequested(m_game);
    });
    btnRow->addWidget(m_actionBtn, 1);

    m_manageBtn = new QPushButton(T("Gestisci"));
    m_manageBtn->setObjectName("Secondary");
    m_manageBtn->setCursor(Qt::PointingHandCursor);
    m_manageBtn->setVisible(false);
    connect(m_manageBtn, &QPushButton::clicked, this, [this]() { emit manageRequested(m_game.id); });
    btnRow->addWidget(m_manageBtn);

    m_checkBtn = new QPushButton(QString::fromUtf8("⟳"));
    m_checkBtn->setObjectName("Secondary");
    m_checkBtn->setCursor(Qt::PointingHandCursor);
    m_checkBtn->setFixedWidth(40);
    m_checkBtn->setToolTip(T("Controlla aggiornamenti"));
    m_checkBtn->setVisible(false);
    connect(m_checkBtn, &QPushButton::clicked, this, [this]() { emit updateCheckRequested(m_game.id); });
    btnRow->addWidget(m_checkBtn);

    m_forceBtn = new QPushButton(QString::fromUtf8("⬇"));
    m_forceBtn->setObjectName("Secondary");
    m_forceBtn->setCursor(Qt::PointingHandCursor);
    m_forceBtn->setFixedWidth(40);
    m_forceBtn->setToolTip(T("Aggiorna forzatamente"));
    m_forceBtn->setVisible(false);
    connect(m_forceBtn, &QPushButton::clicked, this, [this]() { emit forceUpdateRequested(m_game.id); });
    btnRow->addWidget(m_forceBtn);

    auto* reviewsBtn = new QPushButton(QString(QChar(0x2605)));
    reviewsBtn->setObjectName("Secondary");
    reviewsBtn->setCursor(Qt::PointingHandCursor);
    reviewsBtn->setFixedWidth(40);
    reviewsBtn->setToolTip(T("Recensioni e voti"));
    connect(reviewsBtn, &QPushButton::clicked, this, [this]() { emit reviewsRequested(m_game.id); });
    btnRow->addWidget(reviewsBtn);

    bl->addLayout(btnRow);

    // Scarica la copertina (es. TITLEPIC.png) in modo asincrono
    if (!game.imgUrl.isEmpty()) {
        auto* nam = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(game.imgUrl)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            QPixmap pix;
            if (reply->error() == QNetworkReply::NoError && pix.loadFromData(reply->readAll())) {
                m_coverPix = pix;
                updateCover();
            }
            reply->deleteLater();
        });
    }
}

void GameCardWidget::resizeEvent(QResizeEvent* e) {
    QFrame::resizeEvent(e);
    updateCover();
}

// Ritaglia/scala la copertina sulla larghezza attuale della card, con gli angoli superiori arrotondati
void GameCardWidget::updateCover() {
    if (m_coverPix.isNull()) return;
    m_cover->setPixmap(makeRoundedCover(m_coverPix, qMax(1, m_cover->width()), m_cover->height(), 11));
}

void GameCardWidget::setInstalled(bool installed) {
    m_installed = installed;
    m_actionBtn->setText(installed ? T("Avvia") : T("Installa"));
    m_manageBtn->setVisible(installed);
    m_checkBtn->setVisible(installed);
    m_forceBtn->setVisible(installed);
    m_installedTag->setVisible(installed);
    refreshPlaytime();
}

void GameCardWidget::setOnlinePlayers(int playing, const QString& namesTooltip) {
    m_onlineLabel->setVisible(playing > 0);
    m_onlineLabel->setText(playing == 1 ? "● 1 giocatore in partita" : QString("● %1 giocatori in partita").arg(playing));
    m_onlineLabel->setToolTip(namesTooltip);
}

void GameCardWidget::setBusy(bool busy, const QString& label) {
    m_actionBtn->setEnabled(!busy);
    m_statusLabel->setVisible(busy && !label.isEmpty());
    if (busy) m_statusLabel->setText(label);
}

void GameCardWidget::setChecking(bool checking) {
    m_checkBtn->setEnabled(!checking);
    m_checkBtn->setText(checking ? QString::fromUtf8("…") : QString::fromUtf8("⟳"));
    m_checkBtn->setToolTip(checking ? T("Controllo aggiornamenti") + QString::fromUtf8("…") : T("Controlla aggiornamenti"));
}

void GameCardWidget::setUpdateAvailable(bool available, const QString& versionLabel) {
    m_updateBadge->setVisible(available);
    if (available) m_updateBadge->setText(T("Aggiornamento ") + versionLabel);
}

void GameCardWidget::refreshPlaytime() {
    if (!m_playLabel) return;
    const GameStats s = PlayStats::instance().get(m_game.id);
    const bool show = m_installed && s.sessions > 0;
    if (show)
        m_playLabel->setText(T("Giocato %1 · ultima partita %2")
            .arg(PlayStats::formatDuration(s.totalSecs), PlayStats::formatLast(s.lastPlayed)));
    m_playLabel->setVisible(show);
}
