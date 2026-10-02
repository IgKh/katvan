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
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", "Menu title")];
    [self setupFileMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Edit", "Menu title")];
    [self setupEditMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"View" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", "Menu title")];
    [self setupViewMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Go" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Go", "Menu title")];
    [self setupGoMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];

    menuItem = [mainMenu addItemWithTitle:@"Window" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", "Menu title")];
    [self setupWindowMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];
    [NSApp setWindowsMenu: submenu];

    menuItem = [mainMenu addItemWithTitle:@"Help" action:nil keyEquivalent:@""];
    submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", "Menu title")];
    [self setupHelpMenu: submenu];
    [mainMenu setSubmenu:submenu forItem:menuItem];
    [NSApp setHelpMenu: submenu];

    [NSApp setMainMenu: mainMenu];
}

+ (void)setupApplicationMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"About Katvan", "Application menu item")
                     action:@selector(showAboutDialog:)
                     keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Preferences...", "Application menu item")
                     action:@selector(showPreferences:)
                     keyEquivalent:@","];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Services", "Application menu submenu title")
                     action:nil
                     keyEquivalent:@""];

    NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
    [menu setSubmenu:servicesMenu forItem:menuItem];
    [NSApp setServicesMenu:servicesMenu];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Hide Katvan", "Application menu item")
                     action:@selector(hide:)
                     keyEquivalent:@"h"];
    [menuItem setTarget:NSApp];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Hide Others", "Application menu item")
                     action:@selector(hideOtherApplications:)
                     keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [menuItem setTarget:NSApp];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Show All", "Application menu item")
                     action:@selector(unhideAllApplications:)
                     keyEquivalent:@""];
    [menuItem setTarget:NSApp];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Quit Katvan", "Application menu item")
                     action:@selector(terminate:)
                     keyEquivalent:@"q"];
    [menuItem setTarget:NSApp];
}

+ (void)setupFileMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"New", "File menu item")
          action:@selector(newDocument:)
          keyEquivalent:@"n"];

    [menu addItemWithTitle:NSLocalizedString(@"Open...", "File menu item")
          action:@selector(openDocument:)
          keyEquivalent:@"o"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Close", "File menu item")
          action:@selector(performClose:)
          keyEquivalent:@"w"];

    [menu addItemWithTitle:NSLocalizedString(@"Save...", "File menu item")
          action:@selector(saveDocument:)
          keyEquivalent:@"s"];

    [menu addItemWithTitle:NSLocalizedString(@"Save As...", "File menu item")
          action:@selector(saveDocumentAs:)
          keyEquivalent:@"S"];

    [menu addItemWithTitle:NSLocalizedString(@"Save All", "File menu item")
          action:@selector(saveAllDocuments:)
          keyEquivalent:@""];

    [menu addItemWithTitle:NSLocalizedString(@"Revert to Saved", "File menu item")
          action:@selector(revertDocumentToSaved:)
          keyEquivalent:@"r"];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Export As", "File menu submenu title")
                     action:nil
                     keyEquivalent:@""];

    NSMenu* exportMenu = [[NSMenu alloc] initWithTitle:@"Export"];
    [menu setSubmenu:exportMenu forItem:menuItem];

    [exportMenu addItemWithTitle:NSLocalizedString(@"PDF...", "Export submenu item")
          action:@selector(exportAsPdf:)
          keyEquivalent:@"E"];

    [exportMenu addItemWithTitle:NSLocalizedString(@"PNG (Single)...", "Export submenu item")
          action:@selector(exportAsSinglePng:)
          keyEquivalent:@""];

    [exportMenu addItemWithTitle:NSLocalizedString(@"PNG (Multiple)...", "Export submenu item")
          action:@selector(exportAsMultiplePng:)
          keyEquivalent:@""];
}

