#include "StatsWidget.h"
#include "../core/I18n.h"
#include "ProfileWidget.h"
#include "ToggleSwitch.h"
#include "../core/FirestoreClient.h"
#include "../core/GameCatalog.h"
#include "../core/GameManager.h"
#include "../core/ThemeManager.h"
#include "../core/Config.h"
#include "../core/LauncherSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QImage>
#include <QBuffer>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QDateTime>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>

namespace {

QColor currentAccent() {
    const QString id = ThemeManager::normalizeThemeId(
        LauncherSettings::theme().isEmpty() ? Config::instance().theme() : LauncherSettings::theme());
    for (const auto& t : ThemeManager::themes()) if (t.id == id) return QColor(t.accent);
    return QColor("#e50914");
}

// Avatar tondo con anello accent; senza foto mostra l'iniziale.
QPixmap makeAvatar(const QPixmap& photo, const QString& initial, int size) {
    const qreal dpr = 2.0;
    QPixmap out(int(size * dpr), int(size * dpr));
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QColor accent = currentAccent();
    QPainterPath clip;
    clip.addEllipse(QRectF(2, 2, size - 4, size - 4));
    p.setClipPath(clip);
    if (!photo.isNull()) {
        QPixmap s = photo.scaled(QSize(int((size - 4) * dpr), int((size - 4) * dpr)), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap(QRectF(2, 2, size - 4, size - 4), s, QRectF((s.width() - (size - 4) * dpr) / 2, (s.height() - (size - 4) * dpr) / 2, (size - 4) * dpr, (size - 4) * dpr));
    } else {
        p.fillRect(QRectF(0, 0, size, size), accent);
        p.setPen(accent.lightness() > 150 ? QColor("#111111") : QColor("#ffffff"));
        QFont f("Rajdhani", size / 2);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, initial);
    }
    p.setClipping(false);
    p.setPen(QPen(accent, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2));
    return out;
}

QPixmap pixmapFromDataUrl(const QString& dataUrl) {
    const int i = dataUrl.indexOf("base64,");
    if (i < 0) return QPixmap();
    QPixmap pm;
    pm.loadFromData(QByteArray::fromBase64(dataUrl.mid(i + 7).toLatin1()));
    return pm;
}

// Come resizeImage() del sito: riduce mantenendo le proporzioni e restituisce un data URL JPEG (q=80)
QString imageToDataUrl(const QString& path, int maxW, int maxH) {
    QImage img(path);
    if (img.isNull()) return QString();
    if (img.width() > maxW || img.height() > maxH)
        img = img.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", 80);
    return "data:image/jpeg;base64," + QString::fromLatin1(bytes.toBase64());
}

// Badge come nel sito (in base ai giorni di registrazione)
QString badgeForDays(qint64 days) {
    QString badge = "Novellino";
    if (days > 30)  badge = "Gamer Esperto";
    if (days > 180) badge = "Veterano";
    if (days > 365) badge = "Leggenda";
    if (days > 730) badge = "Maestro Supremo";
    return badge;
}

QFrame* makeRow() {
    auto* row = new QFrame;
    row->setObjectName("Row");
    return row;
}

QLabel* makeLabel(const QString& text, const char* objectName = nullptr) {
    auto* l = new QLabel(text);
    if (objectName) l->setObjectName(objectName);
    l->setWordWrap(true);
    return l;
}

QPushButton* makeButton(const QString& text, const char* objectName = "Secondary") {
    auto* b = new QPushButton(text);
    if (objectName) b->setObjectName(objectName);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace

ProfileWidget::ProfileWidget(AuthManager* auth, FirestoreClient* fs, QWidget* parent)
    : QWidget(parent), m_auth(auth), m_fs(fs) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    root->addWidget(m_stack);

    buildGuestPage();
    buildAccountPage();

    m_messageTimer = new QTimer(this);
    m_messageTimer->setSingleShot(true);
    connect(m_messageTimer, &QTimer::timeout, this, [this]() { m_message->clear(); });

    connect(m_auth, &AuthManager::loginSucceeded, this, [this](AuthUser) { reload(true); });
    connect(m_auth, &AuthManager::userUpdated, this, [this](AuthUser u) {
        m_nameLabel->setText(u.displayName.isEmpty() ? u.email : u.displayName);
        m_emailLabel->setText(u.email);
        updateAvatars();
    });
    refresh();
}

void ProfileWidget::buildGuestPage() {
    auto* guest = new QWidget;
    auto* l = new QVBoxLayout(guest);
    l->setAlignment(Qt::AlignCenter);
    l->setSpacing(14);
    auto* t = makeLabel("Accesso riservato", "Heading");
    t->setAlignment(Qt::AlignCenter);
    l->addWidget(t);
    auto* d = makeLabel("Effettua il login per vedere il tuo profilo ETA Games.", "Muted");
    d->setAlignment(Qt::AlignCenter);
    l->addWidget(d);
    auto* b = new QPushButton("Vai al login");
    b->setFixedWidth(220);
    connect(b, &QPushButton::clicked, this, [this]() { emit goToLoginRequested(); });
    l->addWidget(b, 0, Qt::AlignCenter);
    m_stack->addWidget(guest);
}

void ProfileWidget::buildAccountPage() {
    auto* page = new QWidget;
    auto* h = new QHBoxLayout(page);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    auto* side = new QFrame;
    side->setObjectName("SideNav");
    side->setFixedWidth(210);
    m_navLayout = new QVBoxLayout(side);
    m_navLayout->setContentsMargins(0, 20, 0, 16);
    m_navLayout->setSpacing(2);
    auto* sideTitle = new QLabel("ACCOUNT");
    sideTitle->setObjectName("Section");
    sideTitle->setContentsMargins(20, 0, 0, 6);
    m_navLayout->addWidget(sideTitle);
    m_navGroup = new QButtonGroup(this);
    h->addWidget(side);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    m_sections = new QStackedWidget;
    right->addWidget(m_sections, 1);
    m_message = new QLabel;
    m_message->setObjectName("Muted");
    m_message->setContentsMargins(28, 4, 28, 12);
    m_message->setWordWrap(true);
    right->addWidget(m_message);
    h->addLayout(right, 1);

    buildOverview();
    buildAppearance();
    buildProfileSettings();
    buildSecurity();
    buildExtra();
    buildLibrary();
    {
        auto* sl = addSection(T("Statistiche"), T("Statistiche di gioco"));
        sl->addWidget(new StatsWidget);
        sl->addStretch(1);
    }

    m_navLayout->addStretch(1);
    if (auto* first = m_navGroup->button(0)) first->setChecked(true);
    m_stack->addWidget(page);
}

QVBoxLayout* ProfileWidget::addSection(const QString& navText, const QString& title) {
    const int index = m_sections->count();
    auto* btn = new QPushButton(navText);
    btn->setObjectName("SideLink");
    btn->setCheckable(true);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    m_navGroup->addButton(btn, index);
    m_navLayout->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this, index]() { m_sections->setCurrentIndex(index); });

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* l = new QVBoxLayout(content);
    l->setContentsMargins(28, 24, 28, 12);
    l->setSpacing(10);
    l->addWidget(makeLabel(title, "Heading"));
    scroll->setWidget(content);
    m_sections->addWidget(scroll);
    return l;
}

