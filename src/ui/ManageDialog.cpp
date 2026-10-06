#include "ManageDialog.h"
#include "CoverUtil.h"
#include "../core/GameManager.h"
#include "../core/Config.h"
#include "../core/ShortcutManager.h"
#include "../core/I18n.h"
#include "../core/BroadcastManager.h"
#include "ChangelogDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

    static const int DIALOG_W = 520;

ManageDialog::ManageDialog(GameManager* games, const GameEntry& game, const QPixmap& cover,
                           BroadcastManager* broadcast, QWidget* parent)
    : QDialog(parent),
    m_games(games),
    m_game(game),
    m_broadcast(broadcast) {
    setWindowTitle("Gestisci — " + game.title);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFixedWidth(DIALOG_W);
    setMinimumHeight(460);
    resize(DIALOG_W, 640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Banner con la copertina del gioco
    auto* banner = new QLabel;
    banner->setObjectName("Banner");
    banner->setFixedHeight(120);
    banner->setAlignment(Qt::AlignCenter);

    if (!cover.isNull()) {
        banner->setPixmap(makeRoundedCover(cover, DIALOG_W, 120, 0));
    }

    root->addWidget(banner);

    // ── Intestazione: titolo, motore/dimensione/versione, cartella di installazione
    GameMeta meta = m_games->loadMeta(game.id);

    auto* head = new QVBoxLayout;
    head->setContentsMargins(24, 16, 24, 8);
    head->setSpacing(2);

    auto* title = new QLabel(game.title);
    title->setObjectName("Heading");
    head->addWidget(title);

    auto* info = new QLabel(
        QString("%1 · %2 · v%3")
            .arg(game.engine, game.size, game.version)
        );
    info->setObjectName("Muted");
    head->addWidget(info);

    auto* path = new QLabel(QDir::toNativeSeparators(meta.gameDir));
    path->setObjectName("Muted");
    path->setWordWrap(true);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    head->addWidget(path);

    root->addLayout(head);

    // ── Contenuto scorrevole
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(24, 0, 24, 12);
    cl->setSpacing(8);

    // ── GENERALE
    addSection(cl, "GENERALE");
    addRow(
        cl,
        "Cartella di gioco",
        "Apri la cartella di installazione in Esplora file.",
        "Apri",
        [this]() {
            if (!m_games->openGameFolder(m_game.id)) {
                setStatus("Cartella non trovata.", false);
            }
        }
        );

    addRow(
        cl,
        "Collegamento sul Desktop",
        "Avvia il gioco direttamente dal Desktop.",
        "Crea",
        [this]() {
            GameMeta m = m_games->loadMeta(m_game.id);

            Q_UNUSED(m);

            bool ok = ShortcutManager::createDesktopShortcut(
                m_game.id,
                m_game.title.isEmpty() ? m_game.id : m_game.title
                );

            setStatus(
                ok
                    ? "Collegamento creato sul Desktop."
                    : "Impossibile creare il collegamento.",
                ok
                );
        }
        );

    // ── CONFIGURAZIONE
    if (game.needsDoom2Wad) {
        addSection(cl, "CONFIGURAZIONE");
        addWadRow(cl);
    }

    // ── MANUTENZIONE
    addSection(cl, T("MANUTENZIONE"));

    if (m_broadcast) {
        addRow(
            cl,
            T("Novità e changelog"),
            T("Le note dell'ultima release, come pubblicate dagli sviluppatori."),
            T("Apri"),
            [this]() {
                m_broadcast->refreshSource(m_game.id);

                ChangelogDialog dlg(
                    m_game.id,
                    m_game.title,
                    m_broadcast,
                    this
                    );

                dlg.exec();
            }
            );
    }

    addRow(
        cl,
        T("Verifica e ripara file"),
        T("Controlla che i file del gioco siano intatti e ripristina quelli modificati o cancellati, senza reinstallare."),
        T("Verifica"),
        [this]() {
            startVerify();
        }
        );

    // ── DATI
    addSection(cl, "DATI");

    addRow(
        cl,
        "Ripristina configurazione",
        "Elimina i file .ini del gioco: torneranno ai valori predefiniti.",
        "Ripristina",
        [this]() {
            if (!confirm(
                    "Ripristina configurazione",
                    "Eliminare i file .ini del gioco?"
                    )) {
                return;
            }

            m_games->resetGameConfig(m_game.id);
            setStatus("Configurazione ripristinata.");
        }
        );

    addRow(
        cl,
        "Salvataggi",
        "Elimina tutti i salvataggi del gioco.",
        "Elimina",
        [this]() {
            if (!confirm(
                    "Cancella salvataggi",
                    "Eliminare tutti i salvataggi? L'operazione non si può annullare."
                    )) {
                return;
            }

            int n = m_games->clearGameSaves(m_game.id);

            setStatus(
                QString("Eliminati %1 file di salvataggio.").arg(n)
                );
        }
        );

    // ── ZONA PERICOLOSA
    addSection(cl, "ZONA PERICOLOSA");

    addRow(
        cl,
        "Disinstalla",
        "Rimuove il gioco e tutti i suoi file dal computer.",
        "Disinstalla",
        [this]() {
            if (!confirm(
                    "Disinstalla",
                    "Disinstallare " + m_game.title +
                        "? Verranno eliminati tutti i file."
                    )) {
                return;
            }

            m_games->removeGame(m_game.id);
            emit uninstalled(m_game.id);
            accept();
        },
        true
        );

    cl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // ── Footer: messaggio di stato + Chiudi
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(24, 8, 24, 16);

    m_status = new QLabel;
    m_status->setObjectName("Muted");
    m_status->setWordWrap(true);

    footer->addWidget(m_status, 1);

    auto* closeBtn = new QPushButton("Chiudi");
    closeBtn->setObjectName("Secondary");
    closeBtn->setCursor(Qt::PointingHandCursor);

    connect(
        closeBtn,
        &QPushButton::clicked,
        this,
        &QDialog::accept
        );

    footer->addWidget(closeBtn);
    root->addLayout(footer);

    connect(
        m_games,
        &GameManager::verifyFinished,
        this,
        &ManageDialog::onVerifyFinished
        );

    connect(
        m_games,
        &GameManager::repairFinished,
        this,
        &ManageDialog::onRepairFinished
        );
}

// ── Verifica e ripara ─────────────────────────────────────────────────────

static QString gitErrorText(const QString& code) {
    if (code == "git_missing") {
        return T("git non trovato nel PATH. Installa Git for Windows e riprova.");
    }

    if (code == "not_git") {
        return T("Questo gioco non è un clone git: reinstallalo per poterlo verificare.");
    }

    if (code == "git_failed") {
        return T("git non è andato a buon fine.");
    }

    return code;
}

void ManageDialog::startVerify() {
    if (m_busy) {
        return;
    }

    m_busy = true;

    setStatus(
        T("Controllo i file del gioco…")
        );

    m_games->verifyGame(m_game.id);
}

void ManageDialog::onVerifyFinished(
    const QString& id,
    bool ok,
    QStringList modified,
    QStringList deleted,
    QString error
    ) {
    if (id != m_game.id) {
        return;
    }

    m_busy = false;

    if (!ok) {
        setStatus(
            gitErrorText(error),
            false
            );
        return;
    }

    const int total = modified.size() + deleted.size();

    if (total == 0) {
        setStatus(
            T("Tutti i file sono a posto: niente da riparare.")
            );
        return;
    }

    QStringList sample;

    for (const QString& f : deleted) {
        if (sample.size() < 8) {
            sample << "• " + f + "  (" + T("cancellato") + ")";
        }
    }

    for (const QString& f : modified) {
        if (sample.size() < 8) {
            sample << "• " + f + "  (" + T("modificato") + ")";
        }
    }

    QString text = T(
                       "Trovati %1 file da ripristinare: %2 modificati, %3 cancellati."
                       )
                       .arg(total)
                       .arg(modified.size())
                       .arg(deleted.size());

    text += "\n\n" + sample.join("\n");

    if (total > sample.size()) {
        text += "\n" + T("…e altri %1.").arg(total - sample.size());
    }

    text += "\n\n" + T(
                "Le modifiche fatte a questi file andranno perse. "
                "Salvataggi e configurazioni non vengono toccati. Ripristinare?"
                );

    if (!confirm(
            T("Verifica e ripara file"),
            text
            )) {
        setStatus(
            T("Nessun file ripristinato.")
            );
        return;
    }

    m_pendingRepair = deleted + modified;
    m_busy = true;

    setStatus(
        T("Ripristino dei file in corso…")
        );

    m_games->repairGame(
        m_game.id,
        m_pendingRepair
        );
}

void ManageDialog::onRepairFinished(
    const QString& id,
    bool ok,
    int repaired,
    QString error
    ) {
    if (id != m_game.id) {
        return;
    }

    m_busy = false;
    m_pendingRepair.clear();

    if (ok) {
        setStatus(
            T("Ripristinati %1 file.").arg(repaired)
            );
    } else {
        setStatus(
            T("Ripristino non riuscito: %1")
                .arg(gitErrorText(error)),
            false
            );
    }
}

void ManageDialog::addSection(
    QVBoxLayout* into,
    const QString& text
    ) {
    auto* l = new QLabel(text);
    l->setObjectName("Section");

    into->addWidget(l);
}

void ManageDialog::addRow(
    QVBoxLayout* into,
    const QString& title,
    const QString& desc,
    const QString& btnText,
    std::function<void()> onClick,
    bool danger
    ) {
    auto* row = new QFrame;

    row->setObjectName(
        danger ? "DangerRow" : "Row"
        );

    auto* h = new QHBoxLayout(row);

    h->setContentsMargins(
        16,
        12,
        12,
        12
        );

    h->setSpacing(12);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);

    auto* t = new QLabel(title);
    t->setObjectName("RowTitle");

    auto* d = new QLabel(desc);
    d->setObjectName("Muted");
    d->setWordWrap(true);

    texts->addWidget(t);
    texts->addWidget(d);

    h->addLayout(texts, 1);

    auto* b = new QPushButton(btnText);
    b->setObjectName(
        danger ? "Danger" : "Secondary"
        );

    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumWidth(104);

    connect(
        b,
        &QPushButton::clicked,
        this,
        [onClick]() {
            onClick();
        }
        );

    h->addWidget(
        b,
        0,
        Qt::AlignVCenter
        );

    into->addWidget(row);
}

