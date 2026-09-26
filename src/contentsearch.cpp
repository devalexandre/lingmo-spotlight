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

#include "contentsearch.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDeadlineTimer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

namespace {

constexpr int MaxResults = 6;
constexpr qint64 MaxFileSize = 2 * 1024 * 1024;
constexpr int MaxDepth = 8;
constexpr int IndexBudgetMs = 1500;
constexpr int ScanBudgetMs = 3000;
constexpr int SnippetLength = 64;

const QStringList &skippedDirs()
{
    static const QStringList dirs = {
        QStringLiteral("node_modules"), QStringLiteral("__pycache__"), QStringLiteral("venv"),
        QStringLiteral("target"), QStringLiteral("build"), QStringLiteral("dist"), QStringLiteral("snap"),
    };
    return dirs;
}

bool skippedDir(const QString &name)
{
    return name.startsWith(QLatin1Char('.')) || skippedDirs().contains(name) || name.startsWith(QLatin1String("build-"));
}

// The matching line, trimmed around the match so it fits one row
QString snippet(const QString &line, const QString &query)
{
    const QString text = line.simplified();
    if (text.size() <= SnippetLength)
        return text;
    const int pos = std::max<qsizetype>(0, text.indexOf(query, 0, Qt::CaseInsensitive));
    const int start = std::clamp(pos - SnippetLength / 3, 0, int(text.size()) - SnippetLength);
    QString out = text.mid(start, SnippetLength);
    if (start > 0)
        out.prepend(QStringLiteral("…"));
    if (start + SnippetLength < text.size())
        out.append(QStringLiteral("…"));
    return out;
}

SearchResult makeResult(const QString &path, const QString &line, const QString &query, const QString &category)
{
    static const QMimeDatabase mimes;
    const QFileInfo info(path);
    QString dir = info.path();
    if (dir.startsWith(QDir::homePath()))
        dir = QStringLiteral("~") + dir.mid(QDir::homePath().size());

    SearchResult r;
    r.kind = SearchResult::Content;
    r.title = info.fileName();
    const QString snip = snippet(line, query);
    r.subtitle = snip.isEmpty() ? dir : QStringLiteral("%1  ·  %2").arg(snip, dir);
    r.icon = mimes.mimeTypeForFile(path, QMimeDatabase::MatchExtension).iconName();
    r.category = category;
    r.payload = path;
    return r;
}

// First line containing the query, or an empty string; false when the file looks binary
bool findLine(const QString &path, const QString &query, QString *line, const std::atomic_bool &cancelled)
{
    QFile file(path);
    if (file.size() > MaxFileSize || !file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = file.peek(4096);
    if (head.contains('\0'))
        return false;
    while (!file.atEnd() && !cancelled) {
        const QString text = QString::fromUtf8(file.readLine(64 * 1024));
        if (text.contains(query, Qt::CaseInsensitive)) {
            *line = text;
            return true;
        }
    }
    return true;
}

// Worker thread: snippets for the files an index returned
QList<SearchResult> describeHits(const QStringList &paths, const QString &query, const QString &category,
                                 std::shared_ptr<std::atomic_bool> cancelled)
{
    QList<SearchResult> out;
    const QDeadlineTimer deadline(ScanBudgetMs);
    for (const QString &path : paths) {
        if (*cancelled || out.size() >= MaxResults)
            break;
        QString line;
        if (!deadline.hasExpired())
            findLine(path, query, &line, *cancelled);
        out.append(makeResult(path, line, query, category));
    }
    return out;
}

// Worker thread: plain scan when there is neither an index nor ripgrep
QList<SearchResult> scanFiles(const QStringList &roots, const QStringList &homeFiles, const QString &query,
                              const QString &category, std::shared_ptr<std::atomic_bool> cancelled)
{
    QList<SearchResult> out;
    const QDeadlineTimer deadline(ScanBudgetMs);
    auto visit = [&](const QString &path) {
        QString line;
        if (findLine(path, query, &line, *cancelled) && !line.isEmpty())
            out.append(makeResult(path, line, query, category));
        return out.size() < MaxResults && !*cancelled && !deadline.hasExpired();
    };

    for (const QString &path : homeFiles) {
        if (!visit(path))
            return out;
    }
    for (const QString &root : roots) {
        QStringList pending = {root};
        while (!pending.isEmpty()) {
            const QString dir = pending.takeFirst();
            const int depth = dir.count(QLatin1Char('/')) - root.count(QLatin1Char('/'));
            const QFileInfoList entries = QDir(dir).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo &entry : entries) {
                if (entry.isDir()) {
                    if (depth < MaxDepth && !entry.isSymLink() && !skippedDir(entry.fileName()))
                        pending.append(entry.filePath());
                } else if (entry.size() <= MaxFileSize && !visit(entry.filePath())) {
                    return out;
                }
            }
        }
    }
    return out;
}

QString findTool(const QStringList &names)
{
    for (const QString &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty())
            return path;
    }
    return {};
}

} // namespace