void ProfileWidget::addExtraSection(const QString& navText, const QString& title, QWidget* content) {
    if (!m_sections || !m_navLayout || !m_navGroup) return;
    const int index = m_sections->count();
    auto* btn = new QPushButton(navText);
    btn->setObjectName("SideLink");
    btn->setCheckable(true);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    m_navGroup->addButton(btn, index);
    m_navLayout->insertWidget(m_navLayout->count() - 1, btn);   // prima dello stretch finale
    connect(btn, &QPushButton::clicked, this, [this, index]() { m_sections->setCurrentIndex(index); });

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget;
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(28, 24, 28, 12);
    l->setSpacing(10);
    l->addWidget(makeLabel(title, "Heading"));
    l->addWidget(content);
    l->addStretch(1);
    scroll->setWidget(page);
    m_sections->addWidget(scroll);
}

// ── Panoramica ───────────────────────────────────────────────────────────
void ProfileWidget::buildOverview() {
    auto* l = addSection("Panoramica", "Panoramica profilo");

    auto* head = makeRow();
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(16, 16, 16, 16);
    hl->setSpacing(16);
    m_overviewAvatar = new QLabel;
    m_overviewAvatar->setFixedSize(72, 72);
    hl->addWidget(m_overviewAvatar);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    m_nameLabel = makeLabel("", "Heading");
    texts->addWidget(m_nameLabel);
    auto* mailRow = new QHBoxLayout;
    m_emailLabel = makeLabel("", "Muted");
    mailRow->addWidget(m_emailLabel);
    auto* copy = makeButton("Copia");
    copy->setMinimumHeight(32);
    connect(copy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_emailLabel->text());
        showMessage("Email copiata.");
    });
    mailRow->addWidget(copy);
    mailRow->addStretch(1);
    texts->addLayout(mailRow);
    m_statusLine = makeLabel("", "Online");
    texts->addWidget(m_statusLine);
    hl->addLayout(texts, 1);
    l->addWidget(head);

    auto* info = makeRow();
    auto* g = new QGridLayout(info);
    g->setContentsMargins(16, 12, 16, 12);
    g->setHorizontalSpacing(24);
    g->setVerticalSpacing(8);
    const QStringList keys = {"Account creato", "Ultimo accesso", "Giorni registrato"};
    QLabel** values[] = {&m_createdLabel, &m_lastLoginLabel, &m_daysLabel};
    for (int i = 0; i < 3; ++i) {
        g->addWidget(makeLabel(keys[i], "Muted"), i, 0);
        *values[i] = makeLabel("…", "RowTitle");
        g->addWidget(*values[i], i, 1);
    }
    g->setColumnStretch(1, 1);
    l->addWidget(info);

    auto* logout = makeButton("Esci dall'account");
    connect(logout, &QPushButton::clicked, this, [this]() {
        m_auth->signOut();
        m_loadedUid.clear();
        emit loggedOut();
    });
    l->addWidget(logout);
    l->addStretch(1);
}

