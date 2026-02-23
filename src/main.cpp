// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Aaron Rainbolt <arraybolt3@gmail.com>
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

#include <libvirt/libvirt.h>

#include <KAboutData>
#include <KIconTheme>
#include <KLocalizedContext>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KirigamiAddons/App/KirigamiAppDefaults>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>
#include <iostream>

#include "domainconfig.h"
#include "karton.h"
#include "karton_debug.h"
#include "osinfoconfig.h"
#include "vmlistmodel.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KirigamiAppDefaults::apply(&app);

    KLocalizedString::setApplicationDomain("karton");
    KAboutData about(QStringLiteral("karton"),
                     i18nc("Application name", "Karton"),
                     QStringLiteral("1.0"),
                     i18nc("Application description", "Manage and run virtual machines with ease"),
                     KAboutLicense::GPL,
                     i18nc("Copyright statement", "(C) KDE"));

    about.addAuthor(i18nc("author name", "Aaron Rainbolt"), i18nc("author role", "Initial developer"), QStringLiteral("arraybolt3@gmail.com"));
    about.addAuthor(i18nc("author name", "Derek Lin"), i18nc("author role", "Developer"), QStringLiteral("derekhongdalin@gmail.com"));
    about.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));

    KAboutData::setApplicationData(about);
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.kde.karton")));

    QQmlApplicationEngine engine;

    qCInfo(KARTON_DEBUG) << "Hello! Starting Karton...";

    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kde.karton", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
