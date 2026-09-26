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

#include <QDateTime>
#include <QHash>
#include <QObject>

class QJsonObject;
class QNetworkAccessManager;

// Exchange rates (per 1 USD) from free, key-less APIs, cached in
// ~/.cache/lingmo-spotlight/rates.json so conversions keep working offline
class CurrencyRates : public QObject
{
    Q_OBJECT

public:
    explicit CurrencyRates(QObject *parent = nullptr);

    const QHash<QString, double> &rates() const { return m_rates; }
    QDateTime updated() const { return m_updated; }  // when the provider published them
    bool isLoading() const { return m_loading; }

    // Downloads new rates in the background when the cache is older than a few hours
    void refreshIfStale();

Q_SIGNALS:
    void ratesChanged();

private:
    void load();
    void fetch(int source);
    bool apply(const QJsonObject &cache);

    QString m_cachePath;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, double> m_rates;
    QDateTime m_updated;
    QDateTime m_fetched;
    QDateTime m_lastAttempt;
    bool m_loading = false;
};
