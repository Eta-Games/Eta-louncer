#include "ThemeManager.h"
#include <QColor>

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
    bool dark = bg.lightness() < 128;
    QString text = dark ? "#ffffff" : "#111111";
    QString muted = dark ? "#9a9a9a" : "#555555";
    QString card = dark ? bg.lighter(135).name() : bg.darker(105).name();
    QString card2 = dark ? bg.lighter(160).name() : bg.darker(110).name();
    QString border = dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.10)";

    // Font coerenti con il sito (Rajdhani per i titoli/brand, Inter per il corpo).
    // Se non installati nel sistema, Qt ricade automaticamente sul fallback.
    return QString(R"(
        QWidget { background-color: %1; color: %2; font-family: "Inter", "Segoe UI", sans-serif; font-size: 10.5pt; }
        QMainWindow, #TitleBar { background-color: %1; }
        #TitleBar { border-bottom: 1px solid %5; }

        /* Navbar in stile eta-games.github.io: logo + brand a sinistra, voci a destra */
        #SiteNav { background-color: %3; border-bottom: 1px solid %5; }
        #Brand {
            font-family: "Rajdhani", "Segoe UI", sans-serif;
            font-size: 16pt; font-weight: 700; color: %2;
        }
        QPushButton#NavLink {
            background-color: transparent; color: %4; border: none;
            border-bottom: 2px solid transparent; border-radius: 0;
            padding: 6px 10px; font-weight: 600;
        }
        QPushButton#NavLink:hover { color: %2; }
        QPushButton#NavLink:checked { color: %2; border-bottom: 2px solid %6; }

        /* Titoli di sezione (Negozio, Libreria, Profilo...) */
        QLabel#Heading {
            font-family: "Rajdhani", "Segoe UI", sans-serif;
            font-size: 20pt; font-weight: 700; color: %2;
        }

        QFrame#Card { background-color: %3; border: 1px solid %5; border-radius: 10px; }
        QLabel#Muted { color: %4; }
        QLabel#Avatar {
            background-color: %6; color: #ffffff; border-radius: 42px;
            font-size: 22pt; font-weight: 700;
        }
        QPushButton {
            background-color: %6; color: #ffffff; border: none;
            border-radius: 8px; padding: 8px 16px; font-weight: 600;
        }
        QPushButton:hover { background-color: %6; }
        QPushButton:disabled { background-color: %7; color: %4; }
        QPushButton#Secondary { background-color: %3; color: %2; border: 1px solid %5; }
        QProgressBar {
            background-color: %7; border: none; border-radius: 6px; height: 14px; text-align: center; color: %2;
        }
        QProgressBar::chunk { background-color: %6; border-radius: 6px; }
        QLineEdit, QComboBox {
            background-color: %8; color: %2; border: 1px solid %5; border-radius: 6px; padding: 6px 8px;
        }
        QListWidget { background-color: transparent; border: none; }
    )").arg(bg.name(), text, card, muted, border, def->accent, card2, dark ? "#000000" : "#ffffff");
}
