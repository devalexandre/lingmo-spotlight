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

#include "spotlight.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

#include <KWindowSystem>
#include <KX11Extras>

Spotlight::Spotlight(QObject *parent)
    : QObject(parent)
{
}

void Spotlight::setWindow(QQuickWindow *window)
{
    m_window = window;
}

void Spotlight::toggle()
{
    if (m_window && m_window->isVisible())
        hide();
    else
        show();
}

void Spotlight::show()
{
    if (!m_window)
        return;
    // Open on the screen under the mouse, a quarter of the way down like macOS
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->geometry();
    m_window->setScreen(screen);
    m_window->setX(area.x() + (area.width() - m_window->width()) / 2);
    m_window->setY(area.y() + area.height() / 4);

    QMetaObject::invokeMethod(m_window, "reset");
    m_window->show();
    // Not an app window: keep it out of the dock and the status bar title
    if (KWindowSystem::isPlatformX11())
        KX11Extras::setState(m_window->winId(), NET::SkipTaskbar | NET::SkipPager | NET::KeepAbove);
    m_window->raise();
    m_window->requestActivate();
}

void Spotlight::hide()
{
    if (m_window)
        m_window->hide();
}
