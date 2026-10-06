#pragma once
#include <QString>

// Traduzioni leggere: il testo di partenza è l'ITALIANO (così il codice resta leggibile),
// la tabella in Translations.cpp associa a ogni frase italiana la versione inglese e tedesca.
// Una frase senza traduzione viene mostrata in italiano. Il cambio lingua vale al riavvio.
namespace I18n {
enum Lang { It = 0, En = 1, De = 2 };

Lang current();
void setCurrent(Lang lang);
QString code(Lang lang);          // "it" | "en" | "de"
QString displayName(Lang lang);   // "Italiano" | "English" | "Deutsch"

QString tr(const QString& italian);
} // namespace I18n

// Scorciatoia: T("Testo italiano")   e   T("Aggiorno %1…").arg(nome)
inline QString T(const char* italian) { return I18n::tr(QString::fromUtf8(italian)); }
