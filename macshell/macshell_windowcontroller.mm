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
#import "macshell_editorview.h"
#import "macshell_outlineview.h"
#import "macshell_previewer.h"
#import "macshell_sidebar.h"
#import "macshell_typstdocument.h"
#import "macshell_windowcontroller.h"

#include "katvan_completionmanager.h"
#include "katvan_diagnosticsmodel.h"
#include "katvan_editor.h"
#include "katvan_symbolpicker.h"
#include "katvan_typstdriverwrapper.h"

@interface KatvanWindowController ()

@property (nonatomic) NSSplitViewController* splitViewController;
@property (nonatomic) KatvanEditorView* editorView;
@property (nonatomic) KatvanPreviewer* previewer;
@property (nonatomic) KatvanSidebar* sidebar;
@property (nonatomic) KatvanOutlineView* outlineView;

@property (nonatomic) katvan::Document* textDocument;
@property (nonatomic) katvan::TypstDriverWrapper* driver;
@property (nonatomic) katvan::SymbolPicker* symbolPicker;

@property (nonatomic) QString documentFilePath;

@end

@implementation KatvanWindowController

- (instancetype)initWithDocument:(katvan::Document*)textDocument initialURL:(NSURL*)url
{
    NSRect frame = NSMakeRect(0, 0, 800, 500);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:styleMask
                                         backing:NSBackingStoreBuffered
                                         defer:NO];

    self = [super initWithWindow:window];
    if (self) {
        [window setTabbingMode:NSWindowTabbingModeDisallowed];
        [window center];

        self.textDocument = textDocument;
        self.driver = new katvan::TypstDriverWrapper();
        self.symbolPicker = nullptr;

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

    self.outlineView = [[KatvanOutlineView alloc] init];
    self.outlineView.target = self;

    __weak __typeof__(self) weakSelf = self;

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

    QObject::connect(self.driver, &katvan::TypstDriverWrapper::compilationStatusChanged,
                     self.driver, [weakSelf]() {
        weakSelf.editorView.editor->setSourceDiagnostics(weakSelf.driver->diagnosticsModel()->sourceDiagnostics());
    });
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::outlineUpdated,
                     self.driver, [weakSelf](katvan::typstdriver::OutlineNode* outline) {
        [weakSelf.outlineView setOutline:outline];
    });

    QObject::connect(self.editorView.editor, &katvan::Editor::toolTipRequested,
                     self.driver, &katvan::TypstDriverWrapper::requestToolTip);
    QObject::connect(self.editorView.editor, &katvan::Editor::goToDefinitionRequested,
                     self.driver, &katvan::TypstDriverWrapper::searchDefinition);
    QObject::connect(self.editorView.editor->completionManager(), &katvan::CompletionManager::completionsRequested,
                     self.driver, &katvan::TypstDriverWrapper::requestCompletions);

    QObject::connect(self.editorView.editor, &QTextEdit::cursorPositionChanged,
                     self.editorView.editor, [weakSelf]() {
        int line = weakSelf.editorView.editor->textCursor().blockNumber();
        [weakSelf.outlineView selectEntryForLine:line];
    });
    QObject::connect(self.editorView.editor, &katvan::Editor::showSymbolPicker,
                     self.editorView.editor, [weakSelf]() {
        [weakSelf showSymbolPicker];
    });
    QObject::connect(self.editorView.editor, &katvan::Editor::showColorPicker,
                     self.editorView.editor, [weakSelf]() {
        [weakSelf.editorView showColorPicker];
    });
}

- (void)setupUI
{
    [self setupSidebar];

    self.splitViewController = [[NSSplitViewController alloc] init];
    self.splitViewController.splitView.dividerStyle = NSSplitViewDividerStyleThin;

    NSSplitViewItem* sidebarItem = [NSSplitViewItem sidebarWithViewController:self.sidebar];
    [sidebarItem setPreferredThicknessFraction:0.2];
    [sidebarItem setTitlebarSeparatorStyle:NSTitlebarSeparatorStyleLine];
    [self.splitViewController addSplitViewItem:sidebarItem];

    NSSplitViewItem* editorItem = [NSSplitViewItem splitViewItemWithViewController:self.editorView];
    [editorItem setPreferredThicknessFraction:0.4];
    [self.splitViewController addSplitViewItem:editorItem];

    NSSplitViewItem* previewerItem = [NSSplitViewItem splitViewItemWithViewController:self.previewer];
    [previewerItem setPreferredThicknessFraction:0.4];
    [previewerItem setCanCollapse:YES];
    [self.splitViewController addSplitViewItem:previewerItem];

    [self.window setContentViewController:self.splitViewController];

    NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"KatvanWindowToolbar"];
    toolbar.delegate = self;
    toolbar.allowsUserCustomization = NO;
    toolbar.displayMode = NSToolbarDisplayModeIconOnly;
    self.window.toolbar = toolbar;
}

- (void)setupSidebar
{
    self.sidebar = [[KatvanSidebar alloc] init];

    [self.sidebar addTabController:self.outlineView
                  icon:[NSImage imageWithSystemSymbolName:@"document.viewfinder.fill"
                                accessibilityDescription:@"Document page in a targeting frame"]
                  toolTip:NSLocalizedString(@"Document Outline", nil)];

    // TODO persist as part of restorable UI state
    [self.sidebar ensureControllerSelected:self.outlineView];
}

