#include "SettingsDialog.h"
#include "ToggleSwitch.h"
#include "../core/ThemeManager.h"
#include "../core/LauncherSettings.h"
#include "../core/Config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QPainter>
#include <QFileDialog>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QPixmap>
#include <QMap>

namespace {

// Nome mostrato per ogni tema (ricavato dall'id, così non dipende dai nomi dei campi di ThemeDef)
QString themeLabel(const QString& id) {
    static const QMap<QString, QString> names = {
        {"night-theme", "Night"}, {"TVH-theme", "Valter House"}, {"matrix-theme", "Matrix"},
        {"hight-theme", "Hi-Contrast"}, {"inverted-theme", "Inverted"}, {"day-theme", "Day"},
    };
    return names.value(id, id);
}

// Card di anteprima di un tema: sfondo, accent e nome, con spunta sul tema attivo.
class ThemeSwatch : public QPushButton {
public:
    explicit ThemeSwatch(const ThemeDef& def, QWidget* parent = nullptr) : QPushButton(parent), m_def(def) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(104);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
        const QColor bg(m_def.bg), acc(m_def.accent);
        const bool dark = bg.lightness() < 128;

        p.setPen(QPen(isChecked() ? acc : QColor(128, 128, 128, 110), isChecked() ? 2 : 1));
        p.setBrush(bg);
        p.drawRoundedRect(r, 10, 10);

        // finte "righe di testo" + pulsante accent
        p.setPen(Qt::NoPen);
        p.setBrush(dark ? QColor(255, 255, 255, 45) : QColor(0, 0, 0, 40));
        p.drawRoundedRect(QRectF(r.left() + 14, r.bottom() - 50, 96, 6), 3, 3);
        p.drawRoundedRect(QRectF(r.left() + 14, r.bottom() - 38, 64, 6), 3, 3);
        p.setBrush(acc);
        p.drawRoundedRect(QRectF(r.left() + 14, r.bottom() - 24, 56, 10), 5, 5);

        p.setPen(dark ? QColor("#ffffff") : QColor("#111111"));
        QFont f("Rajdhani", 13);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(r.left() + 14, r.top() + 8, r.width() - 56, 26), Qt::AlignLeft | Qt::AlignVCenter, themeLabel(m_def.id));

        if (isChecked()) {
            const QPointF c(r.right() - 20, r.top() + 21);
            p.setPen(Qt::NoPen);
            p.setBrush(acc);
            p.drawEllipse(c, 10, 10);
            p.setPen(QPen(acc.lightness() > 150 ? QColor("#111111") : QColor("#ffffff"), 2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(c.x() - 4, c.y()), QPointF(c.x() - 1, c.y() + 3));
            p.drawLine(QPointF(c.x() - 1, c.y() + 3), QPointF(c.x() + 4, c.y() - 3));
        }
    }

private:
    ThemeDef m_def;
};

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Impostazioni");
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(800, 560);
    setMinimumSize(660, 460);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Barra laterale
    auto* side = new QFrame;
    side->setObjectName("SideNav");
    side->setFixedWidth(210);
    m_navLayout = new QVBoxLayout(side);
    m_navLayout->setContentsMargins(0, 20, 0, 16);
    m_navLayout->setSpacing(2);
    auto* sideTitle = new QLabel("IMPOSTAZIONI");
    sideTitle->setObjectName("Section");
    sideTitle->setContentsMargins(20, 0, 0, 6);
    m_navLayout->addWidget(sideTitle);
    m_navGroup = new QButtonGroup(this);
    root->addWidget(side);

    // ── Area destra: pagine + footer
    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    m_stack = new QStackedWidget;
    right->addWidget(m_stack, 1);

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(28, 8, 28, 16);
    auto* hint = new QLabel("Le modifiche vengono applicate subito.");
    hint->setObjectName("Muted");
    footer->addWidget(hint, 1);
    auto* closeBtn = new QPushButton("Chiudi");
    closeBtn->setObjectName("Secondary");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);
    right->addLayout(footer);
    root->addLayout(right, 1);

    buildGeneralPage();
    buildAppearancePage();
    buildGamesPage();
    buildAboutPage();

    m_navLayout->addStretch(1);
    if (auto* first = m_navGroup->button(0)) first->setChecked(true);
}

