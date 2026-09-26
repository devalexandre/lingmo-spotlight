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

#include "searchengine.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QGuiApplication>
#include <QJSEngine>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent>

#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KSycoca>

#include "converter.h"

#include <cmath>

namespace {

constexpr int MaxApps = 6;
constexpr int MaxFiles = 8;
constexpr int MaxFileDepth = 5;
constexpr int MaxVisitedEntries = 40000;
constexpr int MinContentQuery = 3;

// Lingmo Settings pages: module id, title, icon, extra search words (en + pt)
// Titles are translated at display time (context "SettingsPage")
struct SettingsPage { const char *id; const char *title; const char *icon; const char *keywords; };
const SettingsPage kSettingsPages[] = {
    {"wlan", QT_TRANSLATE_NOOP("SettingsPage", "Wi-Fi"), "network-wireless", "wifi wlan wireless rede sem fio internet"},
    {"ethernet", QT_TRANSLATE_NOOP("SettingsPage", "Ethernet"), "network-wired", "ethernet cabo wired rede network"},
    {"bluetooth", QT_TRANSLATE_NOOP("SettingsPage", "Bluetooth"), "bluetooth", "bluetooth fone headset"},
    {"proxy", QT_TRANSLATE_NOOP("SettingsPage", "Proxy"), "preferences-system-network-proxy", "proxy rede network"},
    {"display", QT_TRANSLATE_NOOP("SettingsPage", "Display"), "preferences-desktop-display", "display tela monitor exibicao exibição resolucao resolução escala brilho brightness"},
    {"appearance", QT_TRANSLATE_NOOP("SettingsPage", "Appearance"), "preferences-desktop-theme", "appearance aparencia aparência tema theme escuro dark claro light cor accent"},
    {"background", QT_TRANSLATE_NOOP("SettingsPage", "Wallpaper"), "preferences-desktop-wallpaper", "wallpaper background fundo plano papel de parede"},
    {"dock", QT_TRANSLATE_NOOP("SettingsPage", "Dock"), "preferences-desktop", "dock barra"},
    {"accounts", QT_TRANSLATE_NOOP("SettingsPage", "Users"), "system-users", "user users usuario usuário conta account senha password foto avatar"},
    {"notifications", QT_TRANSLATE_NOOP("SettingsPage", "Notifications"), "preferences-desktop-notification", "notifications notificacoes notificações avisos"},
    {"sound", QT_TRANSLATE_NOOP("SettingsPage", "Sound"), "audio-volume-high", "sound som audio áudio volume microfone microphone"},
    {"mouse", QT_TRANSLATE_NOOP("SettingsPage", "Mouse"), "input-mouse", "mouse cursor ponteiro"},
    {"touchpad", QT_TRANSLATE_NOOP("SettingsPage", "Touchpad"), "input-touchpad", "touchpad trackpad"},
    {"datetime", QT_TRANSLATE_NOOP("SettingsPage", "Date & Time"), "preferences-system-time", "date time data hora relogio relógio fuso timezone"},
    {"accessibility", QT_TRANSLATE_NOOP("SettingsPage", "Accessibility"), "preferences-desktop-accessibility", "accessibility acessibilidade"},
    {"defaultapps", QT_TRANSLATE_NOOP("SettingsPage", "Default Applications"), "preferences-desktop-default-applications", "default apps aplicativos padrao padrão navegador browser"},
    {"language", QT_TRANSLATE_NOOP("SettingsPage", "Language"), "preferences-desktop-locale", "language idioma lingua língua teclado keyboard"},
    {"battery", QT_TRANSLATE_NOOP("SettingsPage", "Battery"), "battery", "battery bateria energia"},
    {"power", QT_TRANSLATE_NOOP("SettingsPage", "Power"), "preferences-system-power", "power energia suspender desligar sleep"},
    {"about", QT_TRANSLATE_NOOP("SettingsPage", "About"), "help-about", "about sobre sistema system versao versão"},
};

QString fold(const QString &s)
{
    // Case- and accent-insensitive matching ("usuario" finds "Usuário")
    QString d = s.normalized(QString::NormalizationForm_D).toLower();
    d.remove(QRegularExpression(QStringLiteral("\\p{Mn}")));
    return d;
}

// 0 = no match; higher is better
double matchScore(const QString &text, const QString &q)
{
    const QString t = fold(text);
    if (t.isEmpty() || q.isEmpty())
        return 0;
    if (t == q)
        return 120;
    if (t.startsWith(q))
        return 100;
    const int pos = t.indexOf(q);
    if (pos < 0)
        return 0;
    // start of a word beats the middle of one
    return (pos > 0 && !t.at(pos - 1).isLetterOrNumber()) ? 80 : 55;
}

QList<SearchResult> findFiles(const QString &q)
{
    QList<SearchResult> found;
    const QString home = QDir::homePath();
    const int baseDepth = home.count(QLatin1Char('/'));
    static const QStringList skipDirs = {QStringLiteral("node_modules"), QStringLiteral("__pycache__"),
                                         QStringLiteral("venv"), QStringLiteral("target"),
                                         QStringLiteral("build"), QStringLiteral("snap")};
    QMimeDatabase mimes;

    QDirIterator it(home, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int visited = 0;
    while (it.hasNext() && visited++ < MaxVisitedEntries) {
        const QString path = it.next();
        const QFileInfo info = it.fileInfo();
        const QString rel = path.mid(home.size() + 1);
        // skip hidden trees, heavy build dirs and anything too deep
        if (rel.contains(QLatin1String("/.")) || rel.startsWith(QLatin1Char('.'))
            || path.count(QLatin1Char('/')) - baseDepth > MaxFileDepth)
            continue;
        bool skip = false;
        for (const QString &d : skipDirs) {
            if (rel.contains(QLatin1Char('/') + d + QLatin1Char('/')) || rel.startsWith(d + QLatin1Char('/'))) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;

        const double score = matchScore(info.fileName(), q);
        if (score <= 0)
            continue;
        SearchResult r;
        r.kind = SearchResult::File;
        r.title = info.fileName();
        r.subtitle = QStringLiteral("~/") + QFileInfo(rel).path();
        r.icon = info.isDir() ? QStringLiteral("folder") : mimes.mimeTypeForFile(info).iconName();
        r.category = QCoreApplication::translate("SearchEngine", "Files");
        r.payload = path;
        r.score = score - rel.count(QLatin1Char('/'));  // shallower first
        found.append(r);
    }
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.score > b.score; });
    if (found.size() > MaxFiles)
        found.resize(MaxFiles);
    return found;
}

} // namespace

