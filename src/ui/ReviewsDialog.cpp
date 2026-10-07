#include "ReviewsDialog.h"
#include "../core/I18n.h"
#include "../core/AuthManager.h"
#include "../core/FirestoreClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QDateTime>
#include <QPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

ReviewsDialog::ReviewsDialog(AuthManager* auth, FirestoreClient* fs, const QString& gameId,
                             const QString& title, QWidget* parent)
    : QDialog(parent), m_auth(auth), m_fs(fs), m_gameId(gameId) {
    setWindowTitle(T("Recensioni") + " — " + title);
    resize(560, 640);

    auto* root = new QVBoxLayout(this);
    m_summary = new QLabel(T("Caricamento…"));
    root->addWidget(m_summary);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* host = new QWidget;
    m_list = new QVBoxLayout(host);
    m_list->setAlignment(Qt::AlignTop);
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    const bool logged = m_auth && m_auth->currentUser().isValid();
    if (!logged) {
        root->addWidget(new QLabel(T("Accedi per lasciare un voto o un commento.")));
        return;
    }
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(T("Il tuo voto:")));
    m_rating = new QComboBox;
    for (int i = 5; i >= 1; --i) m_rating->addItem(QString(i, QChar(0x2605)) + QString(5 - i, QChar(0x2606)), i);
    row->addWidget(m_rating);
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
        double sum = 0;
        const QString myUid = self->m_auth->currentUser().uid;
        for (const auto& o : items) {
            const int r = qBound(1, o.value("rating").toInt(), 5);
            sum += r;
            if (self->m_rating && o.value("uid").toString() == myUid) {
                self->m_rating->setCurrentIndex(self->m_rating->findData(r));
                self->m_text->setPlainText(o.value("text").toString());
            }
            const QString date = QDateTime::fromMSecsSinceEpoch(qint64(o.value("ms").toDouble())).toString("dd/MM/yyyy");
            auto* lbl = new QLabel(QString("<b>%1</b> &nbsp; <span style='color:#f5b301'>%2</span> &nbsp; <small>%3</small><br>%4")
                .arg(o.value("name").toString().toHtmlEscaped(),
                     QString(r, QChar(0x2605)) + QString(5 - r, QChar(0x2606)),
                     date,
                     o.value("text").toString().toHtmlEscaped().replace("\n", "<br>")));
            lbl->setWordWrap(true);
            lbl->setTextFormat(Qt::RichText);
            self->m_list->addWidget(lbl);
        }
        if (items.isEmpty()) {
            self->m_summary->setText(T("Ancora nessuna recensione."));
        } else {
            self->m_summary->setText(QString("%1: %2 / 5  (%3 %4)")
                .arg(T("Media")).arg(sum / items.size(), 0, 'f', 1).arg(items.size()).arg(T("voti")));
        }
    });
}

void ReviewsDialog::submit() {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    QString name = u.displayName.isEmpty() ? u.email.section('@', 0, 0) : u.displayName;
    QJsonObject f{
        {"gameId", m_gameId},
        {"uid", u.uid},
        {"name", name},
        {"rating", m_rating->currentData().toInt()},
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
