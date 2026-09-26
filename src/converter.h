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

#pragma once

#include <QHash>
#include <QList>
#include <QLocale>
#include <QString>

#include <optional>

// Parsing of natural conversion queries ("10 km em milhas", "5 kg to lb",
// "R$ 20 em dólar"). Pure functions, no GUI: exercised by tests/convertertest.
namespace Converter {

// "<amount> <from> <separator> <to>", one entry per way of splitting the text
struct Split {
    double amount = 0;
    QString from;  // folded (lower case, no accents)
    QString to;
};
QList<Split> splitQuery(const QString &text, const QLocale &locale = QLocale());

struct UnitResult {
    double amount = 0;
    double value = 0;
    QString fromSymbol;
    QString toSymbol;
};
std::optional<UnitResult> convertUnits(const QString &text, const QLocale &locale = QLocale());

struct CurrencyQuery {
    double amount = 0;
    QString from;  // ISO 4217 code, upper case
    QString to;
};
// knownCodes: upper case ISO codes the rate table has; empty = a built-in list of common ones
std::optional<CurrencyQuery> parseCurrency(const QString &text, const QList<QString> &knownCodes = {},
                                           const QLocale &locale = QLocale());

// rates are "units per 1 base currency"
std::optional<double> convertCurrency(double amount, const QString &from, const QString &to,
                                      const QHash<QString, double> &rates);

// Locale-aware number for display (~7 significant digits, no exponent for everyday sizes)
QString formatNumber(double value, const QLocale &locale = QLocale(), bool groupDigits = true);
// Money: two decimals, more only for amounts below 1
QString formatMoney(double value, const QLocale &locale = QLocale(), bool groupDigits = true);

} // namespace Converter
