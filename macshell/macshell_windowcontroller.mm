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
#import "macshell_exporter.h"
#import "macshell_issuelist.h"
#import "macshell_labelsview.h"
#import "macshell_outlineview.h"
#import "macshell_previewer.h"
#import "macshell_settingsmanager.h"
#import "macshell_sidebar.h"
#import "macshell_typstdocument.h"
#import "macshell_windowcontroller.h"

#include "katvan_completionmanager.h"
#include "katvan_diagnosticsmodel.h"
#include "katvan_editor.h"
#include "katvan_symbolpicker.h"
#include "katvan_typstdriverwrapper.h"
#include "katvan_wordcounter.h"

@interface KatvanWindowController ()

@property (nonatomic) NSSplitViewController* splitViewController;
@property (nonatomic) KatvanEditorView* editorView;
@property (nonatomic) KatvanPreviewer* previewer;
@property (nonatomic) KatvanSidebar* sidebar;
@property (nonatomic) KatvanOutlineView* outlineView;
@property (nonatomic) KatvanLabelsView* labelsView;
@property (nonatomic) KatvanIssueList* issueList;
@property (nonatomic) KatvanExporter* exporter;
@property (nonatomic) NSToolbarItem* compilationStatusItem;
@property (nonatomic) NSToolbarItemGroup* previewLayoutItem;

@property (nonatomic) NSSplitViewItem* sidebarSplitItem;
@property (nonatomic) NSSplitViewItem* editorSplitItem;
@property (nonatomic) NSSplitViewItem* previewerSplitItem;

@property (nonatomic) katvan::Document* textDocument;
@property (nonatomic) katvan::TypstDriverWrapper* driver;
@property (nonatomic) katvan::WordCounter* wordCounter;
@property (nonatomic) katvan::SymbolPicker* symbolPicker;

@property (nonatomic) QString documentFilePath;

@end

@implementation KatvanWindowController

- (instancetype)initWithDocument:(katvan::Document*)textDocument initialURL:(NSURL*)url
{
    NSRect frame = NSMakeRect(0, 0, 1900, 1000);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:styleMask
                                         backing:NSBackingStoreBuffered
                                         defer:NO];

    self = [super initWithWindow:window];
    if (self) {
        window.tabbingMode = NSWindowTabbingModeDisallowed;

        self.textDocument = textDocument;
        self.driver = new katvan::TypstDriverWrapper();
        self.wordCounter = new katvan::WordCounter(self.driver);
        self.symbolPicker = nullptr;
        self.exporter = [[KatvanExporter alloc] initWithDriver:self.driver andWindow:window];

        [self setupViewsWithDocument:textDocument];
        [self setupUI];
        [self readSettings];
        [self resizeInitialUiWithFrameRect:frame];

        [self documentDidExplicitlySaveInURL:url forced:YES];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    delete self.wordCounter;
    delete self.driver;
}

