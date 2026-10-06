#include "FriendsWidget.h"
#include "../core/FriendsManager.h"
#include "../core/PresenceManager.h"
#include "../core/GameCatalog.h"
#include "../core/I18n.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHash>
#include <algorithm>

FriendsWidget::FriendsWidget(FriendsManager* fm, PresenceManager* pm, QWidget* parent)
    : QWidget(parent), m_fm(fm), m_pm(pm) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    auto* addRow = new QHBoxLayout;
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(T("Nome utente dell'amico"));
    auto* addBtn = new QPushButton(T("Aggiungi"));
    addBtn->setCursor(Qt::PointingHandCursor);
    addRow->addWidget(m_edit, 1);
    addRow->addWidget(addBtn);
    root->addLayout(addRow);

    m_msg = new QLabel;
    m_msg->setObjectName("Muted");
    m_msg->setWordWrap(true);
    root->addWidget(m_msg);

    m_rows = new QVBoxLayout;
    m_rows->setSpacing(8);
    root->addLayout(m_rows);

    auto doAdd = [this]() {
        const QString name = m_edit->text();
        if (name.trimmed().isEmpty()) return;
        m_msg->setText(T("Cerco…"));
        m_fm->addByName(name, [this](const QString& err) {
            m_msg->setText(err);
            if (err.isEmpty()) m_edit->clear();
        });
    };
    connect(addBtn, &QPushButton::clicked, this, doAdd);
    connect(m_edit, &QLineEdit::returnPressed, this, doAdd);
    connect(m_fm, &FriendsManager::changed, this, &FriendsWidget::refresh);
    connect(m_pm, &PresenceManager::onlineChanged, this, &FriendsWidget::refresh);
    refresh();
}

void FriendsWidget::refresh() {
    while (QLayoutItem* it = m_rows->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }

    const auto friends = m_fm->friends();
    if (friends.isEmpty()) {
        auto* empty = new QLabel(T("Non hai ancora amici. Aggiungili con il loro nome utente."));
        empty->setObjectName("Muted");
        empty->setWordWrap(true);
        m_rows->addWidget(empty);
        return;
    }

    QHash<QString, PresenceManager::OnlineUser> online;
    for (const auto& u : m_pm->online()) online.insert(u.uid, u);

    struct Row { FriendsManager::Friend f; int rank; PresenceManager::OnlineUser u; };
    QList<Row> rows;
    for (const auto& f : friends) {
        const auto u = online.value(f.uid);
        rows.append({f, online.contains(f.uid) ? (u.status == "playing" ? 0 : 1) : 2, u});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.rank != b.rank ? a.rank < b.rank : a.f.name.compare(b.f.name, Qt::CaseInsensitive) < 0;
    });

    for (const auto& r : rows) {
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);

        auto* dot = new QLabel(r.rank == 2 ? "○" : "●");
        if (r.rank == 2) dot->setObjectName("Muted");
        else dot->setStyleSheet("color:#3fb950;");
        h->addWidget(dot);

        auto* name = new QLabel(r.f.name.isEmpty() ? r.f.uid : r.f.name);
        name->setTextFormat(Qt::PlainText);
        h->addWidget(name, 1);

        QString status = T("Offline");
        if (r.rank == 1) status = T("Online");
        if (r.rank == 0) {
            const GameEntry* g = findGame(r.u.gameId);
            status = T("In gioco: %1").arg(g ? g->title : r.u.gameId);
        }
        auto* st = new QLabel(status);
        st->setTextFormat(Qt::PlainText);
        if (r.rank == 2) st->setObjectName("Muted");
        h->addWidget(st);

        auto* rm = new QPushButton(T("Rimuovi"));
        rm->setFlat(true);
        rm->setCursor(Qt::PointingHandCursor);
        const QString uid = r.f.uid;
        connect(rm, &QPushButton::clicked, this, [this, uid]() {
            m_fm->remove(uid, [this](const QString& err) { m_msg->setText(err); });
        });
        h->addWidget(rm);

        m_rows->addWidget(row);
    }
}
