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

#include <QObject>
#include <QPointer>

class QQuickWindow;

// Owns the popup window and exposes it on D-Bus as com.lingmo.Spotlight /Spotlight
class Spotlight : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.lingmo.Spotlight")

public:
    explicit Spotlight(QObject *parent = nullptr);
    void setWindow(QQuickWindow *window);

public Q_SLOTS:
    void toggle();
    void show();
    void hide();

private:
    QPointer<QQuickWindow> m_window;
};
