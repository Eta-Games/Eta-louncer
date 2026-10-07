#include "BroadcastWidget.h"
#include "../core/I18n.h"
#include "../core/BroadcastManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QVector>

namespace {

// Un colore per ramo (stabile: dipende dalla posizione della sorgente, non dal filtro)
const QList<QColor>& laneColors() {
    static const QList<QColor> c = {QColor("#4ea1ff"), QColor("#3ddc84"), QColor("#ffb020"),
                                    QColor("#c678dd"), QColor("#ff6b6b"), QColor("#2bd9c4")};
    return c;
}

struct GraphInfo {
    int lanes = 1;          // quante corsie (rami) ci sono nel grafico
    int lane = 0;           // corsia del messaggio di questa riga
    int row = 0, rows = 0;  // riga corrente / totale
    bool trunk = false;     // la corsia 0 è il "main" (launcher) e prosegue fino in fondo
    bool unread = false;
    QVector<int> first, last;   // per ogni corsia: prima e ultima riga con un messaggio
    QVector<QColor> colors;     // colore di ogni corsia
};

// La colonna a sinistra di ogni messaggio: linee dei rami, pallino e curva di "fork" verso il main
class GraphCell : public QWidget {
public:
    GraphCell(const GraphInfo& g, QWidget* parent = nullptr) : QWidget(parent), m_g(g) {
        setFixedWidth(16 * g.lanes + 14);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double h = height();
        const double cy = 26; // altezza del pallino: in linea con il titolo del messaggio
        auto lx = [](int lane) { return 10.0 + lane * 16.0; };

        // 1) linee verticali dei rami
        for (int l = 0; l < m_g.lanes; ++l) {
            const bool trunk = (l == 0 && m_g.trunk);
            const int from = trunk ? 0 : m_g.first[l];
            const int to = trunk ? m_g.rows - 1 : m_g.last[l];
            if (m_g.row < from || m_g.row > to) continue;
            const double top = (!trunk && m_g.row == m_g.first[l]) ? cy : 0;
            const double bottom = (!trunk && m_g.row == m_g.last[l]) ? cy : h;
            p.setPen(QPen(m_g.colors[l], 2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(lx(l), top), QPointF(lx(l), bottom));
        }

        // 2) il ramo di un gioco "nasce" dal main: curva dal suo ultimo messaggio verso la corsia 0
        if (m_g.trunk && m_g.lane > 0 && m_g.row == m_g.last[m_g.lane]) {
            QPainterPath path(QPointF(lx(m_g.lane), cy));
            path.cubicTo(lx(m_g.lane), cy + (h - cy) * 0.7, lx(0), cy + (h - cy) * 0.3, lx(0), h);
            p.setPen(QPen(m_g.colors[m_g.lane], 2, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }

        // 3) il "commit": pieno se non letto, solo contorno se già letto
        const QColor c = m_g.colors[m_g.lane];
        const QPointF centre(lx(m_g.lane), cy);
        if (m_g.unread) {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawEllipse(centre, 6.5, 6.5);
        } else {
            p.setPen(QPen(c, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(centre, 5, 5);
        }
    }

private:
    GraphInfo m_g;
};

} // namespace

BroadcastWidget::BroadcastWidget(BroadcastManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 24, 32, 0);
    outer->setSpacing(4);

    auto* top = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto* heading = new QLabel("Broadcast");
    heading->setObjectName("Heading");
    titles->addWidget(heading);
    auto* sub = new QLabel(T("Novità e avvisi da ETA Launcher e dai giochi. Scegli il ramo da vedere."));
    sub->setObjectName("Muted");
    titles->addWidget(sub);
    top->addLayout(titles, 1);

    auto* refreshBtn = new QPushButton(T("Aggiorna"));
    refreshBtn->setObjectName("Secondary");
    refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(refreshBtn, &QPushButton::clicked, m_manager, &BroadcastManager::refresh);
    top->addWidget(refreshBtn, 0, Qt::AlignBottom);

    auto* readBtn = new QPushButton(T("Segna come letto"));
    readBtn->setObjectName("Secondary");
    readBtn->setCursor(Qt::PointingHandCursor);
    connect(readBtn, &QPushButton::clicked, this, &BroadcastWidget::markVisibleRead);
    top->addWidget(readBtn, 0, Qt::AlignBottom);
    outer->addLayout(top);

    // Selettore dei rami (come i "branch" di GitHub)
    m_chips = new QHBoxLayout;
    m_chips->setContentsMargins(0, 12, 0, 0);
    m_chips->setSpacing(8);
    outer->addLayout(m_chips);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget;
    m_list = new QVBoxLayout(host);
    m_list->setContentsMargins(0, 12, 0, 24);
    m_list->setSpacing(0); // le linee dei rami devono essere continue tra una riga e l'altra
    scroll->setWidget(host);
    outer->addWidget(scroll, 1);

    m_filter = QSettings().value("broadcast/filter").toString();
    buildChips();

    connect(m_manager, &BroadcastManager::changed, this, &BroadcastWidget::rebuild);
    rebuild();
}

QColor BroadcastWidget::colorForSource(const QString& sourceId) const {
    const auto sources = m_manager->sources();
    for (int i = 0; i < sources.size(); ++i)
        if (sources[i].id == sourceId) return laneColors()[i % laneColors().size()];
    return laneColors().first();
}

void BroadcastWidget::buildChips() {
    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    auto addChip = [&](const QString& id, const QColor& color) {
        auto* b = new QPushButton;
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("sourceId", id);
        b->setStyleSheet(QString(
            "QPushButton { border: 1px solid %1; border-radius: 13px; padding: 4px 14px; background: transparent; }"
            "QPushButton:hover { background-color: rgba(%2,%3,%4,0.15); }"
            "QPushButton:checked { background-color: rgba(%2,%3,%4,0.28); font-weight: 600; }")
            .arg(color.name()).arg(color.red()).arg(color.green()).arg(color.blue()));
        m_group->addButton(b);
        m_chips->addWidget(b);
        m_chipButtons << b;
        connect(b, &QPushButton::clicked, this, [this, id]() {
            m_filter = id;
            QSettings().setValue("broadcast/filter", m_filter);
            rebuild();
        });
        if (id == m_filter) b->setChecked(true);
    };

    addChip(QString(), QColor("#9a9a9a")); // T("Tutto")
    const auto sources = m_manager->sources();
    for (int i = 0; i < sources.size(); ++i) addChip(sources[i].id, laneColors()[i % laneColors().size()]);
    m_chips->addStretch(1);

    // il filtro salvato non esiste più (gioco tolto dal catalogo): si torna a "Tutto"
    if (!m_group->checkedButton()) { m_filter.clear(); m_chipButtons.first()->setChecked(true); }
    updateChipLabels();
}

void BroadcastWidget::updateChipLabels() {
    const auto sources = m_manager->sources();
    for (auto* b : m_chipButtons) {
        const QString id = b->property("sourceId").toString();
        QString name = T("Tutto");
        int unread = m_manager->unreadCount();
        if (!id.isEmpty()) {
            unread = m_manager->unreadCount(id);
            for (const auto& s : sources) if (s.id == id) name = s.name;
        }
        b->setText(unread > 0 ? QString("%1  (%2)").arg(name).arg(unread) : name);
    }
}

void BroadcastWidget::markVisibleRead() {
    m_manager->markRead(m_filter); // vuoto = tutti
}

void BroadcastWidget::rebuild() {
    updateChipLabels();

    while (QLayoutItem* it = m_list->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }

    // messaggi del ramo scelto
    QList<BroadcastMessage> msgs;
    for (const auto& m : m_manager->messages())
        if (m_filter.isEmpty() || m.sourceId == m_filter) msgs << m;

    if (msgs.isEmpty()) {
        auto* empty = new QLabel(m_filter.isEmpty() ? T("Nessun messaggio al momento.")
                                                    : T("Nessun messaggio su questo ramo."));
        empty->setObjectName("Muted");
        empty->setAlignment(Qt::AlignCenter);
        m_list->addSpacing(24);
        m_list->addWidget(empty);
        m_list->addStretch(1);
        return;
    }

    // corsie: una per ogni ramo che ha almeno un messaggio, nell'ordine delle sorgenti (launcher = main)
    QList<QString> laneIds;
    for (const auto& s : m_manager->sources())
        for (const auto& m : msgs) if (m.sourceId == s.id) { laneIds << s.id; break; }

    GraphInfo base;
    base.lanes = laneIds.size();
    base.rows = msgs.size();
    base.trunk = (!laneIds.isEmpty() && laneIds.first() == "launcher" && base.lanes > 1);
    base.first = QVector<int>(base.lanes, -1);
    base.last = QVector<int>(base.lanes, -1);
    for (const auto& id : laneIds) base.colors << colorForSource(id);
    for (int row = 0; row < msgs.size(); ++row) {
        const int l = laneIds.indexOf(msgs[row].sourceId);
        if (base.first[l] < 0) base.first[l] = row;
        base.last[l] = row;
    }

    for (int row = 0; row < msgs.size(); ++row) {
        const auto& m = msgs[row];
        GraphInfo g = base;
        g.row = row;
        g.lane = laneIds.indexOf(m.sourceId);
        g.unread = !m.read;

        auto* rowW = new QWidget;
        auto* h = new QHBoxLayout(rowW);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        h->addWidget(new GraphCell(g));

        auto* cardHost = new QWidget;
        auto* hostLayout = new QVBoxLayout(cardHost);
        hostLayout->setContentsMargins(0, 6, 0, 6);

        auto* card = new QFrame;
        card->setObjectName("BcCard");
        card->setProperty("level", m.level);
        card->setProperty("unread", !m.read);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(18, 14, 18, 14);
        v->setSpacing(6);

        auto* head = new QHBoxLayout;
        head->setSpacing(8);
        auto* title = new QLabel(m.title.isEmpty() ? m.sourceName : m.title);
        title->setObjectName("RowTitle");
        title->setWordWrap(true);
        head->addWidget(title, 1);

        // etichetta del ramo, colorata come la sua corsia
        const QColor c = base.colors[g.lane];
        auto* branch = new QLabel(m.sourceName);
        branch->setStyleSheet(QString("QLabel { color: %1; border: 1px solid %1; border-radius: 9px;"
                                      " padding: 1px 9px; font-weight: 600; }").arg(c.name()));
        head->addWidget(branch);
        if (!m.read) {
            auto* fresh = new QLabel(T("Nuovo"));
            fresh->setObjectName("Online");
            head->addWidget(fresh);
        }
        v->addLayout(head);

        QStringList meta;
        if (m.date.isValid()) meta << m.date.toLocalTime().toString("dd/MM/yyyy");
        if (!m.version.isEmpty()) meta << "v" + m.version;
        if (!meta.isEmpty()) {
            auto* date = new QLabel(meta.join("  ·  "));
            date->setObjectName("Muted");
            v->addWidget(date);
        }
        auto* body = new QLabel(m.message);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(body);
        for (const QString& n : m.notes) {
            auto* note = new QLabel(QString::fromUtf8("• ") + n);
            note->setWordWrap(true);
            note->setTextInteractionFlags(Qt::TextSelectableByMouse);
            v->addWidget(note);
        }

        if (!m.url.isEmpty()) {
            auto* open = new QPushButton("Apri");
            open->setObjectName("Secondary");
            open->setCursor(Qt::PointingHandCursor);
            open->setFixedWidth(110);
            const QString url = m.url;
            connect(open, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            v->addWidget(open);
        }
        hostLayout->addWidget(card);
        h->addWidget(cardHost, 1);
        m_list->addWidget(rowW);
    }
    m_list->addStretch(1);
}
