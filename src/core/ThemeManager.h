#pragma once
#include <QString>
#include <QList>

struct ThemeDef {
    QString id;
    QString label;
    QString bg;
    QString accent;
};

namespace ThemeManager {
    const QList<ThemeDef>& themes();
    QString normalizeThemeId(const QString& t);
    // Genera il QSS applicato a QApplication/MainWindow per il tema scelto
    QString stylesheetFor(const QString& themeId);
}
