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

#include "resultsmodel.h"

int ResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_results.size();
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};

    const SearchResult &r = m_results.at(index.row());
    switch (role) {
    case TitleRole: return r.title;
    case SubtitleRole: return r.subtitle;
    case IconRole: return r.icon;
    case CategoryRole: return r.category;
    case KindRole: return int(r.kind);
    }
    return {};
}

QHash<int, QByteArray> ResultsModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {IconRole, "iconName"},
        {CategoryRole, "category"},
        {KindRole, "kind"},
    };
}

void ResultsModel::setResults(const QList<SearchResult> &results)
{
    beginResetModel();
    m_results = results;
    endResetModel();
    Q_EMIT countChanged();
}

const SearchResult *ResultsModel::at(int row) const
{
    return (row >= 0 && row < m_results.size()) ? &m_results.at(row) : nullptr;
}