- (void)setupViewsWithDocument:(katvan::Document*)textDocument
{
    self.editorView = [[KatvanEditorView alloc] initWithDocument:textDocument];
    self.previewer = [[KatvanPreviewer alloc] initWithDriver:self.driver];

    self.outlineView = [[KatvanOutlineView alloc] init];
    self.outlineView.target = self;

    self.labelsView = [[KatvanLabelsView alloc] init];
    self.labelsView.target = self;

    self.issueList = [[KatvanIssueList alloc] initWithModel:self.driver->diagnosticsModel()];
    self.issueList.target = self;

    __weak __typeof__(self) weakSelf = self;

    QObject::connect(self.textDocument, &katvan::Document::contentEdited,
                     self.driver, &katvan::TypstDriverWrapper::applyContentEdit);
    QObject::connect(self.textDocument, &katvan::Document::contentModified,
                     self.driver, &katvan::TypstDriverWrapper::updatePreview);

    QObject::connect(self.driver, &katvan::TypstDriverWrapper::jumpToPreview,
                     self.previewer.previewerView, &katvan::PreviewerView::jumpTo);
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::showEditorToolTip,
                     self.editorView.editor, &katvan::Editor::showToolTip);
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::completionsReady,
                     self.editorView.editor->completionManager(), &katvan::CompletionManager::completionsReady);

    QObject::connect(self.driver, &katvan::TypstDriverWrapper::jumpToEditor,
                     self.driver, [weakSelf](int line, int column) {
        [weakSelf goToBlock:line column:column];
    });
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::compilationStatusChanged,
                     self.driver, [weakSelf]() {
        [weakSelf compilationStatusChanged];
    });
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::outlineUpdated,
                     self.driver, [weakSelf](katvan::typstdriver::OutlineNode* outline) {
        [weakSelf.outlineView setOutline:outline];
    });
    QObject::connect(self.driver, &katvan::TypstDriverWrapper::labelsUpdated,
                     self.driver, [weakSelf](QList<katvan::typstdriver::DocumentLabel> labels) {
        [weakSelf.labelsView setLabels:labels];
    });

    QObject::connect(self.editorView.editor, &katvan::Editor::toolTipRequested,
                     self.driver, &katvan::TypstDriverWrapper::requestToolTip);
    QObject::connect(self.editorView.editor, &katvan::Editor::goToDefinitionRequested,
                     self.driver, &katvan::TypstDriverWrapper::searchDefinition);
    QObject::connect(self.editorView.editor->completionManager(), &katvan::CompletionManager::completionsRequested,
                     self.driver, &katvan::TypstDriverWrapper::requestCompletions);

    QObject::connect(self.editorView.editor, &QTextEdit::cursorPositionChanged,
                     self.editorView.editor, [weakSelf]() {
        [weakSelf textCursorPositionChanged];
    });
    QObject::connect(self.editorView.editor, &katvan::Editor::showSymbolPicker,
                     self.editorView.editor, [weakSelf]() {
        [weakSelf showSymbolPicker];
    });
    QObject::connect(self.editorView.editor, &katvan::Editor::showColorPicker,
                     self.editorView.editor, [weakSelf]() {
        [weakSelf.editorView showColorPicker];
    });

    QObject::connect(self.wordCounter, &katvan::WordCounter::wordCountChanged,
                     self.wordCounter, [weakSelf](size_t count) {
        [weakSelf.editorView updateWordCount:count];
    });
}

- (void)resizeInitialUiWithFrameRect:(NSRect)frame
{
    // Need to resize the window, as splitViewController overrides its initial size
    [self.window setContentSize:frame.size];
    [self.window center];

    // Set initial split view proportions and autosave name
    [self.window layoutIfNeeded];

    NSSplitView* splitView = self.splitViewController.splitView;
    CGFloat totalWidth = NSWidth(splitView.frame);
    [splitView setPosition:round(totalWidth * 0.2) ofDividerAtIndex:0];
    [splitView setPosition:round(totalWidth * 0.6) ofDividerAtIndex:1];
}

- (void)setupUI
{
    [self setupSidebar];

    self.splitViewController = [[NSSplitViewController alloc] init];
    self.splitViewController.splitView.dividerStyle = NSSplitViewDividerStyleThin;

    self.sidebarSplitItem = [NSSplitViewItem sidebarWithViewController:self.sidebar];
    [self.sidebarSplitItem setPreferredThicknessFraction:0.2];
    [self.sidebarSplitItem setTitlebarSeparatorStyle:NSTitlebarSeparatorStyleLine];
    [self.splitViewController addSplitViewItem:self.sidebarSplitItem];

    self.editorSplitItem = [NSSplitViewItem splitViewItemWithViewController:self.editorView];
    [self.editorSplitItem setPreferredThicknessFraction:0.4];
    [self.splitViewController addSplitViewItem:self.editorSplitItem];

    self.previewerSplitItem = [NSSplitViewItem splitViewItemWithViewController:self.previewer];
    [self.previewerSplitItem setPreferredThicknessFraction:0.4];
    [self.previewerSplitItem setCanCollapse:YES];
    [self.splitViewController addSplitViewItem:self.previewerSplitItem];

    [self.window setContentViewController:self.splitViewController];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                          selector:@selector(splitViewDidMove:)
                                          name:NSSplitViewDidResizeSubviewsNotification
                                          object:self.splitViewController.splitView];

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
                  toolTip:NSLocalizedString(@"Document Outline", "Tooltip for sidebar tab")];

    [self.sidebar addTabController:self.labelsView
                  icon:[NSImage imageWithSystemSymbolName:@"tag"
                                accessibilityDescription:@"Tag"]
                  toolTip:NSLocalizedString(@"Labels", "Tooltip for sidebar tab")];

    [self.sidebar addTabController:self.issueList
                  icon:[NSImage imageWithSystemSymbolName:@"exclamationmark.triangle"
                                accessibilityDescription:@"Exclamation mark in a triangle"]
                  toolTip:NSLocalizedString(@"Issues", "Tooltip for sidebar tab")];

    [self.sidebar ensureControllerSelected:self.outlineView];
}

