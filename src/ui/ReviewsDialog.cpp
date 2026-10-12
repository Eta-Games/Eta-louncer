#include "ReviewsDialog.h"
#include "../core/I18n.h"
#include "../core/AuthManager.h"
#include "../core/FirestoreClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QScrollArea>
#include <QFrame>
#include <QDateTime>
#include <QPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QGuiApplication>
#include <algorithm>

namespace {

const QColor kUpColor("#66c0f4");    // blu Steam
const QColor kDownColor("#c15755");  // rosso/arancio Steam
const QColor kIdleColor("#8f98a0");  // grigio

// Pollice vettoriale (Material Design, Apache 2.0). up=false -> ruotato di 180°.
QPixmap thumbPixmap(bool up, const QColor& color, int px) {
    // Griglia 24x24: polsino + mano (stessa forma del pollice Material, tracciata a mano).
    QPainterPath path;
    path.addRect(1, 9, 4, 12);
    path.moveTo(23, 10);
    path.cubicTo(23, 8.9, 22.1, 8, 21, 8);
    path.lineTo(14.69, 8);
    path.lineTo(15.64, 3.43);
    path.lineTo(15.67, 3.11);
    path.cubicTo(15.67, 2.70, 15.50, 2.32, 15.23, 2.05);
    path.lineTo(14.17, 1);
    path.lineTo(7.59, 7.59);
    path.cubicTo(7.22, 7.95, 7, 8.45, 7, 9);
    path.lineTo(7, 19);
    path.cubicTo(7, 20.1, 7.9, 21, 9, 21);
    path.lineTo(18, 21);
    path.cubicTo(18.83, 21, 19.54, 20.5, 19.84, 19.78);
    path.lineTo(22.86, 12.73);
    path.cubicTo(22.95, 12.50, 23, 12.26, 23, 12);
    path.closeSubpath();

    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(int(px * dpr), int(px * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(px / 24.0, px / 24.0);
    if (!up) { p.translate(12, 12); p.rotate(180); p.translate(-12, -12); }
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);
    return pm;
}

// Icona con stato spento (grigio) e acceso (colorato) per i pulsanti checkable.
QIcon thumbIcon(bool up, int px) {
    QIcon ic;
    ic.addPixmap(thumbPixmap(up, kIdleColor, px), QIcon::Normal, QIcon::Off);
    ic.addPixmap(thumbPixmap(up, up ? kUpColor : kDownColor, px), QIcon::Normal, QIcon::On);
    return ic;
}

QPushButton* makeVoteButton(const QString& text, bool up) {
    auto* b = new QPushButton(text);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setIcon(thumbIcon(up, 24));
    b->setIconSize(QSize(24, 24));
    const QString c = (up ? kUpColor : kDownColor).name();
    const QString bg = up ? "rgba(102,192,244,0.15)" : "rgba(193,87,85,0.15)";
    b->setStyleSheet(QString(
        "QPushButton{padding:10px 18px;border:1px solid #3d4450;border-radius:8px;font-weight:600;"
        "background:#1b1b1b;color:#c6d4df;}"
        "QPushButton:hover{border-color:%1;background:#242424;}"
        "QPushButton:checked,QPushButton:checked:hover{background:%2;border-color:%1;color:%1;}").arg(c, bg));
    return b;
}

bool isPositive(const QJsonObject& o) { return o.value("rating").toInt() >= 3; }

QString verdict(int pct, int n) {
    if (n < 10) return QString();  // pochi voti: niente giudizio, come Steam
    if (pct >= 95) return T("Estremamente positive");
    if (pct >= 80) return T("Molto positive");
    if (pct >= 70) return T("Perlopiù positive");
    if (pct >= 40) return T("Contrastanti");
    if (pct >= 20) return T("Perlopiù negative");
    return T("Molto negative");
}

}  // namespace

ReviewsDialog::ReviewsDialog(AuthManager* auth, FirestoreClient* fs, const QString& gameId,
                             const QString& title, QWidget* parent)
    : QDialog(parent), m_auth(auth), m_fs(fs), m_gameId(gameId) {
    setWindowTitle(T("Recensioni") + " — " + title);
    resize(560, 680);

    auto* root = new QVBoxLayout(this);
    m_summary = new QLabel(T("Caricamento…"));
    m_summary->setTextFormat(Qt::RichText);
    root->addWidget(m_summary);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(6);
    m_bar->setStyleSheet(
        QString("QProgressBar{background:%1;border:none;border-radius:3px;}"
                "QProgressBar::chunk{background:%2;border-radius:3px;}")
            .arg(kDownColor.name(), kUpColor.name()));
    m_bar->hide();
    root->addWidget(m_bar);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget;
    m_list = new QVBoxLayout(host);
    m_list->setAlignment(Qt::AlignTop);
    m_list->setSpacing(8);
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    const bool logged = m_auth && m_auth->currentUser().isValid();
    if (!logged) {
        root->addWidget(new QLabel(T("Accedi per lasciare un voto o un commento.")));
        load();  // le recensioni restano visibili anche senza login
        return;
    }

    auto* ask = new QLabel("<b>" + T("Consiglieresti questo gioco?") + "</b>");
    ask->setTextFormat(Qt::RichText);
    root->addWidget(ask);

    auto* row = new QHBoxLayout;
    m_like = makeVoteButton(T("Mi piace"), true);
    m_dislike = makeVoteButton(T("Non mi piace"), false);
    auto* grp = new QButtonGroup(this);
    grp->setExclusive(true);
    grp->addButton(m_like);
    grp->addButton(m_dislike);
    row->addWidget(m_like);
    row->addWidget(m_dislike);
    row->addStretch(1);
    root->addLayout(row);

    m_text = new QPlainTextEdit;
    m_text->setPlaceholderText(T("Scrivi un commento (facoltativo)…"));
    m_text->setMaximumHeight(100);
    root->addWidget(m_text);

    m_msg = new QLabel;
    root->addWidget(m_msg);
    m_send = new QPushButton(T("Pubblica"));
    connect(m_send, &QPushButton::clicked, this, [this]() { submit(); });
    root->addWidget(m_send);

    load();
}

void ReviewsDialog::load() {
    QJsonObject q;
    q["from"] = QJsonArray{QJsonObject{{"collectionId", "reviews"}}};
    q["where"] = QJsonObject{{"fieldFilter", QJsonObject{
        {"field", QJsonObject{{"fieldPath", "gameId"}}},
        {"op", "EQUAL"},
        {"value", QJsonObject{{"stringValue", m_gameId}}}}}};
    q["limit"] = 100;

    QPointer<ReviewsDialog> self(this);
    m_fs->runQuery(q, [self](bool ok, const QJsonArray& docs, const QString& err) {
        if (!self) return;
        if (!ok) { self->m_summary->setText(T("Impossibile caricare le recensioni: ") + err); return; }

        QList<QJsonObject> items;
        for (const auto& d : docs) items << d.toObject();
        std::sort(items.begin(), items.end(), [](const QJsonObject& a, const QJsonObject& b) {
            return a.value("ms").toDouble() > b.value("ms").toDouble();
        });

        while (QLayoutItem* it = self->m_list->takeAt(0)) { delete it->widget(); delete it; }

        const QString myUid = (self->m_auth && self->m_auth->currentUser().isValid())
                                  ? self->m_auth->currentUser().uid : QString();
        int positives = 0;

        for (const auto& o : items) {
            const bool pos = isPositive(o);
            if (pos) ++positives;

            if (self->m_like && !myUid.isEmpty() && o.value("uid").toString() == myUid) {
                (pos ? self->m_like : self->m_dislike)->setChecked(true);
                self->m_text->setPlainText(o.value("text").toString());
            }

            const QString date = QDateTime::fromMSecsSinceEpoch(qint64(o.value("ms").toDouble()))
                                     .toString("dd/MM/yyyy");
            const QString col = (pos ? kUpColor : kDownColor).name();

            auto* card = new QFrame;
            card->setObjectName("reviewCard");
            card->setStyleSheet("QFrame#reviewCard{background:rgba(255,255,255,0.05);border-radius:8px;}");
            auto* h = new QHBoxLayout(card);
            h->setContentsMargins(12, 10, 12, 10);
            h->setSpacing(12);

            auto* icon = new QLabel;
            icon->setPixmap(thumbPixmap(pos, pos ? kUpColor : kDownColor, 32));
            icon->setFixedSize(32, 32);
            h->addWidget(icon, 0, Qt::AlignTop);

            auto* v = new QVBoxLayout;
            v->setSpacing(4);
            auto* head = new QLabel(QString("<b>%1</b><br><span style='color:%2'>%3</span> &nbsp; "
                                            "<small style='color:#8f98a0'>%4</small>")
                .arg(o.value("name").toString().toHtmlEscaped(), col,
                     pos ? T("Consigliato") : T("Non consigliato"), date));
            head->setTextFormat(Qt::RichText);
            v->addWidget(head);

            const QString text = o.value("text").toString();
            if (!text.isEmpty()) {
                auto* body = new QLabel(text.toHtmlEscaped().replace("\n", "<br>"));
                body->setTextFormat(Qt::RichText);
                body->setWordWrap(true);
                body->setTextInteractionFlags(Qt::TextSelectableByMouse);
                v->addWidget(body);
            }
            h->addLayout(v, 1);
            self->m_list->addWidget(card);
        }

        const int n = items.size();
        if (n == 0) {
            self->m_bar->hide();
            self->m_summary->setText(T("Ancora nessuna recensione."));
        } else {
            const int pct = qRound(100.0 * positives / n);
            self->m_bar->setValue(pct);
            self->m_bar->show();
            const QString v = verdict(pct, n);
            self->m_summary->setText(QString("%1<b>%2%</b> %3 &nbsp; <small style='color:#8f98a0'>(%4 %5)</small>")
                .arg(v.isEmpty() ? QString() : "<b>" + v + "</b> — ")
                .arg(pct).arg(T("positive")).arg(n).arg(T("recensioni")));
        }
    });
}

void ReviewsDialog::submit() {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    if (!m_like->isChecked() && !m_dislike->isChecked()) {
        m_msg->setText(T("Scegli Mi piace o Non mi piace."));
        return;
    }
    const QString name = u.displayName.isEmpty() ? u.email.section('@', 0, 0) : u.displayName;
    QJsonObject f{
        {"gameId", m_gameId},
        {"uid", u.uid},
        {"name", name},
        {"rating", m_like->isChecked() ? 5 : 1},  // compatibile col sito
        {"text", m_text->toPlainText().trimmed().left(1000)},
        {"ms", double(QDateTime::currentMSecsSinceEpoch())}
    };
    m_send->setEnabled(false);
    QPointer<ReviewsDialog> self(this);
    m_fs->setDocument("reviews/" + m_gameId + "_" + u.uid, f, [self](bool ok, const QJsonObject&, const QString& err) {
        if (!self) return;
        self->m_send->setEnabled(true);
        self->m_msg->setText(ok ? T("Recensione salvata!") : T("Errore: ") + err);
        if (ok) self->load();
    });
}
