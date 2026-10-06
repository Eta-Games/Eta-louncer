#include "I18n.h"
#include <QHash>
#include <QLocale>
#include <QSettings>

struct TrEntry { const char* it; const char* en; const char* de; };
extern const TrEntry kTranslations[];
extern const int kTranslationCount;

namespace I18n {

static Lang s_lang = It;
static bool s_loaded = false;

static void load() {
    if (s_loaded) return;
    s_loaded = true;
    const QString saved = QSettings().value("ui/lang").toString();
    if (saved == "en") s_lang = En;
    else if (saved == "de") s_lang = De;
    else if (saved == "it") s_lang = It;
    else { // prima volta: lingua del sistema (it/en/de), altrimenti italiano
        const QString sys = QLocale::system().name().section('_', 0, 0);
        s_lang = sys == "en" ? En : sys == "de" ? De : It;
    }
}

Lang current() { load(); return s_lang; }

void setCurrent(Lang lang) {
    load();
    s_lang = lang;
    QSettings().setValue("ui/lang", code(lang));
}

QString code(Lang l) { return l == En ? "en" : l == De ? "de" : "it"; }
QString displayName(Lang l) { return l == En ? "English" : l == De ? "Deutsch" : "Italiano"; }

QString tr(const QString& italian) {
    load();
    if (s_lang == It) return italian;
    static QHash<QString, const TrEntry*> index;
    if (index.isEmpty())
        for (int i = 0; i < kTranslationCount; ++i) index.insert(QString::fromUtf8(kTranslations[i].it), &kTranslations[i]);
    const TrEntry* e = index.value(italian, nullptr);
    if (!e) return italian;
    const char* out = (s_lang == En) ? e->en : e->de;
    return (out && *out) ? QString::fromUtf8(out) : italian;
}

} // namespace I18n