// Aggiunge una voce alla barra laterale e una pagina scorrevole; restituisce il layout del contenuto.
QVBoxLayout* SettingsDialog::addPage(const QString& navText, const QString& title, const QString& subtitle) {
    const int index = m_stack->count();

    auto* btn = new QPushButton(navText);
    btn->setObjectName("SideLink");
    btn->setCheckable(true);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    m_navGroup->addButton(btn, index);
    m_navLayout->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this, index]() { m_stack->setCurrentIndex(index); });

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* l = new QVBoxLayout(content);
    l->setContentsMargins(28, 24, 28, 16);
    l->setSpacing(8);

    auto* h = new QLabel(title);
    h->setObjectName("Heading");
    l->addWidget(h);
    auto* s = new QLabel(subtitle);
    s->setObjectName("Muted");
    s->setWordWrap(true);
    l->addWidget(s);

    scroll->setWidget(content);
    m_stack->addWidget(scroll);
    return l;
}

void SettingsDialog::addSection(QVBoxLayout* into, const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("Section");
    into->addWidget(l);
}

void SettingsDialog::addToggleRow(QVBoxLayout* into, const QString& title, const QString& desc, bool value,
                                  std::function<void(bool)> onToggle) {
    auto* row = new QFrame;
    row->setObjectName("Row");
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(16, 12, 16, 12);
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

    auto* sw = new ToggleSwitch;
    sw->setChecked(value);
    connect(sw, &QAbstractButton::toggled, this, [onToggle](bool on) { onToggle(on); });
    h->addWidget(sw, 0, Qt::AlignVCenter);
    into->addWidget(row);
}

void SettingsDialog::buildGeneralPage() {
    auto* l = addPage("Generale", "Generale", "Comportamento del launcher all'avvio e quando lanci un gioco.");
    addSection(l, "AVVIO");
    addToggleRow(l, "Controlla gli aggiornamenti all'avvio",
                 "Cerca nuove versioni dei giochi installati ogni volta che accedi.",
                 LauncherSettings::checkUpdatesOnStart(), [](bool on) { LauncherSettings::setCheckUpdatesOnStart(on); });
    addToggleRow(l, "Apri direttamente su «Installati»",
                 "Nella pagina dei giochi mostra subito solo quelli già installati.",
                 LauncherSettings::openOnInstalled(), [](bool on) { LauncherSettings::setOpenOnInstalled(on); });
    addSection(l, "PRIVACY");
    addToggleRow(l, "Mostra il mio stato online",
                 "Gli altri utenti vedono che sei nel launcher o in partita. Se lo spegni risulti offline.",
                 LauncherSettings::showOnlineStatus(), [](bool on) { LauncherSettings::setShowOnlineStatus(on); });
    addSection(l, "AVVIO DEI GIOCHI");
    addToggleRow(l, "Chiudi il launcher quando avvii un gioco",
                 "Il launcher si chiude dopo aver lanciato il gioco.",
                 LauncherSettings::closeOnLaunch(), [](bool on) { LauncherSettings::setCloseOnLaunch(on); });
    l->addStretch(1);
}

void SettingsDialog::buildAppearancePage() {
    auto* l = addPage("Aspetto", "Aspetto grafico", "Scegli il tema dell'interfaccia: lo stesso set del sito.");
    addSection(l, "TEMA");

    const QString current = ThemeManager::normalizeThemeId(
        LauncherSettings::theme().isEmpty() ? Config::instance().theme() : LauncherSettings::theme());

    auto* grid = new QGridLayout;
    grid->setSpacing(12);
    auto* group = new QButtonGroup(this);
    int i = 0;
    for (const auto& def : ThemeManager::themes()) {
        auto* sw = new ThemeSwatch(def);
        sw->setChecked(def.id == current);
        group->addButton(sw);
        const QString id = def.id;
        connect(sw, &QPushButton::clicked, this, [this, id]() {
            LauncherSettings::setTheme(id);
            emit themeChanged(id);
        });
        grid->addWidget(sw, i / 3, i % 3);
        ++i;
    }
    for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);
    l->addLayout(grid);
    l->addStretch(1);
}

