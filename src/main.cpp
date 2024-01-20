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
#include <QTranslator>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Katvan");
    QCoreApplication::setApplicationName("Katvan");
    QCoreApplication::setApplicationVersion(katvan::KATVAN_VERSION + "-" + katvan::KATVAN_GIT_SHA);

    QCommandLineOption forceHebrewUI("heb", "Force Hebrew UI");

    QCommandLineParser parser;
    parser.addPositionalArgument("file", "File to open");
    parser.addOption(forceHebrewUI);
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);

    QLocale locale = QLocale::system();
    if (parser.isSet(forceHebrewUI)) {
        locale = QLocale(QLocale::Hebrew);
    }

    QTranslator translator;
    if (translator.load(locale, "katvan", "_", ":/i18n")) {
        QCoreApplication::installTranslator(&translator);
    }

    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qtbase", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    katvan::MainWindow wnd;
    wnd.show();

    if (!parser.positionalArguments().isEmpty()) {
        wnd.loadFile(parser.positionalArguments().at(0));
    }

    return app.exec();
}
