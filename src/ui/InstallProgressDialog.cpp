#include "InstallProgressDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QStyle>
#include <QRegularExpression>

namespace {
const char* kStyle = R"(
    QLabel#StepBadge { border: 2px solid #6b6b6b; border-radius: 15px; color: #8a8a8a; font-weight: 700; }
    QLabel#StepBadge[state="active"] { border-color: #4ea1ff; color: #4ea1ff; }
    QLabel#StepBadge[state="done"]   { border-color: #3ddc84; background-color: #3ddc84; color: #111111; }
    QLabel#StepBadge[state="error"]  { border-color: #ff5566; background-color: #ff5566; color: #ffffff; }

    QProgressBar#StepBar, QProgressBar#TotalBar {
        background-color: rgba(128, 128, 128, 0.25); border: none; border-radius: 4px;
        min-height: 8px; max-height: 8px; text-align: center;
    }
    QProgressBar#TotalBar { min-height: 5px; max-height: 5px; border-radius: 2px; }
    QProgressBar#StepBar::chunk { background-color: #4ea1ff; border-radius: 4px; }
    QProgressBar#StepBar[state="done"]::chunk  { background-color: #3ddc84; }
    QProgressBar#StepBar[state="error"]::chunk { background-color: #ff5566; }
    QProgressBar#TotalBar::chunk { border-radius: 2px; }

    QLabel#StepName[state="pending"] { color: #8a8a8a; }
    QLabel#ErrorBox   { color: #ff5566; background-color: rgba(255, 85, 102, 0.12);
                        border: 1px solid #ff5566; border-radius: 8px; padding: 8px 12px; }
)";

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}
} // namespace

InstallProgressDialog::InstallProgressDialog(const QString& gameTitle, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Installazione — " + gameTitle);
    setMinimumWidth(540);
    setModal(true);
    setStyleSheet(kStyle);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(14);

    // ── Intestazione: copertina, titolo, tempo e percentuale totale ──
    auto* head = new QHBoxLayout;
    head->setSpacing(14);

    m_cover = new QLabel;
    m_cover->setFixedSize(96, 64);
    m_cover->setAlignment(Qt::AlignCenter);
    m_cover->setScaledContents(false);
    m_cover->setStyleSheet("border-radius: 8px; background-color: rgba(128,128,128,0.18);");
    m_cover->hide(); // compare solo se la copertina è disponibile
    head->addWidget(m_cover);

    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_title = new QLabel("Installazione di " + gameTitle);
    m_title->setStyleSheet("font-size: 15pt; font-weight: 700;");
    m_title->setWordWrap(true);
    titles->addWidget(m_title);
    m_subtitle = new QLabel("Avvio del download…");
    m_subtitle->setObjectName("Muted");
    titles->addWidget(m_subtitle);
    head->addLayout(titles, 1);

    m_total = new QLabel("0%");
    m_total->setObjectName("TotalPct");
    m_total->setStyleSheet("font-size: 22pt; font-weight: 800;");
    m_total->setMinimumWidth(100);
    m_total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(m_total, 0, Qt::AlignVCenter);
    root->addLayout(head);

    m_totalBar = new QProgressBar;
    m_totalBar->setObjectName("TotalBar");
    m_totalBar->setRange(0, 100);
    m_totalBar->setTextVisible(false);
    root->addWidget(m_totalBar);

    // ── I tre passaggi ──
    auto* panel = new QFrame;
    panel->setObjectName("Panel");
    auto* steps = new QVBoxLayout(panel);
    steps->setContentsMargins(18, 16, 18, 16);
    steps->setSpacing(16);
    m_steps[0] = makeStep(steps, 1, "Preparazione",  "In attesa del server…");
    m_steps[1] = makeStep(steps, 2, "Download",      "In attesa dei dati…");
    m_steps[2] = makeStep(steps, 3, "Installazione", "In attesa…");
    root->addWidget(panel);

    m_error = new QLabel;
    m_error->setObjectName("ErrorBox");
    m_error->setWordWrap(true);
    m_error->hide();
    root->addWidget(m_error);

    // ── Dettagli (output di git), nascosti di default ──
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(130);
    m_log->setStyleSheet("font-family: Consolas, monospace; font-size: 9pt;");
    m_log->hide();
    root->addWidget(m_log);

    auto* btnRow = new QHBoxLayout;
    m_detailsBtn = new QPushButton("Mostra dettagli");
    m_detailsBtn->setObjectName("Secondary");
    m_detailsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_detailsBtn, &QPushButton::clicked, this, [this]() {
        const bool show = !m_log->isVisible();
        m_log->setVisible(show);
        m_detailsBtn->setText(show ? "Nascondi dettagli" : "Mostra dettagli");
        adjustSize();
    });
    btnRow->addWidget(m_detailsBtn);
    btnRow->addStretch();

    m_cancelBtn = new QPushButton("Annulla");
    m_cancelBtn->setObjectName("Secondary");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        if (m_finished) { accept(); return; }
        emit cancelRequested();
        m_cancelBtn->setEnabled(false);
        m_subtitle->setText("Annullamento in corso…");
    });
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    applyState(m_steps[0], Active, 0); // si parte dal primo passaggio
    ensurePolished(); // stili applicati prima del calcolo delle misure

    m_elapsed.start();
    m_clock = new QTimer(this);
    m_clock->setInterval(1000);
    connect(m_clock, &QTimer::timeout, this, &InstallProgressDialog::updateSubtitle);
    m_clock->start();
}