ContentSearch::ContentSearch(QObject *parent)
    : QObject(parent)
    , m_cancelled(std::make_shared<std::atomic_bool>(false))
    , m_ripgrep(QStandardPaths::findExecutable(QStringLiteral("rg")))
{
    m_budget.setSingleShot(true);
    connect(&m_budget, &QTimer::timeout, this, [this] {
        // Out of time: show whatever was found so far
        const bool indexing = m_backend == Backend::Baloo || m_backend == Backend::LocalSearch;
        stopProcess();
        if (indexing)
            m_ripgrep.isEmpty() ? startScan() : startRipgrep();
        else
            deliver(m_serial, m_found);
    });
}

ContentSearch::~ContentSearch()
{
    cancel();
}

void ContentSearch::cancel()
{
    ++m_serial;
    m_budget.stop();
    stopProcess();
    m_cancelled->store(true);
    m_cancelled = std::make_shared<std::atomic_bool>(false);
}

void ContentSearch::stopProcess()
{
    if (!m_process)
        return;
    m_process->disconnect(this);
    if (m_process->state() == QProcess::NotRunning) {
        m_process->deleteLater();
    } else {
        connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);
        m_process->kill();
    }
    m_process = nullptr;
    m_backend = Backend::None;
}

void ContentSearch::start(const QString &query)
{
    cancel();
    m_query = query;
    m_found.clear();
    m_buffer.clear();

    const QString home = QDir::homePath();
    m_roots.clear();
    for (auto location : {QStandardPaths::DocumentsLocation, QStandardPaths::DesktopLocation, QStandardPaths::DownloadLocation}) {
        const QString dir = QStandardPaths::writableLocation(location);
        if (!dir.isEmpty() && QDir(dir) != QDir(home) && QFileInfo(dir).isDir() && !m_roots.contains(dir))
            m_roots.append(dir);
    }
    m_homeFiles.clear();
    for (const QFileInfo &f : QDir(home).entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (f.size() <= MaxFileSize)
            m_homeFiles.append(f.filePath());
    }

    const Backend index = indexBackend();
    if (index != Backend::None)
        startIndex(index);
    else if (!m_ripgrep.isEmpty())
        startRipgrep();
    else
        startScan();
}

ContentSearch::Backend ContentSearch::indexBackend() const
{
    // Only ask an index whose daemon is up: otherwise its CLI waits for D-Bus timeouts
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return Backend::None;
    if (bus->isServiceRegistered(QStringLiteral("org.kde.baloo"))
        && !findTool({QStringLiteral("baloosearch6"), QStringLiteral("baloosearch")}).isEmpty())
        return Backend::Baloo;
    if ((bus->isServiceRegistered(QStringLiteral("org.freedesktop.LocalSearch3"))
         || bus->isServiceRegistered(QStringLiteral("org.freedesktop.Tracker3.Miner.Files")))
        && !findTool({QStringLiteral("localsearch"), QStringLiteral("tracker3")}).isEmpty())
        return Backend::LocalSearch;
    return Backend::None;
}

void ContentSearch::startIndex(Backend backend)
{
    m_backend = backend;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    m_process->setProcessEnvironment(env);
    connect(m_process, &QProcess::finished, this, &ContentSearch::indexFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            indexFinished();
    });

    if (backend == Backend::Baloo) {
        m_process->start(findTool({QStringLiteral("baloosearch6"), QStringLiteral("baloosearch")}),
                         {QStringLiteral("-l"), QStringLiteral("30"), m_query});
    } else {
        m_process->start(findTool({QStringLiteral("localsearch"), QStringLiteral("tracker3")}),
                         {QStringLiteral("search"), QStringLiteral("--limit"), QStringLiteral("30"), m_query});
    }
    m_budget.start(IndexBudgetMs);
}