- (NSArray<NSString*>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
    return @[
        NSToolbarToggleSidebarItemIdentifier,
        NSToolbarSidebarTrackingSeparatorItemIdentifier,
        @"katvan.toolbar.status",
        @"katvan.toolbar.layout",
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
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.status"]) {
        NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        item.label = NSLocalizedString(@"Compilation Status", "Details about the result of the last compilation");

        NSButton* button = [NSButton buttonWithImage:[NSImage new]
                                     target:self
                                     action:@selector(compilationStatusClicked:)];
        button.bezelStyle = NSBezelStyleToolbar;
        button.bordered = NO;
        button.contentTintColor = NSColor.systemGrayColor;
        item.view = button;

        if (flag) {
            self.compilationStatusItem = item;
        }
        return item;
    }
    if ([itemIdentifier isEqualToString:@"katvan.toolbar.layout"]) {
        NSArray* itemIcons;
        if (NSApp.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionLeftToRight) {
            itemIcons = @[
                [NSImage imageWithSystemSymbolName:@"rectangle.lefthalf.filled" accessibilityDescription:@"On the left"],
                [NSImage imageWithSystemSymbolName:@"rectangle" accessibilityDescription:@"No preview"],
                [NSImage imageWithSystemSymbolName:@"rectangle.righthalf.filled" accessibilityDescription:@"On the right"],
            ];
        }
        else {
            itemIcons = @[
                [NSImage imageWithSystemSymbolName:@"rectangle.righthalf.filled" accessibilityDescription:@"On the right"],
                [NSImage imageWithSystemSymbolName:@"rectangle" accessibilityDescription:@"No preview"],
                [NSImage imageWithSystemSymbolName:@"rectangle.lefthalf.filled" accessibilityDescription:@"On the left"],
            ];
        }

        NSToolbarItemGroup* item =
            [NSToolbarItemGroup groupWithItemIdentifier:itemIdentifier
                                images:itemIcons
                                selectionMode:NSToolbarItemGroupSelectionModeSelectOne
                                labels:@[
                                    NSLocalizedString(@"On the left", "Preview position option"),
                                    NSLocalizedString(@"No preview", "Preview position option"),
                                    NSLocalizedString(@"On the right", "Preview position option"),
                                ]
                                target:self
                                action:@selector(applyPreviewLocation:)];

        item.label = NSLocalizedString(@"Preview Location", "Segment control for choosing where the preview will be");
        item.selectedIndex = 2;

        if (flag) {
            self.previewLayoutItem = item;
        }
        return item;
    }
    return nil;
}

- (void)readSettings
{
    KatvanSettingsManager& manager = KatvanSettingsManager::instance();

    self.editorView.editor->applySettings(manager.editorSettings());
    self.driver->setCompilerSettings(manager.compilerSettings());
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

- (void)restoreStateWithCoder:(NSCoder*)coder
{
    // Decode preview location
    if ([coder containsValueForKey:@"previewLocation"]) {
        NSInteger previewLocation = [coder decodeIntegerForKey:@"previewLocation"];

        [self.previewLayoutItem setSelectedIndex:previewLocation];
        [self applyPreviewLocation:nil];
    }

    // Decode splitter positions
    NSArray<NSNumber*>* positions = [coder decodeArrayOfObjectsOfClass:[NSNumber class] forKey:@"splitterPositions"];
    if (positions) {
        NSSplitView* splitView = self.splitViewController.splitView;
        for (NSUInteger i = 0; i < positions.count && i + 1 < splitView.subviews.count; i++) {
            [splitView setPosition:positions[i].doubleValue ofDividerAtIndex:i];
        }
    }

    [super restoreStateWithCoder:coder];
}

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder
{
    // Encode preview layout
    NSInteger previewLocation = self.previewLayoutItem.selectedIndex;
    [coder encodeInteger:previewLocation forKey:@"previewLocation"];

    // Encode splitter positions
    NSSplitView* splitView = self.splitViewController.splitView;
    NSMutableArray<NSNumber*>* positions = [NSMutableArray array];
    NSArray<NSView*>* subviews = splitView.subviews;
    for (NSUInteger i = 0; i + 1 < subviews.count; i++) {
        [positions addObject:@(NSMaxX(subviews[i].frame))];
    }
    [coder encodeObject:positions forKey:@"splitterPositions"];

    [super encodeRestorableStateWithCoder:coder];
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
        return [self.exporter canExport];
    }
    return YES;
}