InstallProgressDialog::Step InstallProgressDialog::makeStep(QLayout* into, int number, const QString& name,
                                                           const QString& detail) {
    Step s;
    auto* block = new QVBoxLayout;
    block->setSpacing(7);

    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    s.badge = new QLabel(QString::number(number));
    s.badge->setObjectName("StepBadge");
    s.badge->setFixedSize(30, 30);
    s.badge->setAlignment(Qt::AlignCenter);
    row->addWidget(s.badge);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(3);
    s.name = new QLabel(name);
    s.name->setObjectName("StepName");
    s.name->setStyleSheet("font-size: 11pt; font-weight: 700;");
    texts->addWidget(s.name);
    s.detail = new QLabel(detail);
    s.detail->setObjectName("Muted");
    texts->addWidget(s.detail);
    row->addLayout(texts, 1);

    s.pct = new QLabel("0%");
    s.pct->setObjectName("StepPct");
    s.pct->setStyleSheet("font-weight: 700;");
    s.pct->setMinimumWidth(52);
    s.pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(s.pct, 0, Qt::AlignVCenter);
    block->addLayout(row);

    s.bar = new QProgressBar;
    s.bar->setObjectName("StepBar");
    s.bar->setRange(0, 100);
    s.bar->setTextVisible(false);
    block->addWidget(s.bar);

    static_cast<QVBoxLayout*>(into)->addLayout(block);

    s.state = Pending;
    s.badge->setProperty("state", "pending");
    s.name->setProperty("state", "pending");
    s.bar->setProperty("state", "pending");
    return s;
}

void InstallProgressDialog::applyState(Step& s, State st, int index) {
    s.state = st;
    const char* key = st == Pending ? "pending" : st == Active ? "active" : st == Done ? "done" : "error";
    s.badge->setProperty("state", key);
    s.name->setProperty("state", key);
    s.bar->setProperty("state", key);
    s.badge->setText(st == Done ? QString::fromUtf8("✓") : st == Error ? "!" : QString::number(index + 1));
    repolish(s.badge); repolish(s.name); repolish(s.bar);
}

// Imposta l'avanzamento di un passaggio (mai all'indietro) e porta a "fatti" quelli precedenti
void InstallProgressDialog::setStep(int i, int pct, const QString& detail) {
    finishStepsBefore(i);
    Step& s = m_steps[i];
    if (s.state == Error) return;
    s.value = qMax(s.value, qBound(0, pct, 100));
    s.bar->setValue(s.value);
    s.pct->setText(QString("%1%").arg(s.value));
    if (!detail.isEmpty()) s.detail->setText(detail);
    const State want = s.value >= 100 ? Done : Active;
    if (s.state != want) applyState(s, want, i);
}

