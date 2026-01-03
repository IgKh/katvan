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
#import "macshell_mainmenu.h"

#include "katvan_editor.h"

@implementation KatvanMainMenu

+ (void)setupMainMenu
{
    NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenuItem* menuItem;
    NSMenu* submenu;

    menuItem = [mainMenu addItemWithTitle:@"Application" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:@"Application"];
    [self setupApplicationMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"File" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
    [self setupFileMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Edit", nil)];
    [self setupEditMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"View" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
    [self setupViewMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Go" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Go", nil)];
    [self setupGoMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Window" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", nil)];
    [self setupWindowMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];
    [NSApp setWindowsMenu: submenu];

    menuItem = [mainMenu addItemWithTitle:@"Help" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", nil)];
    [self setupHelpMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];
    [NSApp setHelpMenu: submenu];

    [NSApp setMainMenu: mainMenu];
}

+ (void)setupApplicationMenu: (NSMenu*)menu
{
    NSMenuItem* menuItem;

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"About Katvan", nil) action:@selector(showAboutDialog:) keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Preferences...", nil) action:nil keyEquivalent:@","];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Services", nil) action:nil keyEquivalent:@""];

    NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
    [menu setSubmenu:servicesMenu forItem:menuItem];
    [NSApp setServicesMenu:servicesMenu];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Hide Katvan", nil) action:@selector(hide:) keyEquivalent:@"h"];
    [menuItem setTarget:NSApp];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Hide Others", nil) action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [menuItem setTarget:NSApp];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Show All", nil) action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [menuItem setTarget:NSApp];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Quit Katvan", nil) action:@selector(terminate:) keyEquivalent:@"q"];
    [menuItem setTarget:NSApp];
}

+ (void)setupFileMenu: (NSMenu*)menu
{
    [menu addItemWithTitle:NSLocalizedString(@"New", nil) action:@selector(newDocument:) keyEquivalent:@"n"];

    [menu addItemWithTitle:NSLocalizedString(@"Open...", nil) action:@selector(openDocument:) keyEquivalent:@"o"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Close", nil) action:@selector(performClose:) keyEquivalent:@"w"];

    [menu addItemWithTitle:NSLocalizedString(@"Save...", nil) action:@selector(saveDocument:) keyEquivalent:@"s"];

    [menu addItemWithTitle:NSLocalizedString(@"Save As...", nil) action:@selector(saveDocumentAs:) keyEquivalent:@"S"];

    [menu addItemWithTitle:NSLocalizedString(@"Save All", nil) action:@selector(saveAllDocuments:) keyEquivalent:@""];

    [menu addItemWithTitle:NSLocalizedString(@"Revert to Saved", nil) action:@selector(revertDocumentToSaved:) keyEquivalent:@"r"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Export as PDF...", nil) action:@selector(exportAsPdf:) keyEquivalent:@""];
}

+ (void)setupEditMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    // Selectors for undo and redo actions can't be the usual redo: and undo: because
    // they are intercepted by the NSUndoManager which is part of every responder chain.

    [menu addItemWithTitle:NSLocalizedString(@"Undo", nil) action:@selector(performUndo:) keyEquivalent:@"z"];

    [menu addItemWithTitle:NSLocalizedString(@"Redo", nil) action:@selector(performRedo:) keyEquivalent:@"Z"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Cut", nil) action:@selector(cut:) keyEquivalent:@"x"];

    [menu addItemWithTitle:NSLocalizedString(@"Copy", nil) action:@selector(copy:) keyEquivalent:@"c"];

    [menu addItemWithTitle:NSLocalizedString(@"Paste", nil) action:@selector(paste:) keyEquivalent:@"v"];

    [menu addItemWithTitle:NSLocalizedString(@"Delete", nil) action:@selector(delete:) keyEquivalent:@""];

    [menu addItemWithTitle:NSLocalizedString(@"Select All", nil) action:@selector(selectAll:) keyEquivalent:@"a"];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Find", nil) action:nil keyEquivalent:@""];

    NSMenu* findMenu = [[NSMenu alloc] initWithTitle:@"Find"];
    [menu setSubmenu:findMenu forItem:menuItem];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find...", nil) action:@selector(performTextFinderAction:) keyEquivalent:@"f"];
    [menuItem setTag:NSTextFinderActionShowFindInterface];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find and Replace...", nil) action:@selector(performTextFinderAction:) keyEquivalent:@"f"];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [menuItem setTag:NSTextFinderActionShowReplaceInterface];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find Next", nil) action:@selector(performTextFinderAction:) keyEquivalent:@"g"];
    [menuItem setTag:NSTextFinderActionNextMatch];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find Previous", nil) action:@selector(performTextFinderAction:) keyEquivalent:@"G"];
    [menuItem setTag:NSTextFinderActionPreviousMatch];

    [menu addItemWithTitle:NSLocalizedString(@"Spell Checking...", nil) action:nil keyEquivalent:@""];
}

+ (void)setupViewMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Actual Size", nil) action:@selector(zoomToActualSize:) keyEquivalent:@"0"];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Zoom In", nil) action:@selector(zoomIn:) keyEquivalent:@"."];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Zoom In", nil) action:@selector(zoomOut:) keyEquivalent:@","];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Invert Preview Colors", nil) action:@selector(invertPreviewColors:) keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];
}

+ (void)setupGoMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Back", nil) action:@selector(goBack:) keyEquivalent:@"["];
    [menu addItemWithTitle:NSLocalizedString(@"Forward", nil) action:@selector(goForward:) keyEquivalent:@"]"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Go to Line...", nil) action:@selector(goToLine:) keyEquivalent:@"l"];
    [menu addItemWithTitle:NSLocalizedString(@"Jump to Preview", nil) action:@selector(goToPreview:) keyEquivalent:@"j"];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Go to Definition", nil) action:@selector(goToDefinition:) keyEquivalent:@""];
    [menuItem setKeyEquivalent:@"\uF70F"]; // F12
    [menuItem setKeyEquivalentModifierMask:0];
}

+ (void)setupWindowMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Minimize", nil) action:@selector(performMiniaturize:) keyEquivalent:@"m"];

    [menu addItemWithTitle:NSLocalizedString(@"Zoom", nil) action:@selector(performZoom:) keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Bring All to Front", nil) action:@selector(arrangeInFront:) keyEquivalent:@""];
    [menuItem setTarget:NSApp];
}

+ (void)setupHelpMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Katvan Help", nil) action:@selector(showHelp:) keyEquivalent:@"?"];
    [menuItem setTarget:NSApp];

    [menu addItemWithTitle:NSLocalizedString(@"Typst Documentation...", nil) action:@selector(openTypstDocs:) keyEquivalent:@""];
}

@end