SearchEngine::SearchEngine(QObject *parent)
    : QObject(parent)
    , m_model(this)
    , m_usage(QStringLiteral("lingmo"), QStringLiteral("spotlight"))
{
    reloadApps();
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, &SearchEngine::reloadApps);

    // File search walks the disk: wait for a pause in typing
    m_fileDebounce.setSingleShot(true);
    m_fileDebounce.setInterval(180);
    connect(&m_fileDebounce, &QTimer::timeout, this, [this] {
        startFileSearch(fold(m_query.trimmed()), m_generation);
        if (m_searchContents)
            m_contentSearch.start(m_query.trimmed());
    });
    connect(&m_fileWatcher, &QFutureWatcher<QList<SearchResult>>::finished, this, [this] {
        if (m_fileWatcher.property("generation").toULongLong() != m_generation) {
            // Stale: the query changed while walking the disk; search for the current one
            const QString q = fold(m_query.trimmed());
            if (q.size() >= 2)
                startFileSearch(q, m_generation);
            return;
        }
        m_files = m_fileWatcher.result();
        rebuild();
    });
    connect(&m_contentSearch, &ContentSearch::finished, this, [this](const QString &query, const QList<SearchResult> &results) {
        if (query != m_query.trimmed())
            return;
        m_contents = results;
        rebuild();
    });
    // Fresh exchange rates arrived: redo the conversion row
    connect(&m_rates, &CurrencyRates::ratesChanged, this, [this] {
        if (m_current.isEmpty())
            return;
        m_current = instantResults(m_query.trimmed());
        rebuild();
    });
}

void SearchEngine::reloadApps()
{
    m_apps = KApplicationTrader::query([](const KService::Ptr &s) {
        return !s->noDisplay() && !s->exec().isEmpty();
    });
}

void SearchEngine::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    search();
}