// ── Aspetto grafico ──────────────────────────────────────────────────────
void ProfileWidget::buildAppearance() {
    auto* l = addSection("Aspetto grafico", "Aspetto grafico");

    l->addWidget(makeLabel("IMMAGINE PROFILO", "Section"));
    auto* row = makeRow();
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(16, 14, 16, 14);
    rl->setSpacing(16);
    m_avatarPreview = new QLabel;
    m_avatarPreview->setFixedSize(90, 90);
    rl->addWidget(m_avatarPreview);
    auto* up = makeButton("Carica nuova foto");
    connect(up, &QPushButton::clicked, this, [this]() { uploadImage(false); });
    rl->addWidget(up);
    rl->addStretch(1);
    l->addWidget(row);

    l->addWidget(makeLabel("BANNER PROFILO (480p)", "Section"));
    auto* brow = makeRow();
    auto* bl = new QVBoxLayout(brow);
    bl->setContentsMargins(16, 14, 16, 14);
    bl->setSpacing(10);
    m_bannerPreview = new QLabel;
    m_bannerPreview->setFixedHeight(150);
    m_bannerPreview->setAlignment(Qt::AlignCenter);
    m_bannerPreview->setObjectName("Banner");
    bl->addWidget(m_bannerPreview);
    auto* bup = makeButton("Carica banner");
    connect(bup, &QPushButton::clicked, this, [this]() { uploadImage(true); });
    bl->addWidget(bup, 0, Qt::AlignLeft);
    l->addWidget(brow);
    l->addStretch(1);
}

