#include "ChangelogDialog.h"
#include "../core/BroadcastManager.h"
#include "../core/I18n.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

ChangelogDialog::ChangelogDialog(const QString& sourceId, const QString& name, BroadcastManager* broadcast,
                                 QWidget* parent)
    : QDialog(parent), m_sourceId(sourceId), m_broadcast(broadcast) {
    setWindowTitle(T("Novità") + " — " + name);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumSize(480, 380);
    resize(520, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* head = new QVBoxLayout;
    head->setContentsMargins(24, 20, 24, 8);
    head->setSpacing(2);
    auto* title = new QLabel(T("Novità") + " — " + name);
    title->setObjectName("Heading");
    title->setWordWrap(true);
    head->addWidget(title);
    m_caption = new QLabel;
    m_caption->setObjectName("Muted");
    m_caption->setWordWrap(true);
    m_caption->hide();
    head->addWidget(m_caption);
    root->addLayout(head);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(24, 0, 24, 12);
    cl->setSpacing(8);

    auto* s1 = new QLabel(T("NOTE DELL'ULTIMA RELEASE"));
    s1->setObjectName("Section");
    cl->addWidget(s1);
    m_notes = new QVBoxLayout;
    m_notes->setSpacing(8);
    cl->addLayout(m_notes);

    auto* s2 = new QLabel(T("MODIFICHE (COMMIT)"));
    s2->setObjectName("Section");
    cl->addWidget(s2);
    m_commits = new QVBoxLayout;
    m_commits->setSpacing(4);
    cl->addLayout(m_commits);
    cl->addStretch(1);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(24, 8, 24, 16);
    footer->addStretch(1);
    auto* closeBtn = new QPushButton(T("Chiudi"));
    closeBtn->setObjectName("Secondary");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);
    root->addLayout(footer);

    if (m_broadcast) connect(m_broadcast, &BroadcastManager::changed, this, &ChangelogDialog::rebuildNotes);
    rebuildNotes();
    rebuildCommits();
}

void ChangelogDialog::setCaption(const QString& text) {
    m_caption->setText(text);
    m_caption->setVisible(!text.isEmpty());
}

void ChangelogDialog::setCommits(const QStringList& titles, int total) {
    m_commitTitles = titles;
    m_commitTotal = total;
    rebuildCommits();
}

void ChangelogDialog::clear(QVBoxLayout* l) {
    while (QLayoutItem* it = l->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }
}

void ChangelogDialog::rebuildNotes() {
    clear(m_notes);

    BroadcastMessage msg;
    if (!m_broadcast || !m_broadcast->latestUpdate(m_sourceId, &msg)) {
        auto* none = new QLabel(T("Nessuna nota di rilascio pubblicata."));
        none->setObjectName("Muted");
        none->setWordWrap(true);
        m_notes->addWidget(none);
        return;
    }

    auto* card = new QFrame;
    card->setObjectName("Row");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(6);

    auto* t = new QLabel(msg.title);
    t->setObjectName("RowTitle");
    t->setWordWrap(true);
    v->addWidget(t);

    QStringList meta;
    if (!msg.version.isEmpty()) meta << "v" + msg.version;
    if (msg.date.isValid()) meta << msg.date.toLocalTime().toString("dd/MM/yyyy");
    if (!meta.isEmpty()) {
        auto* m = new QLabel(meta.join("  ·  "));
        m->setObjectName("Muted");
        v->addWidget(m);
    }
    if (!msg.message.isEmpty()) {
        auto* b = new QLabel(msg.message);
        b->setWordWrap(true);
        b->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(b);
    }
    for (const QString& n : msg.notes) {
        auto* l = new QLabel(QString::fromUtf8("• ") + n);
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(l);
    }
    if (!msg.url.isEmpty()) {
        auto* open = new QPushButton(T("Apri"));
        open->setObjectName("Secondary");
        open->setCursor(Qt::PointingHandCursor);
        open->setFixedWidth(110);
        const QString url = msg.url;
        connect(open, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        v->addWidget(open);
    }
    m_notes->addWidget(card);
}

void ChangelogDialog::rebuildCommits() {
    clear(m_commits);
    if (m_commitTitles.isEmpty()) {
        auto* none = new QLabel(T("Elenco dei commit non disponibile."));
        none->setObjectName("Muted");
        none->setWordWrap(true);
        m_commits->addWidget(none);
        return;
    }
    for (const QString& c : m_commitTitles) {
        auto* l = new QLabel(QString::fromUtf8("• ") + c);
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_commits->addWidget(l);
    }
    if (m_commitTotal > m_commitTitles.size()) {
        auto* more = new QLabel(T("…e altri %1 commit.").arg(m_commitTotal - m_commitTitles.size()));
        more->setObjectName("Muted");
        m_commits->addWidget(more);
    }
}