void SearchEngine::search()
{
    ++m_generation;
    m_fileDebounce.stop();
    m_contentSearch.cancel();
    m_files.clear();
    m_contents.clear();
    m_web.clear();
    m_searchContents = false;
    const QString q = fold(m_query.trimmed());
    m_current.clear();

    if (!q.isEmpty()) {
        m_current = instantResults(m_query.trimmed());

        SearchResult web;
        web.kind = SearchResult::Web;
        web.title = tr("Search the web for \"%1\"").arg(m_query.trimmed());
        web.icon = QStringLiteral("internet-web-browser");
        web.category = tr("Web");
        web.payload = m_query.trimmed();
        m_web << web;

        // A calculation or conversion is the answer: don't grep the disk for "10 km em milhas"
        const bool answered = !m_current.isEmpty()
            && (m_current.first().kind == SearchResult::Calculator || m_current.first().kind == SearchResult::Conversion
                || m_current.first().kind == SearchResult::Info);
        m_searchContents = !answered && q.size() >= MinContentQuery;
        if (q.size() >= 2)
            m_fileDebounce.start();
    }
    rebuild();
}

QList<SearchResult> SearchEngine::instantResults(const QString &text)
{
    const QString q = fold(text);
    if (q.isEmpty())
        return {};
    return calculator(text) + conversions(text) + apps(q) + settings(q);
}

void SearchEngine::rebuild()
{
    QList<SearchResult> all = m_current + m_files;
    // A file already listed by name isn't repeated under its contents
    for (const SearchResult &r : std::as_const(m_contents)) {
        const bool listed = std::any_of(m_files.cbegin(), m_files.cend(),
                                        [&r](const SearchResult &f) { return f.payload == r.payload; });
        if (!listed)
            all << r;
    }
    all << m_web;  // "Search the web" stays last
    m_model.setResults(all);
}

void SearchEngine::startFileSearch(const QString &query, quint64 generation)
{
    if (m_fileWatcher.isRunning())
        return;  // the running search will be superseded by the next keystroke's
    m_fileWatcher.setProperty("generation", QVariant::fromValue(generation));
    m_fileWatcher.setFuture(QtConcurrent::run(findFiles, query));
}

QList<SearchResult> SearchEngine::calculator(const QString &q) const
{
    // Only plain arithmetic reaches the JS engine
    static const QRegularExpression allowed(QStringLiteral("^[0-9\\s.,+\\-*/()%^]+$"));
    static const QRegularExpression hasOperator(QStringLiteral("[0-9)]\\s*[+\\-*/%^]\\s*[0-9(]"));
    if (!allowed.match(q).hasMatch() || !hasOperator.match(q).hasMatch())
        return {};

    QString expr = q;
    expr.replace(QLatin1Char(','), QLatin1Char('.')).replace(QLatin1Char('^'), QStringLiteral("**"));
    QJSEngine engine;
    const QJSValue v = engine.evaluate(expr);
    if (v.isError() || !v.isNumber() || !std::isfinite(v.toNumber()))
        return {};

    SearchResult r;
    r.kind = SearchResult::Calculator;
    r.title = QString::number(v.toNumber(), 'g', 12);
    r.subtitle = tr("%1 — press Enter to copy").arg(q);
    r.icon = QStringLiteral("accessories-calculator");
    r.category = tr("Calculator");
    r.payload = r.title;
    r.score = 1000;
    return {r};
}

QList<SearchResult> SearchEngine::conversions(const QString &q)
{
    // Every conversion starts with an amount (maybe after a currency sign or "quanto é")
    static const QRegularExpression hasDigit(QStringLiteral("\\d"));
    if (!hasDigit.match(q).hasMatch())
        return {};

    SearchResult r;
    r.icon = QStringLiteral("accessories-calculator");
    r.score = 1000;

    if (const auto unit = Converter::convertUnits(q)) {
        r.kind = SearchResult::Conversion;
        r.title = QStringLiteral("%1 %2").arg(Converter::formatNumber(unit->value), unit->toSymbol);
        r.subtitle = tr("%1 %2 = %3 — press Enter to copy")
                         .arg(Converter::formatNumber(unit->amount), unit->fromSymbol, r.title);
        r.category = tr("Conversion");
        r.payload = Converter::formatNumber(unit->value, QLocale(), false);
        return {r};
    }

    const auto money = Converter::parseCurrency(q, m_rates.rates().keys());
    if (!money)
        return {};
    m_rates.refreshIfStale();
    r.category = tr("Currency");
    const auto value = Converter::convertCurrency(money->amount, money->from, money->to, m_rates.rates());
    if (!value) {
        r.kind = SearchResult::Info;
        r.title = m_rates.isLoading() ? tr("Fetching exchange rates…") : tr("Exchange rates unavailable");
        r.subtitle = m_rates.isLoading() ? tr("%1 %2 to %3").arg(Converter::formatMoney(money->amount), money->from, money->to)
                                         : tr("Connect to the internet once to download them");
        return {r};
    }
    r.kind = SearchResult::Conversion;
    r.title = QStringLiteral("%1 %2").arg(Converter::formatMoney(*value), money->to);
    r.subtitle = tr("%1 %2 = %3 · rates from %4 — press Enter to copy")
                     .arg(Converter::formatMoney(money->amount), money->from, r.title,
                          QLocale().toString(m_rates.updated().date(), QLocale::ShortFormat));
    r.payload = Converter::formatMoney(*value, QLocale(), false);
    return {r};
}