+ (void)setupEditMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    // Selectors for undo and redo actions can't be the usual redo: and undo: because
    // they are intercepted by the NSUndoManager which is part of every responder chain.

    [menu addItemWithTitle:NSLocalizedString(@"Undo", "Edit menu item")
          action:@selector(performUndo:)
          keyEquivalent:@"z"];

    [menu addItemWithTitle:NSLocalizedString(@"Redo", "Edit menu item")
          action:@selector(performRedo:)
          keyEquivalent:@"Z"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Cut", "Edit menu item")
          action:@selector(cut:)
          keyEquivalent:@"x"];

    [menu addItemWithTitle:NSLocalizedString(@"Copy", "Edit menu item")
          action:@selector(copy:)
          keyEquivalent:@"c"];

    [menu addItemWithTitle:NSLocalizedString(@"Paste", "Edit menu item")
          action:@selector(paste:)
          keyEquivalent:@"v"];

    [menu addItemWithTitle:NSLocalizedString(@"Delete", "Edit menu item")
          action:@selector(delete:)
          keyEquivalent:@""];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Complete", "Edit menu item")
                     action:@selector(triggerAutocomplete:)
                     keyEquivalent:@""];
    [menuItem setKeyEquivalent:@"\u001B"]; // Esc
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];

    [menu addItemWithTitle:NSLocalizedString(@"Select All", "Edit menu item")
          action:@selector(selectAll:)
          keyEquivalent:@"a"];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Find", "Edit menu submenu title")
                     action:nil
                     keyEquivalent:@""];

    NSMenu* findMenu = [[NSMenu alloc] initWithTitle:@"Find"];
    [menu setSubmenu:findMenu forItem:menuItem];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find...", "Edit>Find menu item")
                         action:@selector(performTextFinderAction:)
                         keyEquivalent:@"f"];
    [menuItem setTag:NSTextFinderActionShowFindInterface];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find and Replace...", "Edit>Find menu item")
                         action:@selector(performTextFinderAction:)
                         keyEquivalent:@"f"];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [menuItem setTag:NSTextFinderActionShowReplaceInterface];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find Next", "Edit>Find menu item")
                         action:@selector(performTextFinderAction:)
                         keyEquivalent:@"g"];
    [menuItem setTag:NSTextFinderActionNextMatch];

    menuItem = [findMenu addItemWithTitle:NSLocalizedString(@"Find Previous", "Edit>Find menu item")
                         action:@selector(performTextFinderAction:)
                         keyEquivalent:@"G"];
    [menuItem setTag:NSTextFinderActionPreviousMatch];

    // AppKit magically modifies the title of this menu item
    [menu addItemWithTitle:@"Spelling and Grammar" action:@selector(showGuessPanel:) keyEquivalent:@":"];
}

+ (void)setupViewMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Actual Size", "View menu item")
          action:@selector(zoomToActualSize:)
          keyEquivalent:@"0"];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Zoom In", "View menu item")
                     action:@selector(zoomIn:)
                     keyEquivalent:@"."];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Zoom Out", "View menu item")
                     action:@selector(zoomOut:)
                     keyEquivalent:@","];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Invert Preview Colors", "View menu item")
          action:@selector(invertPreviewColors:)
          keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];
}

+ (void)setupGoMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Back", "Go menu item")
          action:@selector(goBack:)
          keyEquivalent:@"["];

    [menu addItemWithTitle:NSLocalizedString(@"Forward", "Go menu item")
          action:@selector(goForward:)
          keyEquivalent:@"]"];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:NSLocalizedString(@"Go to Line...", "Go menu item")
          action:@selector(goToLine:)
          keyEquivalent:@"l"];

    [menu addItemWithTitle:NSLocalizedString(@"Jump to Preview", "Go menu item")
          action:@selector(goToPreview:)
          keyEquivalent:@"j"];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Go to Definition", "Go menu item")
                     action:@selector(goToDefinition:)
                     keyEquivalent:@""];
    [menuItem setKeyEquivalent:@"\uF70F"]; // F12
    [menuItem setKeyEquivalentModifierMask:0];
}

+ (void)setupWindowMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    [menu addItemWithTitle:NSLocalizedString(@"Minimize", "Window menu item")
          action:@selector(performMiniaturize:)
          keyEquivalent:@"m"];

    [menu addItemWithTitle:NSLocalizedString(@"Zoom", "Window menu item")
          action:@selector(performZoom:)
          keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Bring All to Front", "Window menu item")
                     action:@selector(arrangeInFront:)
                     keyEquivalent:@""];
    [menuItem setTarget:NSApp];
}

+ (void)setupHelpMenu:(NSMenu*)menu
{
    NSMenuItem* menuItem;

    menuItem = [menu addItemWithTitle:NSLocalizedString(@"Katvan Help", "Help menu item")
                     action:@selector(showHelp:)
                     keyEquivalent:@"?"];
    [menuItem setTarget:NSApp];

    [menu addItemWithTitle:NSLocalizedString(@"Typst Documentation...", "Help menu item")
          action:@selector(openTypstDocs:)
          keyEquivalent:@""];
}

@end
