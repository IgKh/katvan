/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2026 Igor Khanin
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
#import "macshell_colorconverter.h"
#import "macshell_mainmenu.h"
#import "macshell_settingsdialog.h"
#import "macshell_settingsmanager.h"

#include "katvan_aboutdialog.h"
#include "katvan_text_utils.h"
#include "katvan_version.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QTranslator>

#import <AppKit/AppKit.h>

void loadQtTranslations()
{
    // We only need to load Qt Ligusit translations for the core library and Qt
    // itself. Translations for macshell strings are automatically loaded by
    // AppKit from strings files in the application bundle.
    QLocale locale = QLocale::system();

    static QTranslator coreTranslator;
    if (coreTranslator.load(locale, "core", "_", ":/i18n")) {
        QCoreApplication::installTranslator(&coreTranslator);
    }

    QString qtTranslationPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    static QTranslator qtTranslator;
    bool ok = qtTranslator.load(locale, "qtbase", "_", qtTranslationPath);
    if (!ok) {
        ok = qtTranslator.load(locale, "qt", "_", qtTranslationPath);
    }

    if (ok) {
        QCoreApplication::installTranslator(&qtTranslator);
    }
}

@implementation KatvanAppDelegate
{
    QApplication* d_app;
    KatvanSettingsDialog* d_settingsDialog;
}

- (instancetype)initWithArgc:(int)argc Argv:(char**)argv
{
    self = [super init];
    if (self) {
        // Workaround for QTBUG-144944
        qputenv("QT_MAC_NO_CONTAINER_LAYER", "1");

        // Our application isn't actually a plugin, but AA_PluginApplication ensures
        // that Qt's application delegate and menu bar are not installed. We depend
        // on AppKit handling all lifecycle events as per default, otherwise some
        // of the Cocoa Document Framework magic doesn't happen.
        QCoreApplication::setAttribute(Qt::AA_PluginApplication);
        QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

        d_app = new QApplication(argc, argv);
        QCoreApplication::setOrganizationDomain("katvan.app");
        QCoreApplication::setOrganizationName("Katvan");
        QCoreApplication::setApplicationName("Katvan");

        QString version = katvan::KATVAN_VERSION;
        if (!katvan::KATVAN_GIT_SHA.isEmpty()) {
            version += "-" + katvan::KATVAN_GIT_SHA;
        }
        QCoreApplication::setApplicationVersion(version);

        new ColorUtiMimeConverter();
        katvan::utils::loadAuxiliaryFonts();
        loadQtTranslations();

        KatvanSettingsManager::instance().reloadSettings();

        d_app->setQuitOnLastWindowClosed(false);
    }
    return self;
}

- (void)applicationWillFinishLaunching:(NSNotification*)notification
{
    [KatvanMainMenu setupMainMenu];
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
    delete d_app;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app
{
    return YES;
}

- (void)showAboutDialog:(id)sender
{
    katvan::AboutDialog dialog;
    dialog.exec();
}

- (void)showPreferences:(id)sender
{
    if (!d_settingsDialog) {
        d_settingsDialog = [[KatvanSettingsDialog alloc] init];
    }
    [d_settingsDialog showDialog];
}

- (void)showGuessPanel:(id)sender
{
    NSPanel* panel = [[NSSpellChecker sharedSpellChecker] spellingPanel];
    if (panel.visible) {
        [panel orderOut:self];
    }
    else {
        [panel orderFront:self];
    }
}

- (void)openTypstDocs:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://typst.app/docs/"]];
}

- (void)settingsUpdated:(id)sender
{
    for (NSDocument* document in NSDocumentController.sharedDocumentController.documents) {
        for (NSWindowController* controller in document.windowControllers) {
            [controller performSelector:@selector(readSettings)];
        }
    }
}

@end