QList<SearchResult> SearchEngine::apps(const QString &q) const
{
    QList<SearchResult> out;
    for (const KService::Ptr &s : m_apps) {
        double score = matchScore(s->name(), q);
        score = std::max(score, matchScore(s->genericName(), q) * 0.7);
        score = std::max(score, matchScore(s->untranslatedGenericName(), q) * 0.6);
        for (const QString &k : s->keywords())
            score = std::max(score, matchScore(k, q) * 0.5);
        score = std::max(score, matchScore(QFileInfo(s->exec().section(QLatin1Char(' '), 0, 0)).fileName(), q) * 0.6);
        if (score <= 0)
            continue;
        // Apps you open often float to the top
        score += 12 * std::log1p(m_usage.value(QStringLiteral("usage/") + s->storageId(), 0).toInt());

        SearchResult r;
        r.kind = SearchResult::App;
        r.title = s->name();
        r.subtitle = s->genericName().isEmpty() ? s->comment() : s->genericName();
        r.icon = s->icon();
        r.category = tr("Applications");
        r.payload = s->storageId();
        r.score = score;
        out.append(r);
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.score > b.score; });
    if (out.size() > MaxApps)
        out.resize(MaxApps);
    return out;
}

QList<SearchResult> SearchEngine::settings(const QString &q) const
{
    QList<SearchResult> out;
    for (const SettingsPage &p : kSettingsPages) {
        double score = std::max(matchScore(QString::fromUtf8(p.title), q),
                                matchScore(QCoreApplication::translate("SettingsPage", p.title), q));
        for (const QString &k : QString::fromUtf8(p.keywords).split(QLatin1Char(' ')))
            score = std::max(score, matchScore(k, q) * 0.8);
        if (score <= 0)
            continue;
        SearchResult r;
        r.kind = SearchResult::Setting;
        r.title = QCoreApplication::translate("SettingsPage", p.title);
        r.subtitle = tr("Settings");
        r.icon = QString::fromUtf8(p.icon);
        r.category = tr("Settings");
        r.payload = QString::fromUtf8(p.id);
        r.score = score;
        out.append(r);
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.score > b.score; });
    if (out.size() > 4)
        out.resize(4);
    return out;
}

bool SearchEngine::activate(int row)
{
    const SearchResult *r = m_model.at(row);
    if (!r)
        return false;

    switch (r->kind) {
    case SearchResult::App: {
        const KService::Ptr service = KService::serviceByStorageId(r->payload);
        if (!service)
            return false;
        auto *job = new KIO::ApplicationLauncherJob(service);
        job->start();
        const QString key = QStringLiteral("usage/") + r->payload;
        m_usage.setValue(key, m_usage.value(key, 0).toInt() + 1);
        break;
    }
    case SearchResult::Calculator:
    case SearchResult::Conversion:
        QGuiApplication::clipboard()->setText(r->payload);
        break;
    case SearchResult::Info:
        return false;
    case SearchResult::Content:
        if (!QProcess::startDetached(QStringLiteral("xdg-open"), {r->payload}))
            QDesktopServices::openUrl(QUrl::fromLocalFile(r->payload));
        break;
    case SearchResult::Setting:
        QProcess::startDetached(QStringLiteral("lingmo-settings"), {QStringLiteral("-m"), r->payload});
        break;
    case SearchResult::File:
        QDesktopServices::openUrl(QUrl::fromLocalFile(r->payload));
        break;
    case SearchResult::Web: {
        QUrl url(QStringLiteral("https://duckduckgo.com/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), r->payload);
        url.setQuery(query);
        QDesktopServices::openUrl(url);
        break;
    }
    }
    return true;
}