void ContentSearch::indexFinished()
{
    if (!m_process)
        return;
    const QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    stopProcess();
    m_budget.stop();

    // baloosearch prints paths, localsearch prints file:// URIs (maybe with terminal escapes)
    static const QRegularExpression uri(QStringLiteral("file://[^\\s\\x1b\\x07]+"));
    QStringList candidates;
    for (QString line : output.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('/'))) {
            candidates << line;
        } else {
            for (const auto &m : uri.globalMatch(line))
                candidates << QUrl(m.captured()).toLocalFile();
        }
    }

    const QString home = QDir::homePath() + QLatin1Char('/');
    QStringList paths;
    for (const QString &path : std::as_const(candidates)) {
        const QFileInfo info(path);
        if (paths.contains(path) || !path.startsWith(home) || !info.isFile() || info.size() > MaxFileSize)
            continue;
        const QStringList parts = path.mid(home.size()).split(QLatin1Char('/'));
        if (std::any_of(parts.begin(), parts.end() - 1, skippedDir))
            continue;
        paths << path;
    }

    if (paths.isEmpty()) {
        // The index may not cover these folders: fall back to a scan
        m_ripgrep.isEmpty() ? startScan() : startRipgrep();
        return;
    }
    const quint64 serial = m_serial;
    QtConcurrent::run(describeHits, paths, m_query, tr("File Contents"), m_cancelled)
        .then(this, [this, serial](const QList<SearchResult> &results) { deliver(serial, results); });
}

void ContentSearch::startRipgrep()
{
    if (m_roots.isEmpty() && m_homeFiles.isEmpty()) {
        deliver(m_serial, {});
        return;
    }
    m_backend = Backend::Ripgrep;
    m_found.clear();
    m_buffer.clear();

    QStringList args = {
        QStringLiteral("--no-config"), QStringLiteral("--fixed-strings"), QStringLiteral("--ignore-case"),
        QStringLiteral("--line-number"), QStringLiteral("--no-heading"), QStringLiteral("--with-filename"),
        QStringLiteral("--null"), QStringLiteral("--color=never"), QStringLiteral("--no-messages"),
        QStringLiteral("--max-count=1"), QStringLiteral("--max-filesize=2M"),
        QStringLiteral("--max-depth=%1").arg(MaxDepth), QStringLiteral("--max-columns=400"),
        QStringLiteral("--max-columns-preview"), QStringLiteral("--threads=2"),
    };
    for (const QString &dir : skippedDirs())
        args << QStringLiteral("--glob=!%1/").arg(dir);
    args << QStringLiteral("--glob=!build-*/") << QStringLiteral("--") << m_query << m_homeFiles << m_roots;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::readyReadStandardOutput, this, &ContentSearch::readRipgrep);
    connect(m_process, &QProcess::finished, this, [this] {
        readRipgrep();
        if (!m_process)
            return;  // readRipgrep already delivered
        stopProcess();
        deliver(m_serial, m_found);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        stopProcess();
        m_ripgrep.clear();
        startScan();
    });
    m_process->start(m_ripgrep, args);
    m_budget.start(ScanBudgetMs);
}

void ContentSearch::readRipgrep()
{
    if (!m_process)
        return;
    m_buffer += m_process->readAllStandardOutput();
    // Each hit: <path>\0<line number>:<text>\n
    qsizetype end;
    while ((end = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray record = m_buffer.left(end);
        m_buffer.remove(0, end + 1);
        const qsizetype nul = record.indexOf('\0');
        const qsizetype colon = record.indexOf(':', nul + 1);
        if (nul <= 0 || colon < 0)
            continue;
        const QString path = QString::fromUtf8(record.left(nul));
        const QString line = QString::fromUtf8(record.mid(colon + 1));
        m_found.append(makeResult(path, line, m_query, tr("File Contents")));
        if (m_found.size() >= MaxResults) {
            stopProcess();
            deliver(m_serial, m_found);
            return;
        }
    }
}

void ContentSearch::startScan()
{
    m_backend = Backend::None;
    const quint64 serial = m_serial;
    QtConcurrent::run(scanFiles, m_roots, m_homeFiles, m_query, tr("File Contents"), m_cancelled)
        .then(this, [this, serial](const QList<SearchResult> &results) { deliver(serial, results); });
}

void ContentSearch::deliver(quint64 serial, const QList<SearchResult> &results)
{
    if (serial != m_serial)
        return;  // superseded by a newer query
    m_budget.stop();
    ++m_serial;  // one answer per search
    Q_EMIT finished(m_query, results);
}