void ProfileWidget::uploadImage(bool banner) {
    const QString path = QFileDialog::getOpenFileName(this, banner ? "Scegli il banner" : "Scegli la foto profilo",
        QString(), "Immagini (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;
    const QString dataUrl = banner ? imageToDataUrl(path, 854, 480) : imageToDataUrl(path, 120, 120);
    if (dataUrl.isEmpty()) { showMessage("Impossibile leggere l'immagine.", false); return; }

    const QString field = banner ? "banner" : "photoBase64";
    saveField(field, dataUrl, banner ? "Banner aggiornato." : "Foto profilo aggiornata.");
    if (banner) {
        QPixmap pm = pixmapFromDataUrl(dataUrl);
        m_bannerPreview->setPixmap(pm.scaled(m_bannerPreview->width() > 0 ? m_bannerPreview->width() : 600, 150,
                                             Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        m_photo = pixmapFromDataUrl(dataUrl);
        updateAvatars();
    }
}

// ── Impostazioni profilo ─────────────────────────────────────────────────
void ProfileWidget::buildProfileSettings() {
    auto* l = addSection("Impostazioni profilo", "Impostazioni profilo");

    l->addWidget(makeLabel("USERNAME PUBBLICO", "Section"));
    auto* urow = makeRow();
    auto* ul = new QHBoxLayout(urow);
    ul->setContentsMargins(16, 12, 16, 12);
    ul->setSpacing(8);
    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText("Nuovo username");
    m_usernameEdit->setMaxLength(30);
    ul->addWidget(m_usernameEdit, 1);
    auto* save = makeButton("Salva");
    connect(save, &QPushButton::clicked, this, [this]() {
        const QString name = m_usernameEdit->text().trimmed();
        if (name.isEmpty()) return;
        m_auth->updateDisplayName(name, [this, name](bool ok, const QString& err) {
            if (!ok) { showMessage(err, false); return; }
            m_fs->mergeFields("users/" + m_auth->currentUser().uid, QJsonObject{{"displayName", name}});
            m_nameLabel->setText(name);
            showMessage("Username aggiornato.");
        });
    });
    ul->addWidget(save);
    l->addWidget(urow);

    l->addWidget(makeLabel("BIO", "Section"));
    auto* brow = makeRow();
    auto* bl = new QVBoxLayout(brow);
    bl->setContentsMargins(16, 12, 16, 12);
    bl->setSpacing(8);
    m_bioEdit = new QPlainTextEdit;
    m_bioEdit->setPlaceholderText("Scrivi qualcosa su di te...");
    m_bioEdit->setFixedHeight(100);
    bl->addWidget(m_bioEdit);
    auto* saveBio = makeButton("Aggiorna bio");
    connect(saveBio, &QPushButton::clicked, this, [this]() { saveField("bio", m_bioEdit->toPlainText(), "Bio salvata."); });
    bl->addWidget(saveBio, 0, Qt::AlignLeft);
    l->addWidget(brow);

    l->addWidget(makeLabel("PAESE", "Section"));
    auto* crow = makeRow();
    auto* cl = new QHBoxLayout(crow);
    cl->setContentsMargins(16, 12, 16, 12);
    m_countryBox = new QComboBox;
    m_countryBox->addItem("Seleziona paese", "");
    m_countryBox->addItem("Italia", "IT");
    m_countryBox->addItem("USA", "US");
    m_countryBox->addItem("Germania", "DE");
    m_countryBox->addItem("Francia", "FR");
    m_countryBox->addItem("Spagna", "ES");
    connect(m_countryBox, &QComboBox::activated, this, [this](int i) {
        saveField("country", m_countryBox->itemData(i).toString(), "Paese salvato.");
    });
    cl->addWidget(m_countryBox, 1);
    l->addWidget(crow);
    l->addStretch(1);
}

// ── Sicurezza e privacy ──────────────────────────────────────────────────
void ProfileWidget::buildSecurity() {
    auto* l = addSection("Sicurezza e privacy", "Sicurezza e privacy");

    auto* prow = makeRow();
    auto* pl = new QHBoxLayout(prow);
    pl->setContentsMargins(16, 12, 16, 12);
    pl->setSpacing(12);
    auto* ptexts = new QVBoxLayout;
    ptexts->setSpacing(2);
    ptexts->addWidget(makeLabel("Profilo pubblico", "RowTitle"));
    m_publicText = makeLabel("Privato", "Muted");
    ptexts->addWidget(m_publicText);
    pl->addLayout(ptexts, 1);
    m_publicSwitch = new ToggleSwitch;
    connect(m_publicSwitch, &QAbstractButton::clicked, this, [this](bool on) {
        m_publicText->setText(on ? "Pubblico" : "Privato");
        saveField("profilePublic", on, on ? "Il profilo ora è pubblico." : "Il profilo ora è privato.");
    });
    pl->addWidget(m_publicSwitch);
    l->addWidget(prow);

    auto* grow = makeRow();
    auto* gl = new QHBoxLayout(grow);
    gl->setContentsMargins(16, 12, 12, 12);
    auto* gtexts = new QVBoxLayout;
    gtexts->setSpacing(2);
    gtexts->addWidget(makeLabel("Collega account Google", "RowTitle"));
    gtexts->addWidget(makeLabel("Si apre il sito: il collegamento Google richiede il browser.", "Muted"));
    gl->addLayout(gtexts, 1);
    auto* gbtn = makeButton("Apri il sito");
    connect(gbtn, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(QUrl("https://eta-games.github.io/profilo.html")); });
    gl->addWidget(gbtn);
    l->addWidget(grow);

    l->addWidget(makeLabel("CAMBIO EMAIL", "Section"));
    auto* erow = makeRow();
    auto* el = new QHBoxLayout(erow);
    el->setContentsMargins(16, 12, 16, 12);
    el->setSpacing(8);
    m_newEmail = new QLineEdit;
    m_newEmail->setPlaceholderText("Nuova email");
    el->addWidget(m_newEmail, 1);
    auto* ebtn = makeButton("Aggiorna email");
    connect(ebtn, &QPushButton::clicked, this, [this]() {
        const QString v = m_newEmail->text().trimmed();
        if (v.isEmpty()) return;
        m_auth->updateEmail(v, [this](bool ok, const QString& err) {
            if (ok) m_newEmail->clear();
            showMessage(ok ? "Email aggiornata." : err, ok);
        });
    });
    el->addWidget(ebtn);
    l->addWidget(erow);

    l->addWidget(makeLabel("CAMBIO PASSWORD", "Section"));
    auto* wrow = makeRow();
    auto* wl = new QHBoxLayout(wrow);
    wl->setContentsMargins(16, 12, 16, 12);
    wl->setSpacing(8);
    m_newPassword = new QLineEdit;
    m_newPassword->setEchoMode(QLineEdit::Password);
    m_newPassword->setPlaceholderText("Nuova password");
    wl->addWidget(m_newPassword, 1);
    auto* wbtn = makeButton("Aggiorna password");
    connect(wbtn, &QPushButton::clicked, this, [this]() {
        const QString v = m_newPassword->text();
        if (v.isEmpty()) return;
        m_auth->updatePassword(v, [this](bool ok, const QString& err) {
            if (ok) m_newPassword->clear();
            showMessage(ok ? "Password aggiornata." : err, ok);
        });
    });
    wl->addWidget(wbtn);
    l->addWidget(wrow);

    l->addWidget(makeLabel("ZONA PERICOLOSA", "Section"));
    auto* drow = new QFrame;
    drow->setObjectName("DangerRow");
    auto* dl = new QHBoxLayout(drow);
    dl->setContentsMargins(16, 12, 12, 12);
    auto* dtexts = new QVBoxLayout;
    dtexts->setSpacing(2);
    dtexts->addWidget(makeLabel("Elimina account", "RowTitle"));
    dtexts->addWidget(makeLabel("Cancella definitivamente l'account ETA Games.", "Muted"));
    dl->addLayout(dtexts, 1);
    auto* del = makeButton("Elimina", "Danger");
    del->setMinimumWidth(104);
    connect(del, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Elimina account", "Eliminare definitivamente l'account? L'operazione non si può annullare.",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        m_auth->deleteAccount([this](bool ok, const QString& err) {
            if (!ok) { showMessage(err, false); return; }
            m_auth->signOut();
            m_loadedUid.clear();
            emit loggedOut();
        });
    });
    dl->addWidget(del);
    l->addWidget(drow);
    l->addStretch(1);
}