// ── Selezione di doom2.wad ────────────────────────────────────────────────
// Salvata nei dati di QUESTO gioco (meta.json),
// non più nelle impostazioni generali.

void ManageDialog::addWadRow(
    QVBoxLayout* into
    ) {
    auto* row = new QFrame;
    row->setObjectName("Row");

    auto* v = new QVBoxLayout(row);

    v->setContentsMargins(
        16,
        12,
        16,
        14
        );

    v->setSpacing(6);

    auto* t = new QLabel(
        "Doom II WAD (doom2.wad)"
        );
    t->setObjectName("RowTitle");

    auto* d = new QLabel(
        "Serve per avviare il gioco e non è incluso nel download. "
        "Va scelto una volta sola."
        );

    d->setObjectName("Muted");
    d->setWordWrap(true);

    v->addWidget(t);
    v->addWidget(d);

    auto* line = new QHBoxLayout;
    line->setSpacing(8);

    m_wadPath = new QLineEdit;
    m_wadPath->setReadOnly(true);
    m_wadPath->setPlaceholderText(
        "Nessun file selezionato"
        );

    GameMeta meta = m_games->loadMeta(m_game.id);

    QString cur =
        meta.doom2WadPath.isEmpty()
            ? Config::instance().doom2WadPath()
            : meta.doom2WadPath;

    if (!cur.isEmpty()) {
        m_wadPath->setText(
            QDir::toNativeSeparators(cur)
            );
    }

    line->addWidget(m_wadPath, 1);

    auto* browse = new QPushButton("Sfoglia…");
    browse->setObjectName("Secondary");
    browse->setCursor(Qt::PointingHandCursor);

    connect(
        browse,
        &QPushButton::clicked,
        this,
        &ManageDialog::browseWad
        );

    line->addWidget(browse);

    v->addLayout(line);

    m_wadState = new QLabel;
    m_wadState->setObjectName("Muted");

    v->addWidget(m_wadState);

    updateWadState();

    into->addWidget(row);
}

