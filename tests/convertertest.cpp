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

// Self-check for the conversion parser: prints every sample and exits non-zero on a mismatch

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

#include "../src/converter.h"

namespace {

int failures = 0;
QTextStream out(stdout);

bool near(double a, double b)
{
    return std::abs(a - b) <= 1e-6 * std::max(1.0, std::abs(b));
}

void unit(const QString &q, const QLocale &locale, double expected, const QString &toSymbol = {})
{
    const auto r = Converter::convertUnits(q, locale);
    const bool ok = r && near(r->value, expected) && (toSymbol.isEmpty() || r->toSymbol == toSymbol);
    out << (ok ? "ok   " : "FAIL ") << q.leftJustified(34) << " -> "
        << (r ? QStringLiteral("%1 %2").arg(Converter::formatNumber(r->value, locale), r->toSymbol) : QStringLiteral("(none)"))
        << "\n";
    failures += !ok;
}

void noUnit(const QString &q)
{
    const auto r = Converter::convertUnits(q, QLocale(QLocale::Portuguese, QLocale::Brazil));
    out << (r ? "FAIL " : "ok   ") << q.leftJustified(34) << " -> "
        << (r ? Converter::formatNumber(r->value) : QStringLiteral("(not a unit conversion)")) << "\n";
    failures += bool(r);
}

void money(const QString &q, const QString &from, const QString &to, double amount, const QHash<QString, double> &rates)
{
    const QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    const auto c = Converter::parseCurrency(q, rates.keys(), pt);
    const bool ok = c && c->from == from && c->to == to && near(c->amount, amount);
    QString shown = QStringLiteral("(none)");
    if (c) {
        const auto v = Converter::convertCurrency(c->amount, c->from, c->to, rates);
        shown = QStringLiteral("%1 %2 -> %3 %4").arg(Converter::formatMoney(c->amount, pt), c->from,
                                                    v ? Converter::formatMoney(*v, pt) : QStringLiteral("?"), c->to);
    }
    out << (ok ? "ok   " : "FAIL ") << q.leftJustified(34) << " -> " << shown << "\n";
    failures += !ok;
}

void format(double v, const QLocale &locale, const QString &expected)
{
    const QString s = Converter::formatNumber(v, locale);
    const bool ok = s == expected;
    out << (ok ? "ok   " : "FAIL ") << "format " << QString::number(v, 'g', 12).leftJustified(27) << " -> " << s << "\n";
    failures += !ok;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    const QLocale en(QLocale::English, QLocale::UnitedStates);

    out << "== units (pt_BR) ==\n";
    unit(QStringLiteral("10 km em milhas"), pt, 6.213711922, QStringLiteral("mi"));
    unit(QStringLiteral("5 kg to lb"), pt, 11.02311311, QStringLiteral("lb"));
    unit(QStringLiteral("100 f em c"), pt, 37.77777778, QStringLiteral("°C"));
    unit(QStringLiteral("30 cm em polegadas"), pt, 11.81102362, QStringLiteral("in"));
    unit(QStringLiteral("2 gb em mb"), pt, 2000, QStringLiteral("MB"));
    unit(QStringLiteral("1 hora em minutos"), pt, 60, QStringLiteral("min"));
    unit(QStringLiteral("37,5 °C em F"), pt, 99.5);
    unit(QStringLiteral("0 k em celsius"), pt, -273.15);
    unit(QStringLiteral("1,5 litros em ml"), pt, 1500);
    unit(QStringLiteral("2 galões para litros"), pt, 7.570823568);
    unit(QStringLiteral("3 xícaras em ml"), pt, 709.7647095);
    unit(QStringLiteral("100 m² em pés quadrados"), pt, 1076.391042);
    unit(QStringLiteral("1 alqueire em hectares"), pt, 2.42);
    unit(QStringLiteral("1 km2 em ha"), pt, 100);
    unit(QStringLiteral("100 km/h em mph"), pt, 62.13711922);
    unit(QStringLiteral("10 nós em km/h"), pt, 18.52);
    unit(QStringLiteral("1 gib em mb"), pt, 1073.741824);
    unit(QStringLiteral("100 megabits em mb"), pt, 12.5);
    unit(QStringLiteral("1 semana em horas"), pt, 168);
    unit(QStringLiteral("1 ano em dias"), pt, 365.2425);
    unit(QStringLiteral("10km→mi"), pt, 6.213711922);
    unit(QStringLiteral("converter 1.000 metros em km"), pt, 1);
    unit(QStringLiteral("quanto é 72 polegadas em cm?"), pt, 182.88);
    unit(QStringLiteral("5 in in cm"), pt, 12.7);
    unit(QStringLiteral("50 libras em kg"), pt, 22.6796185);

    out << "== units (en_US) ==\n";
    unit(QStringLiteral("5 kg to lb"), en, 11.02311311);
    unit(QStringLiteral("1,000 feet in meters"), en, 304.8);
    unit(QStringLiteral("2.5 miles to km"), en, 4.02336);
    unit(QStringLiteral("98.6 f to c"), en, 37);
    unit(QStringLiteral("1 tablespoon in teaspoons"), en, 3);

    out << "== not conversions ==\n";
    noUnit(QStringLiteral("10 km em kg"));
    noUnit(QStringLiteral("firefox"));
    noUnit(QStringLiteral("10 + 5"));
    noUnit(QStringLiteral("100 usd em brl"));
    noUnit(QStringLiteral("2 km"));

    out << "== currency (pt_BR, sample rates per USD) ==\n";
    const QHash<QString, double> rates = {
        {QStringLiteral("USD"), 1}, {QStringLiteral("BRL"), 5.4}, {QStringLiteral("EUR"), 0.9},
        {QStringLiteral("GBP"), 0.75}, {QStringLiteral("JPY"), 150}, {QStringLiteral("ARS"), 1000},
    };
    money(QStringLiteral("100 usd em brl"), QStringLiteral("USD"), QStringLiteral("BRL"), 100, rates);
    money(QStringLiteral("50 euros em reais"), QStringLiteral("EUR"), QStringLiteral("BRL"), 50, rates);
    money(QStringLiteral("R$ 20 em dólar"), QStringLiteral("BRL"), QStringLiteral("USD"), 20, rates);
    money(QStringLiteral("$ 1.500,50 para reais"), QStringLiteral("USD"), QStringLiteral("BRL"), 1500.5, rates);
    money(QStringLiteral("10 libras em reais"), QStringLiteral("GBP"), QStringLiteral("BRL"), 10, rates);
    money(QStringLiteral("1000 ienes em dolares"), QStringLiteral("JPY"), QStringLiteral("USD"), 1000, rates);
    money(QStringLiteral("100 BRL to EUR"), QStringLiteral("BRL"), QStringLiteral("EUR"), 100, rates);
    money(QStringLiteral("1 real em pesos argentinos"), QStringLiteral("BRL"), QStringLiteral("ARS"), 1, rates);
    const bool noCurrency = !Converter::parseCurrency(QStringLiteral("10 km em milhas"), rates.keys(), pt)
                         && !Converter::parseCurrency(QStringLiteral("100 abc em brl"), rates.keys(), pt);
    out << (noCurrency ? "ok   " : "FAIL ") << "units and unknown codes are not currency\n";
    failures += !noCurrency;

    out << "== formatting ==\n";
    format(6.213711922, pt, QStringLiteral("6,213712"));
    format(1234567.891, pt, QStringLiteral("1.234.568"));
    format(1234567.891, en, QStringLiteral("1,234,568"));
    format(0.5, pt, QStringLiteral("0,5"));
    format(60, pt, QStringLiteral("60"));
    format(-273.15, pt, QStringLiteral("-273,15"));
    format(1e-9, en, QStringLiteral("1e-09"));

    out << (failures ? QStringLiteral("%1 FAILED\n").arg(failures) : QStringLiteral("all passed\n"));
    out.flush();
    return failures ? 1 : 0;
}
