/*
 * Copyright (C) 2026 LingmoOS Team.
 *
 * Author:     devalexandre <alexandre@dev2learn.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "converter.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace Converter {

namespace {

enum Dimension { Length, Mass, Temperature, Volume, Area, Speed, Data, Time };

// value in the base unit = value * factor + offset (base: m, kg, K, L, m², m/s, byte, s)
// aliases are folded (lower case, no accents) and separated by '|'
struct Unit {
    Dimension dim;
    double factor;
    double offset;
    const char *symbol;
    const char *aliases;
};

// Symbols that read better as words are translated at display time (context "Converter")
const Unit kUnits[] = {
    {Length, 0.001, 0, "mm", "mm|milimetro|milimetros|millimeter|millimeters|millimetre|millimetres"},
    {Length, 0.01, 0, "cm", "cm|centimetro|centimetros|centimeter|centimeters|centimetre|centimetres"},
    {Length, 1, 0, "m", "m|mt|mts|metro|metros|meter|meters|metre|metres"},
    {Length, 1000, 0, "km", "km|kms|quilometro|quilometros|kilometro|kilometros|kilometer|kilometers|kilometre|kilometres"},
    {Length, 0.0254, 0, "in", "in|inch|inches|pol|polegada|polegadas|\""},
    {Length, 0.3048, 0, "ft", "ft|foot|feet|pe|pes|'"},
    {Length, 0.9144, 0, "yd", "yd|yds|yard|yards|jarda|jardas"},
    {Length, 1609.344, 0, "mi", "mi|mile|miles|milha|milhas"},
    {Length, 1852, 0, "nmi", "nmi|milha nautica|milhas nauticas|nautical mile|nautical miles"},

    {Mass, 1e-6, 0, "mg", "mg|miligrama|miligramas|milligram|milligrams"},
    {Mass, 0.001, 0, "g", "g|gr|grama|gramas|gram|grams|gramme|grammes"},
    {Mass, 1, 0, "kg", "kg|kgs|quilo|quilos|kilo|kilos|quilograma|quilogramas|kilograma|kilogramas|kilogram|kilograms"},
    {Mass, 1000, 0, "t", "t|ton|tons|tonelada|toneladas|tonne|tonnes"},
    {Mass, 0.028349523125, 0, "oz", "oz|ounce|ounces|onca|oncas"},
    {Mass, 0.45359237, 0, "lb", "lb|lbs|pound|pounds|libra|libras"},
    {Mass, 6.35029318, 0, "st", "st|stone|stones"},

    {Temperature, 1, 273.15, "°C", "c|°c|ºc|celsius|grau celsius|graus celsius|centigrado|centigrados"},
    {Temperature, 5.0 / 9.0, 273.15 - 32 * 5.0 / 9.0, "°F", "f|°f|ºf|fahrenheit|grau fahrenheit|graus fahrenheit"},
    {Temperature, 1, 0, "K", "k|kelvin|kelvins"},

    {Volume, 0.001, 0, "mL", "ml|mililitro|mililitros|milliliter|milliliters|millilitre|millilitres"},
    {Volume, 1, 0, "L", "l|lt|lts|litro|litros|liter|liters|litre|litres"},
    {Volume, 0.001, 0, "cm³", "cm3|cm³|cc|centimetro cubico|centimetros cubicos"},
    {Volume, 1000, 0, "m³", "m3|m³|metro cubico|metros cubicos|cubic meter|cubic meters"},
    {Volume, 3.785411784, 0, "gal", "gal|gallon|gallons|galao|galoes"},
    {Volume, 0.946352946, 0, "qt", "qt|quart|quarts"},
    {Volume, 0.473176473, 0, "pt", "pint|pints|quartilho|quartilhos"},
    {Volume, 0.2365882365, 0, QT_TRANSLATE_NOOP("Converter", "cups"), "cup|cups|xicara|xicaras"},
    {Volume, 0.0295735295625, 0, "fl oz", "floz|fl oz|fluid ounce|fluid ounces|onca fluida|oncas fluidas"},
    {Volume, 0.01478676478125, 0, QT_TRANSLATE_NOOP("Converter", "tbsp"), "tbsp|colher de sopa|colheres de sopa|tablespoon|tablespoons"},
    {Volume, 0.00492892159375, 0, QT_TRANSLATE_NOOP("Converter", "tsp"), "tsp|colher de cha|colheres de cha|teaspoon|teaspoons"},

    {Area, 1e-4, 0, "cm²", "cm2|cm²|cm^2|centimetro quadrado|centimetros quadrados|square centimeter|square centimeters"},
    {Area, 1, 0, "m²", "m2|m²|m^2|sqm|sq m|metro quadrado|metros quadrados|square meter|square meters|square metre|square metres"},
    {Area, 1e6, 0, "km²", "km2|km²|km^2|quilometro quadrado|quilometros quadrados|square kilometer|square kilometers"},
    {Area, 1e4, 0, "ha", "ha|hectare|hectares"},
    {Area, 4046.8564224, 0, "ac", "ac|acre|acres"},
    {Area, 6.4516e-4, 0, "in²", "in2|in²|sq in|square inch|square inches|polegada quadrada|polegadas quadradas"},
    {Area, 0.09290304, 0, "ft²", "ft2|ft²|sqft|sq ft|square foot|square feet|pe quadrado|pes quadrados"},
    {Area, 2589988.110336, 0, "mi²", "mi2|mi²|sq mi|square mile|square miles|milha quadrada|milhas quadradas"},
    {Area, 24200, 0, QT_TRANSLATE_NOOP("Converter", "alqueires"), "alqueire|alqueires|alqueire paulista|alqueires paulistas"},

    {Speed, 1, 0, "m/s", "m/s|mps|metro por segundo|metros por segundo|meter per second|meters per second"},
    {Speed, 1 / 3.6, 0, "km/h", "km/h|kmh|kph|km por hora|quilometro por hora|quilometros por hora|kilometer per hour|kilometers per hour"},
    {Speed, 0.44704, 0, "mph", "mph|mi/h|milha por hora|milhas por hora|mile per hour|miles per hour"},
    {Speed, 1852.0 / 3600.0, 0, "kn", "kn|kt|kts|knot|knots|no|nos"},
    {Speed, 0.3048, 0, "ft/s", "ft/s|fps|pe por segundo|pes por segundo|feet per second"},

    {Data, 0.125, 0, "bit", "bit|bits"},
    {Data, 125, 0, "kbit", "kbit|kbits|kilobit|kilobits|quilobit|quilobits"},
    {Data, 125e3, 0, "Mbit", "mbit|mbits|megabit|megabits"},
    {Data, 125e6, 0, "Gbit", "gbit|gbits|gigabit|gigabits"},
    {Data, 1, 0, "B", "b|byte|bytes|octeto|octetos"},
    {Data, 1e3, 0, "kB", "kb|kbyte|kbytes|kilobyte|kilobytes|quilobyte|quilobytes"},
    {Data, 1e6, 0, "MB", "mb|mbyte|mbytes|megabyte|megabytes|mega|megas"},
    {Data, 1e9, 0, "GB", "gb|gbyte|gbytes|gigabyte|gigabytes|giga|gigas"},
    {Data, 1e12, 0, "TB", "tb|tbyte|tbytes|terabyte|terabytes|tera|teras"},
    {Data, 1e15, 0, "PB", "pb|petabyte|petabytes"},
    {Data, 1024, 0, "KiB", "kib|kibibyte|kibibytes"},
    {Data, 1048576, 0, "MiB", "mib|mebibyte|mebibytes"},
    {Data, 1073741824, 0, "GiB", "gib|gibibyte|gibibytes"},
    {Data, 1099511627776, 0, "TiB", "tib|tebibyte|tebibytes"},

    {Time, 0.001, 0, "ms", "ms|milissegundo|milissegundos|millisecond|milliseconds"},
    {Time, 1, 0, "s", "s|sec|secs|seg|segs|segundo|segundos|second|seconds"},
    {Time, 60, 0, "min", "min|mins|minuto|minutos|minute|minutes"},
    {Time, 3600, 0, "h", "h|hr|hrs|hora|horas|hour|hours"},
    {Time, 86400, 0, QT_TRANSLATE_NOOP("Converter", "days"), "d|dia|dias|day|days"},
    {Time, 604800, 0, QT_TRANSLATE_NOOP("Converter", "weeks"), "sem|semana|semanas|wk|wks|week|weeks"},
    {Time, 2629746, 0, QT_TRANSLATE_NOOP("Converter", "months"), "mes|meses|month|months"},
    {Time, 31556952, 0, QT_TRANSLATE_NOOP("Converter", "years"), "ano|anos|yr|yrs|year|years"},
};

struct Currency { const char *code; const char *aliases; };
const Currency kCurrencies[] = {
    {"USD", "us$|u$s|$|dolar|dolares|dollar|dollars|dolar americano|dolares americanos|us dollar|us dollars"},
    {"BRL", "r$|real|reais|real brasileiro|reais brasileiros"},
    {"EUR", "€|euro|euros"},
    {"GBP", "£|libra|libras|libra esterlina|libras esterlinas|pound|pounds|pound sterling|pounds sterling"},
    {"JPY", "¥|iene|ienes|yen"},
    {"CNY", "yuan|yuans|renminbi"},
    {"ARS", "peso argentino|pesos argentinos|argentine peso|argentine pesos"},
    {"CLP", "peso chileno|pesos chilenos|chilean peso|chilean pesos"},
    {"MXN", "peso mexicano|pesos mexicanos|mexican peso|mexican pesos"},
    {"COP", "peso colombiano|pesos colombianos|colombian peso|colombian pesos"},
    {"UYU", "peso uruguaio|pesos uruguaios|uruguayan peso|uruguayan pesos"},
    {"PYG", "guarani|guaranis"},
    {"CAD", "dolar canadense|dolares canadenses|canadian dollar|canadian dollars"},
    {"AUD", "dolar australiano|dolares australianos|australian dollar|australian dollars"},
    {"CHF", "franco suico|francos suicos|swiss franc|swiss francs"},
    {"INR", "rupia|rupias|rupee|rupees"},
    {"RUB", "rublo|rublos|ruble|rubles|rouble|roubles"},
    {"BTC", "bitcoin|bitcoins"},
};

const char *const kCommonCodes[] = {
    "USD", "EUR", "BRL", "GBP", "JPY", "CNY", "ARS", "CLP", "MXN", "COP", "UYU", "PYG", "PEN", "BOB",
    "CAD", "AUD", "NZD", "CHF", "SEK", "NOK", "DKK", "PLN", "CZK", "HUF", "TRY", "ZAR", "INR", "RUB",
    "KRW", "HKD", "SGD", "ILS", "AED", "SAR",
};

QString fold(const QString &s)
{
    QString d = s.normalized(QString::NormalizationForm_D).toLower();
    d.remove(QRegularExpression(QStringLiteral("\\p{Mn}")));
    return d.simplified();
}

bool hasAlias(const char *aliases, const QString &name)
{
    const QStringList list = QString::fromUtf8(aliases).split(QLatin1Char('|'));
    return list.contains(name);
}

const Unit *findUnit(const QString &name)
{
    for (const Unit &u : kUnits) {
        if (hasAlias(u.aliases, name))
            return &u;
    }
    return nullptr;
}

QString findCurrency(const QString &name, const QList<QString> &knownCodes)
{
    for (const Currency &c : kCurrencies) {
        if (hasAlias(c.aliases, name))
            return QString::fromLatin1(c.code);
    }
    static const QRegularExpression code(QStringLiteral("^[a-z]{3}$"));
    if (!code.match(name).hasMatch())
        return {};
    const QString upper = name.toUpper();
    if (!knownCodes.isEmpty())
        return knownCodes.contains(upper) ? upper : QString();
    for (const char *c : kCommonCodes) {
        if (upper == QLatin1String(c))
            return upper;
    }
    return {};
}

// "1.234,5" / "1,234.5" / "2,5" / "2.5" / "1.000" (pt) -> double
std::optional<double> parseNumber(QString s, const QLocale &locale)
{
    const int lastDot = s.lastIndexOf(QLatin1Char('.'));
    const int lastComma = s.lastIndexOf(QLatin1Char(','));
    QChar decimal;
    if (lastDot >= 0 && lastComma >= 0) {
        decimal = lastDot > lastComma ? QLatin1Char('.') : QLatin1Char(',');
    } else if (lastDot >= 0 || lastComma >= 0) {
        const QChar sep = lastDot >= 0 ? QLatin1Char('.') : QLatin1Char(',');
        const int count = s.count(sep);
        const int pos = s.lastIndexOf(sep);
        const bool threeAfter = s.size() - pos - 1 == 3;
        // several of them, or "1.000" where '.' groups digits in this locale: thousands
        if (count > 1 || (threeAfter && locale.groupSeparator() == QString(sep)))
            decimal = QChar();
        else
            decimal = sep;
    }
    QString plain;
    for (const QChar c : s) {
        if (c.isDigit() || c == QLatin1Char('-'))
            plain += c;
        else if (!decimal.isNull() && c == decimal)
            plain += QLatin1Char('.');
    }
    bool ok = false;
    const double v = plain.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return std::nullopt;
    return v;
}

} // namespace

QList<Split> splitQuery(const QString &text, const QLocale &locale)
{
    static const QRegularExpression leading(
        QStringLiteral("^(converter|converta|convert|quanto e|quanto sao|quantos sao|quantas sao|how much is|how many)\\s+"));
    static const QRegularExpression amountRe(
        QStringLiteral("^(us\\$|u\\$s|r\\$|\\$|€|£|¥)?\\s*([-+]?\\d[\\d.,]*)\\s*(.*)$"));
    static const QStringList separators = {
        QStringLiteral("em"), QStringLiteral("para"), QStringLiteral("pra"), QStringLiteral("to"),
        QStringLiteral("in"), QStringLiteral("into"), QStringLiteral("as"), QStringLiteral("->"),
        QStringLiteral("=>"), QStringLiteral("="), QStringLiteral("→"),
    };

    QString s = fold(text);
    s.remove(leading);
    s.remove(QRegularExpression(QStringLiteral("\\?$")));
    for (const char *arrow : {"->", "=>", "→"})
        s.replace(QString::fromUtf8(arrow), QStringLiteral(" %1 ").arg(QString::fromUtf8(arrow)));
    s = s.simplified();

    const QRegularExpressionMatch m = amountRe.match(s);
    if (!m.hasMatch())
        return {};
    const std::optional<double> amount = parseNumber(m.captured(2), locale);
    if (!amount)
        return {};
    const QString prefix = m.captured(1);
    const QStringList tokens = m.captured(3).split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QList<Split> out;
    for (int i = 0; i < tokens.size() - 1; ++i) {
        if (!separators.contains(tokens.at(i)))
            continue;
        Split split;
        split.amount = *amount;
        split.from = tokens.mid(0, i).join(QLatin1Char(' '));
        split.to = tokens.mid(i + 1).join(QLatin1Char(' '));
        if (!prefix.isEmpty() && split.from.isEmpty())
            split.from = prefix;
        if (split.from.isEmpty())
            continue;
        out.append(split);
    }
    return out;
}

std::optional<UnitResult> convertUnits(const QString &text, const QLocale &locale)
{
    for (const Split &split : splitQuery(text, locale)) {
        const Unit *from = findUnit(split.from);
        const Unit *to = findUnit(split.to);
        if (!from || !to || from->dim != to->dim || from == to)
            continue;
        const double base = split.amount * from->factor + from->offset;
        UnitResult r;
        r.amount = split.amount;
        r.value = (base - to->offset) / to->factor;
        r.fromSymbol = QCoreApplication::translate("Converter", from->symbol);
        r.toSymbol = QCoreApplication::translate("Converter", to->symbol);
        if (!std::isfinite(r.value))
            continue;
        return r;
    }
    return std::nullopt;
}

std::optional<CurrencyQuery> parseCurrency(const QString &text, const QList<QString> &knownCodes, const QLocale &locale)
{
    for (const Split &split : splitQuery(text, locale)) {
        const QString from = findCurrency(split.from, knownCodes);
        const QString to = findCurrency(split.to, knownCodes);
        if (from.isEmpty() || to.isEmpty() || from == to)
            continue;
        return CurrencyQuery{split.amount, from, to};
    }
    return std::nullopt;
}

std::optional<double> convertCurrency(double amount, const QString &from, const QString &to,
                                      const QHash<QString, double> &rates)
{
    const double rFrom = rates.value(from, 0);
    const double rTo = rates.value(to, 0);
    if (rFrom <= 0 || rTo <= 0)
        return std::nullopt;
    return amount / rFrom * rTo;
}

static QLocale withGrouping(QLocale locale, bool groupDigits)
{
    locale.setNumberOptions(groupDigits ? QLocale::DefaultNumberOptions : QLocale::OmitGroupSeparator);
    return locale;
}

QString formatNumber(double value, const QLocale &locale, bool groupDigits)
{
    const QLocale l = withGrouping(locale, groupDigits);
    if (value == 0)
        return l.toString(0);
    const double mag = std::abs(value);
    if (mag >= 1e15 || mag < 1e-6)
        return l.toString(value, 'g', 7);
    const int intDigits = int(std::floor(std::log10(mag))) + 1;
    const int decimals = std::clamp(7 - intDigits, 0, 12);
    QString s = l.toString(value, 'f', decimals);
    const QString point = l.decimalPoint();
    if (decimals > 0 && s.contains(point)) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(point))
            s.chop(point.size());
    }
    if (s == QLatin1String("-0"))
        s = l.toString(0);
    return s;
}

QString formatMoney(double value, const QLocale &locale, bool groupDigits)
{
    if (std::abs(value) >= 1 || value == 0)
        return withGrouping(locale, groupDigits).toString(value, 'f', 2);
    return formatNumber(value, locale, groupDigits);
}

} // namespace Converter
