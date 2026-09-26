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

#include "currencyrates.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>

namespace {

constexpr int MaxAgeSecs = 6 * 3600;
constexpr int RetryAfterSecs = 10 * 60;
constexpr int TimeoutMs = 10000;

// Tried in order until one answers
const char *const kSources[] = {
    "https://open.er-api.com/v6/latest/USD",
    "https://api.frankfurter.app/latest?from=USD",
};

// Both APIs answer with {"rates": {...}} plus a publication date in their own format;
// normalize to {"base": "USD", "updated": <unix>, "rates": {...}}
QJsonObject normalize(const QJsonObject &reply)
{
    QJsonObject rates = reply.value(QStringLiteral("rates")).toObject();
    if (rates.isEmpty())
        return {};
    qint64 updated = reply.value(QStringLiteral("time_last_update_unix")).toInteger();
    if (updated <= 0) {
        const QDate date = QDate::fromString(reply.value(QStringLiteral("date")).toString(), Qt::ISODate);
        updated = date.isValid() ? date.startOfDay(QTimeZone::UTC).toSecsSinceEpoch()
                                 : QDateTime::currentSecsSinceEpoch();
    }
    rates.insert(QStringLiteral("USD"), 1.0);
    return {
        {QStringLiteral("base"), QStringLiteral("USD")},
        {QStringLiteral("updated"), updated},
        {QStringLiteral("fetched"), QDateTime::currentSecsSinceEpoch()},
        {QStringLiteral("rates"), rates},
    };
}

} // namespace

CurrencyRates::CurrencyRates(QObject *parent)
    : QObject(parent)
    , m_cachePath(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                  + QStringLiteral("/lingmo-spotlight/rates.json"))
{
    load();
}

void CurrencyRates::load()
{
    QFile file(m_cachePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    apply(QJsonDocument::fromJson(file.readAll()).object());
}

bool CurrencyRates::apply(const QJsonObject &cache)
{
    const QJsonObject rates = cache.value(QStringLiteral("rates")).toObject();
    QHash<QString, double> parsed;
    for (auto it = rates.begin(); it != rates.end(); ++it) {
        const double v = it.value().toDouble();
        if (v > 0)
            parsed.insert(it.key().toUpper(), v);
    }
    if (parsed.isEmpty() || !parsed.contains(QStringLiteral("USD")))
        return false;
    m_rates = parsed;
    m_updated = QDateTime::fromSecsSinceEpoch(cache.value(QStringLiteral("updated")).toInteger());
    m_fetched = QDateTime::fromSecsSinceEpoch(cache.value(QStringLiteral("fetched")).toInteger());
    return true;
}

void CurrencyRates::refreshIfStale()
{
    const QDateTime now = QDateTime::currentDateTime();
    if (m_loading || (!m_rates.isEmpty() && m_fetched.secsTo(now) < MaxAgeSecs))
        return;
    // Offline: don't hammer the network on every keystroke
    if (m_lastAttempt.isValid() && m_lastAttempt.secsTo(now) < RetryAfterSecs)
        return;
    m_lastAttempt = now;
    m_loading = true;
    fetch(0);
}

void CurrencyRates::fetch(int source)
{
    if (source >= int(std::size(kSources))) {
        m_loading = false;
        Q_EMIT ratesChanged();  // lets the UI drop its "loading" row
        return;
    }
    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(QString::fromLatin1(kSources[source])));
    request.setTransferTimeout(TimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("lingmo-spotlight"));
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source] {
        reply->deleteLater();
        const QJsonObject cache = reply->error() == QNetworkReply::NoError
            ? normalize(QJsonDocument::fromJson(reply->readAll()).object())
            : QJsonObject();
        if (cache.isEmpty() || !apply(cache)) {
            fetch(source + 1);
            return;
        }
        m_loading = false;
        QDir().mkpath(QFileInfo(m_cachePath).path());
        QSaveFile file(m_cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(cache).toJson(QJsonDocument::Compact));
            file.commit();
        }
        Q_EMIT ratesChanged();
    });
}
