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

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct SearchResult {
    enum Kind { App, Calculator, Setting, File, Web, Conversion, Content, Info };

    Kind kind;
    QString title;
    QString subtitle;
    QString icon;      // icon theme name or absolute path
    QString category;  // section header shown in the list
    QString payload;   // what to launch/open/copy
    double score = 0;
};

class ResultsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { TitleRole = Qt::UserRole + 1, SubtitleRole, IconRole, CategoryRole, KindRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setResults(const QList<SearchResult> &results);
    const SearchResult *at(int row) const;

Q_SIGNALS:
    void countChanged();

private:
    QList<SearchResult> m_results;
};
