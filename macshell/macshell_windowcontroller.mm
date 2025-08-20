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
#import "macshell_editorview.h"
#include <Foundation/Foundation.h>
#import "macshell_previewer.h"
#import "macshell_typstdocument.h"
#import "macshell_windowcontroller.h"

#include "katvan_completionmanager.h"
#include "katvan_diagnosticsmodel.h"
#include "katvan_editor.h"
#include "katvan_typstdriverwrapper.h"

@interface KatvanWindowController ()

@property NSSplitViewController* splitViewController;
@property KatvanEditorView* editorView;
@property KatvanPreviewer* previewer;

@property katvan::Document* textDocument;
@property katvan::TypstDriverWrapper* driver;

@property QString documentFilePath;

@end

@implementation KatvanWindowController

- (instancetype)initWithDocument:(katvan::Document*)textDocument initialURL:(NSURL*)url
{
    NSRect frame = NSMakeRect(0, 0, 800, 500);
    NSWindow* window  = [[NSWindow alloc] initWithContentRect:frame
                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView
                        backing:NSBackingStoreBuffered
                        defer:NO];

    self = [super initWithWindow:window];
    if (self) {
        [window center];

        self.textDocument = textDocument;
        self.driver = new katvan::TypstDriverWrapper();

        [self setupViewsWithDocument:textDocument];
        [self setupUI];

        [self documentDidExplicitlySaveInURL:url forced:YES];
    }
    return self;
}

- (void)dealloc
{
    delete self.driver;
}

- (void)setupViewsWithDocument:(katvan::Document*)textDocument
{
    self.editorView = [[KatvanEditorView alloc] initWithDocument:textDocument];
    self.previewer = [[KatvanPreviewer alloc] initWithDriver:self.driver];

    QObject::connect(self.textDocument, &katvan::Document::contentEdited,
                    self.driver, &katvan::TypstDriverWrapper::applyContentEdit);
    QObject::connect(self.textDocument, &katvan::Document::contentModified,
                    self.driver, &katvan::TypstDriverWrapper::updatePreview);

    QObject::connect(self.driver, &katvan::TypstDriverWrapper::jumpToPreview,
                    self.previewer.previewerView, &katvan::PreviewerView::jumpTo);
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::jumpToEditor,
                    self.editorView.editor, qOverload<int, int>(&katvan::Editor::goToBlock));
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::showEditorToolTip,
                    self.editorView.editor, &katvan::Editor::showToolTip);
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::showEditorToolTipAtLocation,
                    self.editorView.editor, &katvan::Editor::showToolTipAtLocation);
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::completionsReady,
                    self.editorView.editor->completionManager(), &katvan::CompletionManager::completionsReady);

    __weak __typeof__(self) weakSelf = self;
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::compilationStatusChanged,
                     self.driver, [weakSelf]() {
                        weakSelf.editorView.editor->setSourceDiagnostics(weakSelf.driver->diagnosticsModel()->sourceDiagnostics());
                     });

    QObject::connect(self.editorView.editor, &katvan::Editor::toolTipRequested,
                    self.driver, &katvan::TypstDriverWrapper::requestToolTip);
    QObject::connect(self.editorView.editor, &katvan::Editor::goToDefinitionRequested,
                    self.driver, &katvan::TypstDriverWrapper::searchDefinition);
    QObject::connect(self.editorView.editor->completionManager(), &katvan::CompletionManager::completionsRequested,
                    self.driver, &katvan::TypstDriverWrapper::requestCompletions);
}

- (void)setupUI
{
    self.splitViewController = [[NSSplitViewController alloc] init];
    self.splitViewController.splitView.dividerStyle = NSSplitViewDividerStyleThin;

    NSSplitViewItem* editorItem = [NSSplitViewItem splitViewItemWithViewController:self.editorView];
    [self.splitViewController addSplitViewItem:editorItem];

    NSSplitViewItem* previewerItem = [NSSplitViewItem splitViewItemWithViewController:self.previewer];
    [previewerItem setCanCollapse:YES];
    [self.splitViewController addSplitViewItem:previewerItem];

    [self.window setContentViewController:self.splitViewController];

    NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"KatvanWindowToolbar"];
    toolbar.delegate = self;
    toolbar.allowsUserCustomization = NO;
    toolbar.displayMode = NSToolbarDisplayModeIconOnly;
    self.window.toolbar = toolbar;
}

- (NSArray<NSString*>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
    return @[
        @"katvan.toolbar.editor.insert",
        @"katvan.toolbar.separator",
        @"katvan.toolbar.previewer.zoomout",
        @"katvan.toolbar.previewer.zoomin",
    ];
}

- (NSArray<NSString*>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar
{
    return [self toolbarAllowedItemIdentifiers:toolbar];
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar
                  itemForItemIdentifier:(NSString*)itemIdentifier
                  willBeInsertedIntoToolbar:(BOOL)flag
{
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.editor.insert"]) {
        NSMenuToolbarItem* item = [[NSMenuToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Insert", "Opens insert menu for the editor");
        item.image = [NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:@"Plus sign"];
        item.menu = [self.editorView createInsertMenu];
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.separator"]) {
        NSTrackingSeparatorToolbarItem* item = [NSTrackingSeparatorToolbarItem
            trackingSeparatorToolbarItemWithIdentifier:itemIdentifier
            splitView:self.splitViewController.splitView
            dividerIndex:0];
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.zoomout"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom Out", "Zoom out the preview");
        item.image = [NSImage imageWithSystemSymbolName:@"minus.magnifyingglass" accessibilityDescription:@"Maginfying glass with minus"];
        item.target = self.previewer;
        item.action = @selector(zoomOut:);
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.zoomin"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom In", "Zoom in the preview");
        item.image = [NSImage imageWithSystemSymbolName:@"plus.magnifyingglass" accessibilityDescription:@"Magnifying glass with plus"];
        item.target = self.previewer;
        item.action = @selector(zoomIn:);
        return item;
    }
    return nil;
}

- (void)documentDidExplicitlySaveInURL:(NSURL*)url forced:(BOOL)forced
{
    QString filePath;
    if (url != nil) {
        filePath = QString::fromNSString(url.path);
    }

    if (self.documentFilePath != filePath || forced) {
        self.documentFilePath = filePath;
        self.driver->resetInputFile(filePath);
        self.driver->setSource(self.textDocument->textForPreview());
    }

    self.editorView.editor->checkForModelines();
}

@end