- (NSArray<NSString*>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
    return @[
        NSToolbarToggleSidebarItemIdentifier,
        NSToolbarSidebarTrackingSeparatorItemIdentifier,
        @"katvan.toolbar.editor.insert",
        @"katvan.toolbar.separator",
        @"katvan.toolbar.previewer.zoomout",
        @"katvan.toolbar.previewer.zoomlevel",
        @"katvan.toolbar.previewer.zoomin",
        NSToolbarFlexibleSpaceItemIdentifier,
        @"katvan.toolbar.previewer.invertcolors",
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
        item.toolTip = NSLocalizedString(@"Insert", nil);
        item.image = [NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:@"Plus sign"];
        item.menu = [self.editorView createInsertMenu];
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.separator"]) {
        NSTrackingSeparatorToolbarItem* item = [NSTrackingSeparatorToolbarItem
            trackingSeparatorToolbarItemWithIdentifier:itemIdentifier
            splitView:self.splitViewController.splitView
            dividerIndex:(self.splitViewController.splitViewItems.count - 2)];
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.zoomout"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom Out", nil);
        item.toolTip = NSLocalizedString(@"Zoom out the preview", nil);
        item.image = [NSImage imageWithSystemSymbolName:@"minus.magnifyingglass" accessibilityDescription:@"Magnifying glass with minus"];
        item.target = self.previewer;
        item.action = @selector(zoomOut:);
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.zoomlevel"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom Level", nil);
        item.toolTip = NSLocalizedString(@"Set the zoom level of the preview", nil);
        item.view = [self.previewer makeZoomLevelPopup];
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.zoomin"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Zoom In", nil);
        item.toolTip = NSLocalizedString(@"Zoom in the preview", nil);
        item.image = [NSImage imageWithSystemSymbolName:@"plus.magnifyingglass" accessibilityDescription:@"Magnifying glass with plus"];
        item.target = self.previewer;
        item.action = @selector(zoomIn:);
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.previewer.invertcolors"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Invert Colors", nil);
        item.toolTip = NSLocalizedString(@"Invert the colors of the document preview", nil);
        item.image = [NSImage imageWithSystemSymbolName:@"circle.lefthalf.filled" accessibilityDescription:@"Circle half filled"];
        item.target = self.previewer;
        item.action = @selector(invertPreviewColors:);
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

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goToPreview:) || action == @selector(goToDefinition:)) {
        // These actions are specific to the editor view, but are implemented in the
        // window controller due to needing state from the driver and previewer. For
        // consistency, we only enable them when the editor is first responder.
        NSResponder* firstResponder = self.window.firstResponder;
        if ([firstResponder isKindOfClass:[NSView class]]) {
            return [(NSView*)firstResponder isDescendantOf:self.editorView.view] || firstResponder == self.editorView.view;
        }
        return NO;
    }
    else if (action == @selector(exportAsPdf:)) {
        katvan::TypstDriverWrapper::Status status = self.driver->status();
        return status == katvan::TypstDriverWrapper::Status::SUCCESS
            || status == katvan::TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS;
    }
    return YES;
}

- (void)exportAsPdf:(id)sender
{
    NSPDFPanel* dialog = [NSPDFPanel panel];
    NSPDFInfo* info = [[NSPDFInfo alloc] init];

    NSString* filename = self.window.representedFilename;
    if ([filename length] > 0) {
        NSString* base = filename.lastPathComponent.stringByDeletingPathExtension;
        dialog.defaultFileName = base; // No .pdf suffix
    }

    [dialog beginSheetWithPDFInfo: info
            modalForWindow: self.window
            completionHandler: ^(NSInteger rc) {
                if (rc) {
                    QString path = QString::fromNSString(info.URL.path);
                    self.driver->exportToPdf(path);
                }
            }
    ];
}

- (void)goToPreview:(id)sender
{
    QTextCursor cursor = self.editorView.editor->textCursor();
    self.driver->forwardSearch(
        cursor.blockNumber(),
        cursor.positionInBlock(),
        self.previewer.previewerView->currentPage());
}

- (void)goToDefinition:(id)sender
{
    QTextCursor cursor = self.editorView.editor->textCursor();
    self.driver->searchDefinition(cursor.blockNumber(), cursor.positionInBlock());
}

- (void)goToBlock:(int)line column:(int)column
{
    self.editorView.editor->goToBlock(line, column);
}

- (void)showSymbolPicker
{
    if (!self.symbolPicker) {
        self.symbolPicker = new katvan::SymbolPicker(self.driver, self.editorView.editor);
        self.symbolPicker->setWindowFlag(Qt::Sheet);

        __weak __typeof__(self) weakSelf = self;
        QObject::connect(self.symbolPicker, &katvan::SymbolPicker::accepted,
                         self.symbolPicker, [weakSelf]() {
            QString symbol = weakSelf.symbolPicker->selectedSymbolName();
            weakSelf.editorView.editor->insertSymbol(symbol);
        });
    }

    self.symbolPicker->open();
}

@end
