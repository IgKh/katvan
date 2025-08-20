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

#include "katvan_text_utils.h"
#include "katvan_version.h"

#include "typstdriver_packagemanager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
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

void loadTranslations(const QLocale& locale)
{
    QStringList translationPaths;
    translationPaths.append(":/i18n");

#if defined(Q_OS_WINDOWS)
    translationPaths.append(QCoreApplication::applicationDirPath() + "/translations");
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& dir : locations) {
        translationPaths.append(dir + "/katvan/translations");
    }
#endif

    static QTranslator translator;
    for (const QString& directory : std::as_const(translationPaths)) {
        if (translator.load(locale, "katvan", "_", directory)) {
            qDebug() << "Loaded application translations from" << translator.filePath();
            QCoreApplication::installTranslator(&translator);
            break;
        }
    }

    QString qtTranslationPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    static QTranslator qtTranslator;
    bool ok = qtTranslator.load(locale, "qtbase", "_", qtTranslationPath);
    if (!ok) {
        ok = qtTranslator.load(locale, "qt", "_", qtTranslationPath);
    }

    if (ok) {
        qDebug() << "Loaded Qt translations from" << qtTranslator.filePath();
        QCoreApplication::installTranslator(&qtTranslator);
    }
}

int main(int argc, char** argv)
{
#ifdef Q_OS_MACOS
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationDomain("katvan.app");
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

    loadTranslations(locale);
    katvan::utils::loadAuxiliaryFonts();

#ifdef Q_OS_WINDOWS
    // Starting from Qt 6.8.1 the Windows11 style works on Windows 10 too. Prefer
    // it since it supports dark mode.
    if (QLibraryInfo::version() >= QVersionNumber(6, 8, 1)) {
        if (app.style()->name() == "windowsvista") {
            app.setStyle("windows11");
        }
    }
#endif

    katvan::MainWindow wnd;
    QCoreApplication::connect(&app, &QGuiApplication::commitDataRequest, &wnd, &katvan::MainWindow::commitSession);

    wnd.show();

    QTimer::singleShot(0, &wnd, [&parser, &wnd]() {
        if (!parser.positionalArguments().isEmpty()) {
            wnd.loadFile(parser.positionalArguments().at(0));
        }
        else {
            wnd.newFile();
        }
    });

    return app.exec();
}