void SettingsDialog::buildGamesPage() {
    auto* l = addPage("Giochi", "Giochi", "Dove vengono installati i giochi.");
    addSection(l, "CARTELLA DI INSTALLAZIONE");

    auto* row = new QFrame;
    row->setObjectName("Row");
    auto* v = new QVBoxLayout(row);
    v->setContentsMargins(16, 12, 16, 14);
    v->setSpacing(6);
    auto* t = new QLabel("Cartella dei giochi");
    t->setObjectName("RowTitle");
    auto* d = new QLabel("Vale per i giochi che installi da ora in poi; quelli già installati restano dove sono.");
    d->setObjectName("Muted");
    d->setWordWrap(true);
    v->addWidget(t);
    v->addWidget(d);

    auto* line = new QHBoxLayout;
    line->setSpacing(8);
    m_gamesDirEdit = new QLineEdit;
    m_gamesDirEdit->setReadOnly(true);
    line->addWidget(m_gamesDirEdit, 1);

    auto* browse = new QPushButton("Sfoglia…");
    browse->setObjectName("Secondary");
    browse->setCursor(Qt::PointingHandCursor);
    connect(browse, &QPushButton::clicked, this, [this]() {
        QString start = LauncherSettings::gamesDir().isEmpty() ? Config::instance().gamesBaseDir() : LauncherSettings::gamesDir();
        QString dir = QFileDialog::getExistingDirectory(this, "Cartella di installazione dei giochi", start);
        if (dir.isEmpty()) return;
        LauncherSettings::setGamesDir(dir);
        refreshGamesDir();
    });
    line->addWidget(browse);

    auto* reset = new QPushButton("Predefinita");
    reset->setObjectName("Secondary");
    reset->setCursor(Qt::PointingHandCursor);
    connect(reset, &QPushButton::clicked, this, [this]() {
        LauncherSettings::setGamesDir(QString());
        refreshGamesDir();
    });
    line->addWidget(reset);
    v->addLayout(line);
    l->addWidget(row);

    auto* openRow = new QFrame;
    openRow->setObjectName("Row");
    auto* oh = new QHBoxLayout(openRow);
    oh->setContentsMargins(16, 12, 12, 12);
    auto* ot = new QVBoxLayout;
    ot->setSpacing(2);
    auto* ott = new QLabel("Apri cartella dei giochi");
    ott->setObjectName("RowTitle");
    auto* otd = new QLabel("Mostra la cartella di installazione in Esplora file.");
    otd->setObjectName("Muted");
    ot->addWidget(ott);
    ot->addWidget(otd);
    oh->addLayout(ot, 1);
    auto* openBtn = new QPushButton("Apri");
    openBtn->setObjectName("Secondary");
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setMinimumWidth(104);
    connect(openBtn, &QPushButton::clicked, this, []() {
        QString dir = LauncherSettings::gamesDir().isEmpty() ? Config::instance().gamesBaseDir() : LauncherSettings::gamesDir();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    oh->addWidget(openBtn);
    l->addWidget(openRow);

    l->addStretch(1);
    refreshGamesDir();
}

void SettingsDialog::refreshGamesDir() {
    const QString custom = LauncherSettings::gamesDir();
    m_gamesDirEdit->setText(QDir::toNativeSeparators(custom.isEmpty() ? Config::instance().gamesBaseDir() : custom));
}

void SettingsDialog::buildAboutPage() {
    auto* l = addPage("Info", "Info", "ETA Games Launcher");

    auto* card = new QFrame;
    card->setObjectName("Row");
    auto* h = new QHBoxLayout(card);
    h->setContentsMargins(16, 16, 16, 16);
    h->setSpacing(16);
    auto* logo = new QLabel;
    QPixmap pix(":/logo.png");
    if (!pix.isNull()) logo->setPixmap(pix.scaledToHeight(64, Qt::SmoothTransformation));
    h->addWidget(logo);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto* name = new QLabel("ETA Games Launcher");
    name->setObjectName("CardTitle");
    const QString ver = QCoreApplication::applicationVersion().isEmpty() ? QString("dev") : QCoreApplication::applicationVersion();
    auto* info = new QLabel(QString("Versione %1 · Qt %2").arg(ver, qVersion()));
    info->setObjectName("Muted");
    texts->addWidget(name);
    texts->addWidget(info);
    h->addLayout(texts, 1);
    l->addWidget(card);

    addSection(l, "LINK");
    auto* links = new QHBoxLayout;
    links->setSpacing(8);
    const QList<QPair<QString, QString>> items = {
        {"Sito", "https://eta-games.github.io"},
        {"Discord", "https://discord.gg/Da7sq3Mwpw"},
        {"YouTube", "https://www.youtube.com/@HGames_studio"},
        {"Instagram", "https://www.instagram.com/eta.games_"},
    };
    for (const auto& it : items) {
        auto* b = new QPushButton(it.first);
        b->setObjectName("Secondary");
        b->setCursor(Qt::PointingHandCursor);
        const QString url = it.second;
        connect(b, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        links->addWidget(b);
    }
    links->addStretch(1);
    l->addLayout(links);
    l->addStretch(1);
}