// ── Funzioni extra ───────────────────────────────────────────────────────
void ProfileWidget::buildExtra() {
    auto* l = addSection("Funzioni extra", "Funzioni extra");

    auto* brow = makeRow();
    auto* bl = new QHBoxLayout(brow);
    bl->setContentsMargins(16, 12, 16, 12);
    bl->addWidget(makeLabel("Badge utente", "RowTitle"), 1);
    m_badgeLabel = makeLabel("…", "Online");
    bl->addWidget(m_badgeLabel);
    l->addWidget(brow);

    auto* trow = makeRow();
    auto* tl = new QVBoxLayout(trow);
    tl->setContentsMargins(16, 12, 16, 12);
    tl->setSpacing(6);
    tl->addWidget(makeLabel("Tema interfaccia", "RowTitle"));
    tl->addWidget(makeLabel("Il tema scelto qui viene salvato sul tuo account, come sul sito.", "Muted"));
    m_themeBox = new QComboBox;
    for (const auto& t : ThemeManager::themes()) m_themeBox->addItem(t.label, t.id);
    connect(m_themeBox, &QComboBox::activated, this, [this](int i) {
        const QString id = m_themeBox->itemData(i).toString();
        emit themeSelected(id);
        syncThemeToCloud(id);
    });
    tl->addWidget(m_themeBox);
    l->addWidget(trow);
    l->addStretch(1);
}

