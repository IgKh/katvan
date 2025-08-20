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
#import "macshell_appdelegate.h"

#include "katvan_text_utils.h"

#include <QApplication>

#import <AppKit/AppKit.h>

int main(int argc, char** argv)
{
    // Set our delegate (and implicitly create the NSApplication instance)
    // before Qt does any initialization
    KatvanAppDelegate* delegate = [[KatvanAppDelegate alloc] init];
    [NSApplication sharedApplication].delegate = delegate;

    // Our application isn't actually a plugin, but AA_PluginApplication ensures
    // that Qt's application delegate and menu bar are not installed. We depend
    // on AppKit handling all lifecycle events as per default, otherwise some
    // of the Cocoa Document Framework magic doesn't happen.
    QCoreApplication::setAttribute(Qt::AA_PluginApplication);
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationDomain("katvan.app");
    QCoreApplication::setOrganizationName("Katvan");
    QCoreApplication::setApplicationName("Katvan");

    katvan::utils::loadAuxiliaryFonts();

    app.setQuitOnLastWindowClosed(false);

    return app.exec();
}
