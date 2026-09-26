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

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <memory>

#include "resultsmodel.h"

class QProcess;

// Finds text inside the user's documents. Uses the desktop index when its daemon
// runs (Baloo, LocalSearch/Tracker), otherwise ripgrep, otherwise a bounded scan
// on a worker thread. Every search has a time budget and is cancelled by the next one.
class ContentSearch : public QObject
{
    Q_OBJECT

public:
    explicit ContentSearch(QObject *parent = nullptr);
    ~ContentSearch() override;

    void start(const QString &query);
    void cancel();

Q_SIGNALS:
    void finished(const QString &query, const QList<SearchResult> &results);

private:
    enum class Backend { None, Baloo, LocalSearch, Ripgrep };

    Backend indexBackend() const;
    void startIndex(Backend backend);
    void startRipgrep();
    void startScan();
    void readRipgrep();
    void indexFinished();
    void deliver(quint64 serial, const QList<SearchResult> &results);
    void stopProcess();

    QString m_query;
    quint64 m_serial = 0;
    Backend m_backend = Backend::None;
    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    QList<SearchResult> m_found;
    QStringList m_roots;       // directories searched recursively
    QStringList m_homeFiles;   // text files at the top of the home folder
    QTimer m_budget;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    QString m_ripgrep;
};