void ManageDialog::browseWad() {
    QString start =
        m_wadPath->text().isEmpty()
            ? QDir::homePath()
            : QFileInfo(m_wadPath->text()).absolutePath();

    QString f = QFileDialog::getOpenFileName(
        this,
        "Seleziona doom2.wad",
        start,
        "WAD di Doom (*.wad);;Tutti i file (*)"
        );

    if (f.isEmpty()) {
        return;
    }

    m_games->setDoom2WadForGame(
        m_game.id,
        f
        );

    m_wadPath->setText(
        QDir::toNativeSeparators(f)
        );

    updateWadState();

    if (
        QFileInfo(f)
            .fileName()
            .compare(
                "doom2.wad",
                Qt::CaseInsensitive
                ) != 0
        ) {
        setStatus(
            "File salvato, ma non si chiama doom2.wad: "
            "controlla di aver scelto quello giusto.",
            false
            );
    } else {
        setStatus(
            "doom2.wad impostato."
            );
    }
}

void ManageDialog::updateWadState() {
    if (!m_wadState) {
        return;
    }

    QString p = m_wadPath->text();

    if (p.isEmpty()) {
        m_wadState->setText(
            "Non impostato"
            );
    } else {
        m_wadState->setText(
            QFile::exists(p)
                ? "File trovato"
                : "File non trovato in questo percorso"
            );
    }
}

void ManageDialog::setStatus(
    const QString& text,
    bool ok
    ) {
    m_status->setText(text);

    m_status->setStyleSheet(
        ok
            ? QString()
            : "color: #ff5566;"
        );
}

bool ManageDialog::confirm(
    const QString& title,
    const QString& text
    ) {
    return QMessageBox::question(
               this,
               title,
               text,
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No
               ) == QMessageBox::Yes;
}