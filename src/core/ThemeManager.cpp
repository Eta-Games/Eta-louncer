#include "ThemeManager.h"
#include <QColor>
#include <QPair>

const QList<ThemeDef>& ThemeManager::themes() {
    static const QList<ThemeDef> list = {
        {"night-theme",    "Night",        "#111111", "#e50914"},
        {"TVH-theme",      "Valter House", "#1f0003", "#ff4d6d"},
        {"matrix-theme",   "Matrix",       "#000000", "#00ff00"},
        {"hight-theme",    "Hi-Contrast",  "#000000", "#ff8800"},
        {"inverted-theme", "Inverted",     "#ffffff", "#1af6eb"},
        {"day-theme",      "Day",          "#f5f5f5", "#e50914"},
    };
    return list;
}

QString ThemeManager::normalizeThemeId(const QString& t) {
    if (t.endsWith("-theme")) return t;
    return (t.isEmpty() ? "night" : t) + "-theme";
}

QString ThemeManager::stylesheetFor(const QString& themeId) {
    const ThemeDef* def = nullptr;
    for (const auto& t : themes()) if (t.id == themeId) { def = &t; break; }
    if (!def) def = &themes().first();

    QColor bg(def->bg);
    QColor accent(def->accent);
    bool dark = bg.lightness() < 128;

    auto rgba = [](const QColor& c, double a) {
        return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };

    const QString text     = dark ? "#ffffff" : "#111111";
    const QString muted    = dark ? "#9a9a9a" : "#555555";
    const QString card     = dark ? bg.lighter(135).name() : bg.darker(105).name();
    const QString card2    = dark ? bg.lighter(165).name() : bg.darker(112).name();
    const QString border   = dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.10)";
    const QString border2  = dark ? "rgba(255,255,255,0.18)" : "rgba(0,0,0,0.20)";
    const QString input    = dark ? "#000000" : "#ffffff";
    // testo leggibile sopra il colore accent (es. tema Inverted con accent ciano chiaro)
    const QString onAccent = accent.lightness() > 150 ? "#111111" : "#ffffff";
    const QString toggleOff = dark ? "#4a4a4a" : "#bdbdbd";
    const QString accentH  = accent.lighter(115).name();
    const QString accentP  = accent.darker(120).name();

    QString css = QStringLiteral(R"(
        QWidget { background-color: @BG@; color: @TEXT@; font-family: "Inter", "Segoe UI", sans-serif; font-size: 10pt; }
        QLabel { background: transparent; }
        QMainWindow, #TitleBar { background-color: @BG@; }
        #TitleBar { border-bottom: 1px solid @BORDER@; }
        QToolTip { background-color: @CARD2@; color: @TEXT@; border: 1px solid @BORDER@; padding: 4px 8px; }

        /* Sfondo delle pagine: bagliore dell'accent in alto, come l'hero del sito */
        #Page {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 @GLOW@, stop:0.40 @BG@, stop:1 @BG@);
        }

        /* Navbar in stile eta-games.github.io */
        #SiteNav { background-color: @CARD@; border-bottom: 1px solid @BORDER@; }
        #Brand { font-family: "Rajdhani", "Segoe UI", sans-serif; font-size: 18pt; font-weight: 700; color: @TEXT@; }
        QPushButton#NavLink {
            background-color: transparent; color: @MUTED@; border: none;
            border-bottom: 3px solid transparent; border-radius: 0;
            padding: 8px 14px; font-family: "Rajdhani", "Segoe UI", sans-serif;
            font-size: 13pt; font-weight: 700;
        }
        QPushButton#NavLink:hover { color: @TEXT@; background-color: transparent; }
        QPushButton#NavLink:checked { color: @TEXT@; border-bottom: 3px solid @ACCENT@; background-color: transparent; }

        /* Bottoni della barra titolo: padding 0 altrimenti il glifo non entra e sparisce */
        QPushButton#TitleBtn, QPushButton#CloseBtn {
            background-color: transparent; color: @TEXT@; border: none; border-radius: 4px;
            padding: 0px; margin: 0px; font-family: "Segoe UI Symbol", "Segoe UI", sans-serif;
            font-size: 11pt; font-weight: 400;
        }
        QPushButton#TitleBtn:hover, QPushButton#CloseBtn:hover { background-color: @CARD2@; }
        QPushButton#CloseBtn:hover { background-color: #e81123; color: #ffffff; }

        QLabel#Heading { font-family: "Rajdhani", "Segoe UI", sans-serif; font-size: 24pt; font-weight: 700; color: @TEXT@; }
        QLabel#CardTitle { font-family: "Rajdhani", "Segoe UI", sans-serif; font-size: 16pt; font-weight: 700; }
        QLabel#Muted { color: @MUTED@; }
        QLabel#Online { color: #3ddc84; font-weight: 600; }
        QLabel#OnlinePill { color: #3ddc84; border: 1px solid #3ddc84; border-radius: 10px; padding: 3px 10px; font-weight: 600; }
        QFrame#BcCard { background-color: @CARD@; border: 1px solid @BORDER@; border-left: 4px solid @ACCENT@; border-radius: 10px; }
        QFrame#BcCard[level="warning"] { border-left: 4px solid #ffb020; }
        QFrame#BcCard[level="update"] { border-left: 4px solid #3ddc84; }
        QFrame#BcCard[unread="true"] { border-top: 1px solid @ACCENT@; border-right: 1px solid @ACCENT@; border-bottom: 1px solid @ACCENT@; }
        QLabel#RowTitle { font-weight: 600; }
        QLabel#Section { color: @MUTED@; font-family: "Rajdhani", "Segoe UI", sans-serif; font-size: 11pt; font-weight: 700; padding-top: 10px; }
        QLabel#Tag { border: 1px solid @ACCENT@; color: @ACCENT@; border-radius: 6px; padding: 1px 8px; font-size: 9pt; font-weight: 600; }
        QLabel#Banner { background-color: @CARD2@; border: none; }

        /* Gestione gioco: righe come le card del sito */
        QFrame#Row { background-color: @CARD@; border: 1px solid @BORDER@; border-radius: 10px; }
        QFrame#Row:hover { border: 1px solid @BORDER2@; }
        QFrame#DangerRow { background-color: rgba(229,48,60,0.08); border: 1px solid rgba(229,48,60,0.45); border-radius: 10px; }
        QPushButton#Danger { background-color: #e5303c; color: #ffffff; }
        QPushButton#Danger:hover { background-color: #ff4655; }
        QPushButton#Danger:pressed { background-color: #b9202b; }

        /* Impostazioni: barra laterale e interruttori */
        QFrame#SideNav { background-color: @CARD@; border: none; border-right: 1px solid @BORDER@; }
        QPushButton#SideLink {
            background-color: transparent; color: @MUTED@; border: none; border-left: 3px solid transparent;
            border-radius: 0; text-align: left; padding: 11px 20px; font-weight: 600;
        }
        QPushButton#SideLink:hover { color: @TEXT@; background-color: transparent; }
        QPushButton#SideLink:checked { color: @TEXT@; border-left: 3px solid @ACCENT@; background-color: @GLOW@; }
        ToggleSwitch { qproperty-onColor: @ACCENT@; qproperty-offColor: @TOGGLE_OFF@; qproperty-knobColor: #ffffff; }

        /* Filtro Tutti / Installati */
        QPushButton#Seg { background-color: transparent; color: @MUTED@; border: 1px solid @BORDER2@; border-radius: 8px; padding: 6px 16px; }
        QPushButton#Seg:hover:!checked { color: @TEXT@; border: 1px solid @ACCENT@; background-color: transparent; }
        QPushButton#Seg:checked { background-color: @ACCENT@; color: @ONACCENT@; border: 1px solid @ACCENT@; }

        /* Card dei giochi / pannello login */
        QFrame#Card, QFrame#Panel { background-color: @CARD@; border: 1px solid @BORDER@; border-radius: 12px; }
        QFrame#Card:hover { border: 1px solid @ACCENT@; }
        QWidget#CardBody { background: transparent; }
        QLabel#Cover { background-color: @CARD2@; border: none;
                       border-top-left-radius: 12px; border-top-right-radius: 12px; }
        QLabel#Badge { background-color: #ffc400; color: #111111; border-radius: 6px; padding: 2px 8px; font-weight: 600; }
        QLabel#Avatar { background-color: @ACCENT@; color: @ONACCENT@; border-radius: 42px; font-size: 22pt; font-weight: 700; }

        QPushButton {
            background-color: @ACCENT@; color: @ONACCENT@; border: none;
            border-radius: 8px; padding: 9px 18px; font-weight: 600;
        }
        QPushButton:hover { background-color: @ACCENT_H@; }
        QPushButton:pressed { background-color: @ACCENT_P@; }
        QPushButton:disabled { background-color: @CARD2@; color: @MUTED@; }
        QPushButton#Secondary { background-color: transparent; color: @TEXT@; border: 1px solid @BORDER2@; }
        QPushButton#Secondary:hover { background-color: @CARD2@; border: 1px solid @ACCENT@; }
        QPushButton#Secondary:pressed { background-color: @CARD@; }
        QPushButton#Secondary:disabled { background-color: transparent; color: @MUTED@; border: 1px solid @BORDER@; }

        QProgressBar {
            background-color: @CARD2@; border: none; border-radius: 7px; min-height: 14px; max-height: 14px;
            text-align: center; color: @TEXT@;
        }
        QProgressBar::chunk { background-color: @ACCENT@; border-radius: 7px; }

        QLineEdit, QComboBox, QPlainTextEdit {
            background-color: @INPUT@; color: @TEXT@; border: 1px solid @BORDER2@; border-radius: 8px;
            padding: 9px 12px; selection-background-color: @ACCENT@; selection-color: @ONACCENT@;
        }
        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus { border: 1px solid @ACCENT@; }
        QComboBox QAbstractItemView { background-color: @CARD@; color: @TEXT@; border: 1px solid @BORDER2@;
                                      selection-background-color: @ACCENT@; selection-color: @ONACCENT@; outline: none; }
        QListWidget { background-color: transparent; border: none; }

        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
        QScrollBar::handle:vertical { background: @BORDER2@; border-radius: 4px; min-height: 36px; }
        QScrollBar::handle:vertical:hover { background: @ACCENT@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

        QDialog, QMessageBox { background-color: @BG@; }
    )");

    const QList<QPair<QString, QString>> tokens = {
        {"@BG@", bg.name()},           {"@TEXT@", text},       {"@MUTED@", muted},
        {"@CARD@", card},              {"@CARD2@", card2},     {"@BORDER@", border},
        {"@BORDER2@", border2},        {"@INPUT@", input},     {"@ACCENT@", accent.name()},
        {"@ACCENT_H@", accentH},       {"@ACCENT_P@", accentP},{"@ONACCENT@", onAccent},
        {"@GLOW@", rgba(accent, dark ? 0.22 : 0.14)}, {"@TOGGLE_OFF@", toggleOff},
    };
    for (const auto& t : tokens) css.replace(t.first, t.second);
    return css;
}
