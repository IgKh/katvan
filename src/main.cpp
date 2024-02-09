/*
 * This file is part of Katvan
 * Copyright (c) 2024 Igor Khanin
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
#include "katvan_version.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

void setupPortableMode()
{
    QString settingsPath = QCoreApplication::applicationDirPath();
    qDebug() << "Running in portable mode, settings stored at" << settingsPath;

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsPath);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Katvan");
    QCoreApplication::setApplicationName("Katvan");

    QString version = katvan::KATVAN_VERSION;
    if (!katvan::KATVAN_GIT_SHA.isEmpty()) {
        version += "-" + katvan::KATVAN_GIT_SHA;
    }
    QCoreApplication::setApplicationVersion(version);

    QCommandLineParser parser;
    parser.addPositionalArgument("file", "File to open");
    parser.addOptions({
        QCommandLineOption{ "heb", "Force Hebrew UI" },
        QCommandLineOption{ "portable", "Use portable mode" },
        QCommandLineOption{ "no-portable", "Don't use portable mode, even if this is a portable build" }
    });
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);

#ifdef KATVAN_PORTABLE_BUILD
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
    if (locale.language() != QLocale::English) {
        if (translator.load(locale, "katvan", "_", ":/i18n")) {
            QCoreApplication::installTranslator(&translator);
        }
    }

    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qtbase", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&qtTranslator);
    }

    katvan::MainWindow wnd;
    wnd.show();

    if (!parser.positionalArguments().isEmpty()) {
        wnd.loadFile(parser.positionalArguments().at(0));
    }

    return app.exec();
}