// ── La mia libreria ──────────────────────────────────────────────────────
void ProfileWidget::buildLibrary() {
    auto* l = addSection("La mia libreria", "La mia libreria");
    l->addWidget(makeLabel("Giochi salvati sul tuo account. Installali o avviali da qui.", "Muted"));
    auto* host = new QVBoxLayout;
    host->setSpacing(8);
    m_libraryList = host;
    l->addLayout(host);
    l->addStretch(1);
}

void ProfileWidget::rebuildLibrary(const QStringList& ids) {
    while (QLayoutItem* it = m_libraryList->takeAt(0)) {
        if (QWidget* w = it->widget()) w->deleteLater();
        delete it;
    }
    bool any = false;
    for (const QString& id : ids) {
        const GameEntry* g = findGame(id);
        if (!g) continue;
        any = true;
        auto* row = makeRow();
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 12, 12);
        auto* texts = new QVBoxLayout;
        texts->setSpacing(2);
        texts->addWidget(makeLabel(g->title, "RowTitle"));
        texts->addWidget(makeLabel(QString("%1 · v%2").arg(g->engine, g->version), "Muted"));
        rl->addLayout(texts, 1);
        const bool installed = Config::instance().games().contains(id);
        auto* btn = makeButton(installed ? "Gioca" : "Installa", nullptr);
        btn->setMinimumWidth(104);
        const QString gid = id;
        connect(btn, &QPushButton::clicked, this, [this, gid]() { emit gameActionRequested(gid); });
        rl->addWidget(btn);
        m_libraryList->addWidget(row);
    }
    if (!any) m_libraryList->addWidget(makeLabel("Non hai ancora aggiunto giochi alla libreria dal sito.", "Muted"));
}

// ── Dati ─────────────────────────────────────────────────────────────────
void ProfileWidget::refresh() {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) { m_stack->setCurrentIndex(0); return; }
    m_stack->setCurrentIndex(1);
    const QString display = u.displayName.isEmpty() ? u.email : u.displayName;
    m_nameLabel->setText(display.isEmpty() ? "Utente ETA Games" : display);
    m_emailLabel->setText(u.email);
    m_usernameEdit->setPlaceholderText(u.displayName.isEmpty() ? "Nuovo username" : u.displayName);
    updateAvatars();
    if (m_loadedUid != u.uid || !m_lastLoad.isValid() || m_lastLoad.elapsed() > 60 * 1000) reload(false);
}

void ProfileWidget::reload(bool force) {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    if (!force && m_loadedUid == u.uid && m_lastLoad.isValid() && m_lastLoad.elapsed() < 60 * 1000) return;
    const bool firstLoadForUser = (m_loadedUid != u.uid);
    m_loadedUid = u.uid;
    m_lastLoad.start();
    if (firstLoadForUser) { m_photo = QPixmap(); m_bannerPreview->clear(); }

    m_auth->accountLookup([this](bool ok, const QJsonObject& user, const QString&) { if (ok) applyLookup(user); });
    m_fs->getDocument("users/" + u.uid, [this, firstLoadForUser](bool ok, const QJsonObject& data, const QString& err) {
        if (!ok) { showMessage("Dati del profilo non disponibili: " + err, false); return; }
        applyCloudData(data);
        // come sul sito, al primo caricamento si applica il tema salvato sull'account
        const QString theme = data.value("theme").toString();
        if (firstLoadForUser && !theme.isEmpty()) emit themeSelected(ThemeManager::normalizeThemeId(theme));
    });
}