- (void)splitViewDidMove:(NSNotification*)notification
{
    if (notification.userInfo[@"NSSplitViewUserResizeKey"]) {
        [self invalidateRestorableState];
    }
}

- (void)applyPreviewLocation:(id)sender
{
    NSInteger index = self.previewLayoutItem.selectedIndex;
    if (index == 0) { // Preview on the left (in LTR mode)
        self.splitViewController.splitViewItems = @[self.sidebarSplitItem, self.previewerSplitItem, self.editorSplitItem];
    }
    else if (index == 1) { // No preview
        self.splitViewController.splitViewItems = @[self.sidebarSplitItem, self.editorSplitItem];
    }
    else if (index == 2) { // Preview on the right (in LTR mode)
        self.splitViewController.splitViewItems = @[self.sidebarSplitItem, self.editorSplitItem, self.previewerSplitItem];
    }

    [self.splitViewController.splitView adjustSubviews];
    [self invalidateRestorableState];
}

- (void)exportAsPdf:(id)sender
{
    [self.exporter exportAsPdf];
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
    [self.editorView ensureFocused];
}

- (void)invertPreviewColors:(id)sender
{
    // Answer the action here in case the previewer isn't first responder
    [self.previewer performSelector:@selector(invertPreviewColors:) withObject:sender];
}

- (void)compilationStatusClicked:(id)sender
{
    // Make sure sidebar is visible
    NSSplitViewItem* sidebarItem = [self.splitViewController splitViewItemForViewController:self.sidebar];
    sidebarItem.collapsed = NO;

    [self.sidebar ensureControllerSelected:self.issueList];
}

- (void)compilationStatusChanged
{
    self.editorView.editor->setSourceDiagnostics(self.driver->diagnosticsModel()->sourceDiagnostics());

    if (self.compilationStatusItem) {
        NSImage* statusSymbol;
        NSColor* symbolColor;
        NSString* toolTip;

        switch (self.driver->status()) {
            case katvan::TypstDriverWrapper::Status::SUCCESS:
                symbolColor = NSColor.systemGreenColor;
                toolTip = NSLocalizedString(@"Compiled successfully", "Compilation status");
                statusSymbol = [NSImage imageWithSystemSymbolName:@"checkmark.circle"
                                        accessibilityDescription:@"Checkmark in a circle"];
                break;
            case katvan::TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS:
                toolTip = NSLocalizedString(@"Compiled with warnings", "Compilation status");
                symbolColor = NSColor.systemYellowColor;
                statusSymbol = [NSImage imageWithSystemSymbolName:@"exclamationmark.circle"
                                        accessibilityDescription:@"Exclamation mark in a circle"];
                break;
            case katvan::TypstDriverWrapper::Status::FAILED:
                toolTip = NSLocalizedString(@"Compiled with errors", "Compilation status");
                symbolColor = NSColor.systemRedColor;
                statusSymbol = [NSImage imageWithSystemSymbolName:@"xmark.circle"
                                        accessibilityDescription:@"X mark in a circle"];
                break;
            default:
                toolTip = NSLocalizedString(@"Compiling...", "Compilation status");
                symbolColor = NSColor.systemGrayColor;
                statusSymbol = [NSImage imageWithSystemSymbolName:@"clock"
                                        accessibilityDescription:@"Clock"];
                break;
        }

        NSButton* button = (NSButton*)self.compilationStatusItem.view;
        button.image = statusSymbol;
        button.contentTintColor = symbolColor;
        button.toolTip = toolTip;
    }
}

- (void)textCursorPositionChanged
{
    QTextCursor cursor = self.editorView.editor->textCursor();
    int line = cursor.blockNumber();
    int column = cursor.positionInBlock();

    if (self.previewer.followEditorCursor) {
        self.driver->forwardSearch(line, column, self.previewer.currentPage);
    }
    [self.outlineView selectEntryForLine:line];
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
