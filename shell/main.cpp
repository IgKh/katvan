/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2025 Igor Khanin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "katvan_mainwindow.h"
#include "katvan_spellchecker.h"
#include "katvan_utils.h"

#include "katvan_version.h"

#include "typstdriver_packagemanager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

void setupPortableMode()
{
    bool isInstalled;
    QString settingsPath = katvan::utils::getApplicationDir(isInstalled);

    if (isInstalled) {
        qWarning() << "Application is considered installed at" << settingsPath << "- not enabling portable mode!";
        return;
    }
    qDebug() << "Running in portable mode, settings stored at" << settingsPath;

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsPath);

    katvan::SpellChecker::instance()->setPersonalDictionaryLocation(settingsPath + "/Katvan");
    katvan::typstdriver::PackageManager::setDownloadCacheLocation(settingsPath + "/Katvan/cache");
}

int main(int argc, char** argv)
{
#ifdef Q_OS_MACOS
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Katvan");
    QCoreApplication::setApplicationName("Katvan");

    QString version = katvan::KATVAN_VERSION;
    if (!katvan::KATVAN_GIT_SHA.isEmpty()) {
        version += "-" + katvan::KATVAN_GIT_SHA;
    }
#if defined(KATVAN_PORTABLE_BUILD) && !defined(KATVAN_DISABLE_PORTABLE)
    version += "-portable";
#endif
    QCoreApplication::setApplicationVersion(version);

    QCommandLineParser parser;
    parser.addPositionalArgument("file", "File to open");
    parser.addOptions({
#if !defined(KATVAN_DISABLE_PORTABLE)
        QCommandLineOption{ "portable", "Use portable mode" },
        QCommandLineOption{ "no-portable", "Don't use portable mode, even if this is a portable build" },
#endif
        QCommandLineOption{ "heb", "Force Hebrew UI" }
    });
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);

#if defined(KATVAN_DISABLE_PORTABLE)
    bool enablePortableMode = false;
#elif defined(KATVAN_PORTABLE_BUILD)
    bool enablePortableMode = !parser.isSet("no-portable");
#else
    bool enablePortableMode = parser.isSet("portable") && !parser.isSet("no-portable");
#endif
    if (enablePortableMode) {
        setupPortableMode();
    }

    QLocale locale = QLocale::system();
    if (parser.isSet("heb")) {
        locale = QLocale(QLocale::Hebrew);
    }

    QTranslator translator;
    if (translator.load(locale, "katvan", "_", ":/i18n")) {
        QCoreApplication::installTranslator(&translator);
    }

    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qtbase", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&qtTranslator);
    }

    int rc = QFontDatabase::addApplicationFont(":/assets/KatvanControl.otf");
    if (rc < 0) {
        qWarning() << "Failed to load control character font";
    }

    katvan::MainWindow wnd;
    QCoreApplication::connect(&app, &QGuiApplication::commitDataRequest, &wnd, &katvan::MainWindow::commitSession);

    wnd.show();

    if (!parser.positionalArguments().isEmpty()) {
        wnd.loadFile(parser.positionalArguments().at(0));
    }
    else {
        wnd.newFile();
    }

    return app.exec();
}