void ProfileWidget::applyLookup(const QJsonObject& user) {
    const QDateTime created = QDateTime::fromMSecsSinceEpoch(user.value("createdAt").toString().toLongLong());
    const QDateTime last = QDateTime::fromMSecsSinceEpoch(user.value("lastLoginAt").toString().toLongLong());
    m_createdLabel->setText(created.toLocalTime().toString("dd/MM/yyyy"));
    m_lastLoginLabel->setText(last.toLocalTime().toString("dd/MM/yyyy"));
    const qint64 days = created.date().daysTo(QDate::currentDate());
    m_daysLabel->setText(QString::number(days));
    m_badgeLabel->setText(badgeForDays(days));
}

void ProfileWidget::applyCloudData(const QJsonObject& data) {
    if (data.contains("photoBase64")) m_photo = pixmapFromDataUrl(data.value("photoBase64").toString());
    updateAvatars();
    if (data.contains("banner")) {
        QPixmap pm = pixmapFromDataUrl(data.value("banner").toString());
        if (!pm.isNull()) m_bannerPreview->setPixmap(pm.scaled(600, 150, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        m_bannerPreview->setText("Nessun banner");
    }
    m_bioEdit->setPlainText(data.value("bio").toString());
    const int ci = m_countryBox->findData(data.value("country").toString());
    m_countryBox->setCurrentIndex(ci < 0 ? 0 : ci);
    const bool pub = data.value("profilePublic").toBool(false);
    m_publicSwitch->setChecked(pub);
    m_publicText->setText(pub ? "Pubblico" : "Privato");
    const int ti = m_themeBox->findData(ThemeManager::normalizeThemeId(data.value("theme").toString()));
    if (data.contains("theme") && ti >= 0) m_themeBox->setCurrentIndex(ti);

    QStringList lib;
    for (const auto& v : data.value("library").toArray()) lib << v.toString();
    rebuildLibrary(lib);
}

void ProfileWidget::updateAvatars() {
    const AuthUser u = m_auth->currentUser();
    const QString display = u.displayName.isEmpty() ? u.email : u.displayName;
    const QString initial = display.isEmpty() ? "?" : display.left(1).toUpper();
    m_overviewAvatar->setPixmap(makeAvatar(m_photo, initial, 72));
    m_avatarPreview->setPixmap(makeAvatar(m_photo, initial, 90));
}

void ProfileWidget::syncThemeToCloud(const QString& themeId) {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    QString value = themeId;
    if (value.endsWith("-theme")) value.chop(6); // il sito usa "night", "TVH", "matrix", ...
    m_fs->mergeFields("users/" + u.uid, QJsonObject{{"theme", value}});
    const int ti = m_themeBox->findData(ThemeManager::normalizeThemeId(themeId));
    if (ti >= 0) m_themeBox->setCurrentIndex(ti);
    updateAvatars(); // l'anello dell'avatar segue il colore del tema
}

void ProfileWidget::setOnlineStatusText(const QString& text) {
    m_onlineText = text;
    m_statusLine->setText(text);
    m_statusLine->setVisible(!text.isEmpty());
}

void ProfileWidget::saveField(const QString& field, const QJsonValue& value, const QString& okText) {
    const AuthUser u = m_auth->currentUser();
    if (!u.isValid()) return;
    QJsonObject f;
    f[field] = value;
    m_fs->mergeFields("users/" + u.uid, f, [this, okText](bool ok, const QJsonObject&, const QString& err) {
        showMessage(ok ? okText : ("Salvataggio non riuscito: " + err), ok);
    });
}

void ProfileWidget::showMessage(const QString& text, bool ok) {
    m_message->setStyleSheet(ok ? QString() : "color: #ff5566;");
    m_message->setText(text);
    m_messageTimer->start(6000);
}
