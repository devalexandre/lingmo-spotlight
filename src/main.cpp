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

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#include "searchengine.h"
#include "spotlight.h"

static const QString DBusService = QStringLiteral("com.lingmo.Spotlight");
static const QString DBusPath = QStringLiteral("/Spotlight");

// The Lingmo platform theme doesn't always hand the icon theme to QGuiApplication
// processes (the dock works around it too): use the one set in Lingmo Settings.
static void applyIconTheme()
{
    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != QLatin1String("hicolor"))
        return;
    QSettings theme(QStringLiteral("lingmoos"), QStringLiteral("theme"));
    const bool dark = theme.value(QStringLiteral("DarkMode"), false).toBool();
    const QStringList candidates = {
        theme.value(dark ? QStringLiteral("DarkIconTheme") : QStringLiteral("IconTheme")).toString(),
        dark ? QStringLiteral("Crule-dark") : QStringLiteral("Crule"),
        QStringLiteral("breeze"),
    };
    for (const QString &name : candidates) {
        if (!name.isEmpty()
            && !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                       QStringLiteral("icons/%1/index.theme").arg(name)).isEmpty()) {
            QIcon::setThemeName(name);
            break;
        }
    }
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lingmo-spotlight"));
    app.setDesktopFileName(QStringLiteral("lingmo-spotlight"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption background(QStringLiteral("background"), QStringLiteral("Start hidden (session autostart)"));
    parser.addOption(background);
    parser.process(app);
    applyIconTheme();

    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("lingmo-spotlight"), QStringLiteral("_"),
                        QStringLiteral(TRANSLATIONS_DIR)))
        app.installTranslator(&translator);

    // One resident instance: a second launch just toggles the popup (Meta+Space)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(DBusService)) {
        QDBusInterface iface(DBusService, DBusPath, DBusService, bus);
        if (!parser.isSet(background))
            iface.call(QStringLiteral("toggle"));
        return 0;
    }

    SearchEngine engine;
    Spotlight spotlight;
    bus.registerObject(DBusPath, &spotlight, QDBusConnection::ExportAllSlots);

    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("searchEngine"), &engine);
    qml.rootContext()->setContextProperty(QStringLiteral("spotlight"), &spotlight);
    qml.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (qml.rootObjects().isEmpty())
        return 1;

    spotlight.setWindow(qobject_cast<QQuickWindow *>(qml.rootObjects().first()));
    if (!parser.isSet(background))
        spotlight.show();

    return app.exec();
}
