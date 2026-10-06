#include "StatsWidget.h"
#include "../core/PlayStats.h"
#include "../core/GameCatalog.h"
#include "../core/I18n.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

StatsWidget::StatsWidget(QWidget* parent) : QFrame(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    m_rows = new QVBoxLayout;
    m_rows->setSpacing(8);
    root->addLayout(m_rows);

    connect(&PlayStats::instance(), &PlayStats::changed, this, &StatsWidget::refresh);
    refresh();
}

void StatsWidget::refresh() {
    while (QLayoutItem* it = m_rows->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }

    const auto ranking = PlayStats::instance().ranking();
    if (ranking.isEmpty()) {
        auto* empty = new QLabel(T("Nessuna partita registrata ancora. Avvia un gioco dalla libreria!"));
        empty->setObjectName("Muted");
        empty->setWordWrap(true);
        m_rows->addWidget(empty);
        return;
    }

    qint64 total = 0;
    int pos = 1;
    for (const auto& [id, st] : ranking) {
        total += st.totalSecs;
        const GameEntry* g = findGame(id);

        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(12);

        auto* rank = new QLabel(QString("#%1").arg(pos++));
        rank->setFixedWidth(32);
        h->addWidget(rank);

        auto* mid = new QVBoxLayout;
        mid->setSpacing(0);
        mid->addWidget(new QLabel(g ? g->title : id));
        auto* sub = new QLabel(T("Ultima partita: %1 · %2 %3")
            .arg(PlayStats::formatLast(st.lastPlayed))
            .arg(st.sessions)
            .arg(st.sessions == 1 ? T("partita") : T("partite")));
        sub->setObjectName("Muted");
        mid->addWidget(sub);
        h->addLayout(mid, 1);

        h->addWidget(new QLabel(PlayStats::formatDuration(st.totalSecs)));
        m_rows->addWidget(row);
    }

    auto* totalLbl = new QLabel(T("Tempo totale: ") + PlayStats::formatDuration(total));
    totalLbl->setObjectName("Muted");
    m_rows->addWidget(totalLbl);
}