void InstallProgressDialog::finishStepsBefore(int i) {
    for (int k = 0; k < i; ++k) {
        Step& p = m_steps[k];
        if (p.state == Error) continue;
        p.value = 100;
        p.bar->setValue(100);
        p.pct->setText("100%");
        if (p.state != Done) applyState(p, Done, k);
    }
}

void InstallProgressDialog::onPhaseProgress(const QString& phase, int phasePct, const QString& rawLine) {
    m_log->appendPlainText(rawLine);

    if (phase == "enumerating") {
        setStep(0, 0, "Il server sta elencando i file…");
    } else if (phase == "counting") {
        m_counting = phasePct;
        setStep(0, (m_counting + m_compressing) / 2, "Conteggio dei file…");
    } else if (phase == "compressing") {
        m_compressing = phasePct;
        setStep(0, (m_counting + m_compressing) / 2, "Compressione dei file…");
    } else if (phase == "receiving") {
        // es. "Receiving objects:  45% (10/23), 164.72 KiB | 1.26 MiB/s"
        static const QRegularExpression re(R"(,\s*([\d.,]+\s*\w+)\s*\|\s*([\d.,]+\s*\w+/s))");
        QString detail = "Scarico i file del gioco…";
        const auto m = re.match(rawLine);
        if (m.hasMatch()) detail = QString("%1 scaricati  •  %2").arg(m.captured(1).trimmed(), m.captured(2).trimmed());
        setStep(1, phasePct, detail);
    } else if (phase == "resolving") {
        m_resolving = phasePct;
        setStep(2, (m_resolving + m_checkout) / 2, "Ricostruzione dei file…");
    } else if (phase == "checkout") {
        m_checkout = phasePct;
        setStep(2, (m_resolving + m_checkout) / 2, "Scrittura dei file sul disco…");
    }
}

void InstallProgressDialog::onOverallProgress(int pct) {
    m_totalBar->setValue(pct);
    m_total->setText(QString("%1%").arg(pct));
}

void InstallProgressDialog::updateSubtitle() {
    if (m_finished) return;
    const int s = static_cast<int>(m_elapsed.elapsed() / 1000);
    m_subtitle->setText(QString("Trascorso %1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
}

void InstallProgressDialog::setCover(const QPixmap& cover) {
    if (cover.isNull()) return;
    m_cover->setPixmap(cover.scaled(m_cover->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                            .copy(0, 0, m_cover->width(), m_cover->height()));
    m_cover->show();
}

void InstallProgressDialog::reject() {
    if (m_finished) QDialog::reject(); // durante il download si usa solo il pulsante "Annulla"
}

void InstallProgressDialog::onFinished(bool success, const QString& error) {
    m_finished = true;
    m_clock->stop();
    m_cancelBtn->setText("Chiudi");
    m_cancelBtn->setEnabled(true);

    if (success) {
        finishStepsBefore(3);
        for (auto& st : m_steps) st.detail->setText("Completato");
        onOverallProgress(100);
        m_title->setText("Installazione completata");
        m_subtitle->setText(QString("Fatto in %1 secondi. Puoi giocare!").arg(m_elapsed.elapsed() / 1000));
    } else {
        // il passaggio in corso diventa rosso
        for (int i = 0; i < 3; ++i) {
            if (m_steps[i].state == Active || (m_steps[i].state == Pending && i == 0)) {
                applyState(m_steps[i], Error, i);
                break;
            }
        }
        m_title->setText("Installazione non riuscita");
        m_subtitle->setText(QString("Interrotta dopo %1 secondi.").arg(m_elapsed.elapsed() / 1000));
        m_error->setText(error);
        m_error->show();
    }
}
