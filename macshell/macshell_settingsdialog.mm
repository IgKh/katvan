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
#import "macshell_settingsdialog.h"
#import "macshell_settingsmanager.h"
#import "macshell_widgets.h"

#include "typstdriver_packagemanager.h"

static constexpr CGFloat kVerticalMargin = 20;
static constexpr CGFloat kHorizontalMargin = 30;
static constexpr CGFloat kColumnSpacing = 10;
static constexpr CGFloat kRowSpacing = 12;

static void addControlRow(NSGridView* grid, NSView* control, NSString* label)
{
    NSView* labelView = label
        ? [NSTextField labelWithString:label]
        : [[NSView alloc] init];

    [grid addRowWithViews:@[labelView, control]];
}

static void addSeparatorRow(NSGridView* grid)
{
    NSBox* line = [[NSBox alloc] init];
    line.boxType = NSBoxSeparator;

    NSGridRow* row = [grid addRowWithViews:@[line]];
    [row setTopPadding: kRowSpacing];
    [row mergeCellsInRange:NSMakeRange(0, 2)];
}

@interface KatvanEditorSettings : NSViewController

@property (nonatomic) KatvanFontPicker* editorFontPicker;
@property (nonatomic) NSPopUpButton* colorSchemePopup;
@property (nonatomic) NSPopUpButton* lineNumberPopup;
@property (nonatomic) NSButton* showControlCharsCheckbox;
@property (nonatomic) NSPopUpButton* indentModePopup;
@property (nonatomic) NSButton* indentWithSpacesRadio;
@property (nonatomic) NSButton* indentWithTabsRadio;
@property (nonatomic) KatvanSpinBox* indentWidthSpinBox;
@property (nonatomic) KatvanSpinBox* tabWidthSpinBox;
@property (nonatomic) NSButton* autoBracketsCheckbox;
@property (nonatomic) NSButton* autoCompletionsCheckbox;

- (void)loadSettings;

@end

@implementation KatvanEditorSettings

- (void)loadView
{
    self.editorFontPicker = [[KatvanFontPicker alloc] init];
    self.editorFontPicker.target = self;
    self.editorFontPicker.action = @selector(settingsChanged:);

    self.colorSchemePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.colorSchemePopup.target = self;
    self.colorSchemePopup.action = @selector(settingsChanged:);
    [self.colorSchemePopup addItemsWithTitles:@[
        NSLocalizedString(@"Light", "Editor color scheme option"),
        NSLocalizedString(@"Dark", "Editor color scheme option"),
        NSLocalizedString(@"Follow system settings", "Editor color scheme option")
    ]];

    self.lineNumberPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.lineNumberPopup.target = self;
    self.lineNumberPopup.action = @selector(settingsChanged:);
    [self.lineNumberPopup addItemsWithTitles:@[
        NSLocalizedString(@"On both sides", "Editor line number gutter option"),
        NSLocalizedString(@"On primary side only", "Editor line number gutter option"),
        NSLocalizedString(@"Don't show", "Editor line number gutter option")
    ]];

    self.showControlCharsCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"Show BiDi control characters", "Editor setting label")
                                              target:self
                                              action:@selector(settingsChanged:)];

    self.indentModePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.indentModePopup.target = self;
    self.indentModePopup.action = @selector(settingsChanged:);
    [self.indentModePopup addItemsWithTitles:@[
        NSLocalizedString(@"None", "Editor indent mode option"),
        NSLocalizedString(@"Normal", "Editor indent mode option"),
        NSLocalizedString(@"Smart", "Editor indent mode option")
    ]];

    self.indentWithSpacesRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"Spaces", "Editor indent style option")
                                           target:self
                                           action:@selector(indentStyleChanged:)];
    self.indentWithTabsRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"Tabs", "Editor indent style option")
                                         target:self
                                         action:@selector(indentStyleChanged:)];

    self.indentWidthSpinBox = [[KatvanSpinBox alloc] init];
    self.indentWidthSpinBox.target = self;
    self.indentWidthSpinBox.action = @selector(settingsChanged:);

    self.tabWidthSpinBox = [[KatvanSpinBox alloc] init];
    self.tabWidthSpinBox.target = self;
    self.tabWidthSpinBox.action = @selector(settingsChanged:);

    self.autoBracketsCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"Automatically insert closing brackets", "Editor setting label")
                                          target:self
                                          action:@selector(settingsChanged:)];

    self.autoCompletionsCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"Automatically show autocomplete suggestions", "Editor setting label")
                                             target:self
                                             action:@selector(settingsChanged:)];

    //
    // Layout
    //
    NSGridView* grid = [NSGridView gridViewWithNumberOfColumns:2 rows:0];
    grid.translatesAutoresizingMaskIntoConstraints = NO;
    grid.rowSpacing = kRowSpacing;
    grid.columnSpacing = kColumnSpacing;

    [grid setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];
    [grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
    [grid columnAtIndex:1].xPlacement = NSGridCellPlacementFill;

    addControlRow(grid, self.editorFontPicker, NSLocalizedString(@"Editor font:", "Editor setting label"));
    addControlRow(grid, self.colorSchemePopup, NSLocalizedString(@"Color scheme:", "Editor setting label"));
    addControlRow(grid, self.lineNumberPopup, NSLocalizedString(@"Show line numbers:", "Editor setting label"));
    addControlRow(grid, self.showControlCharsCheckbox, nil);
    addSeparatorRow(grid);
    addControlRow(grid, self.indentModePopup, NSLocalizedString(@"Automatic indentation:", "Editor setting label"));
    addControlRow(grid, self.indentWithSpacesRadio, NSLocalizedString(@"Indent with:", "Editor setting label"));
    addControlRow(grid, self.indentWithTabsRadio, nil);
    addControlRow(grid, self.indentWidthSpinBox, NSLocalizedString(@"Indent width:", "Editor setting label"));
    addControlRow(grid, self.tabWidthSpinBox, NSLocalizedString(@"Tab display width:", "Editor setting label"));
    addSeparatorRow(grid);
    addControlRow(grid, self.autoBracketsCheckbox, nil);
    addControlRow(grid, self.autoCompletionsCheckbox, nil);

    NSView* container = [[NSView alloc] initWithFrame:NSZeroRect];
    [container addSubview:grid];

    [NSLayoutConstraint activateConstraints:@[
        [grid.topAnchor constraintEqualToAnchor:container.topAnchor constant:kVerticalMargin],
        [grid.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:kHorizontalMargin],
        [grid.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-kHorizontalMargin],
        [grid.bottomAnchor constraintLessThanOrEqualToAnchor:container.bottomAnchor constant:-kVerticalMargin]
    ]];

    self.view = container;
    self.preferredContentSize = [container fittingSize];
}

- (void)viewDidLoad
{
    [self loadSettings];
}

- (void)loadSettings
{
    if (self.view == nil) {
        return;
    }
    const auto& editorSettings = KatvanSettingsManager::instance().editorSettings();

    // Editor font
    QFont font = editorSettings.font();
    NSFont* editorFont = [NSFont fontWithName:font.family().toNSString() size:font.pointSize()];
    self.editorFontPicker.font = editorFont;

    // Color scheme
    QString colorScheme = editorSettings.colorScheme();
    if (colorScheme == QStringLiteral("light")) {
        [self.colorSchemePopup selectItemAtIndex:0];
    }
    else if (colorScheme == QStringLiteral("dark")) {
        [self.colorSchemePopup selectItemAtIndex:1];
    }
    else {
        [self.colorSchemePopup selectItemAtIndex:2];
    }

    // Line number style
    switch (editorSettings.lineNumberStyle()) {
        case katvan::EditorSettings::LineNumberStyle::BOTH_SIDES:
            [self.lineNumberPopup selectItemAtIndex:0];
            break;
        case katvan::EditorSettings::LineNumberStyle::PRIMARY_ONLY:
            [self.lineNumberPopup selectItemAtIndex:1];
            break;
        case katvan::EditorSettings::LineNumberStyle::NONE:
            [self.lineNumberPopup selectItemAtIndex:2];
            break;
    }

    // Show control chars
    self.showControlCharsCheckbox.state = editorSettings.showControlChars() ? NSControlStateValueOn : NSControlStateValueOff;

    // Indent mode
    switch (editorSettings.indentMode()) {
        case katvan::EditorSettings::IndentMode::NONE:
            [self.indentModePopup selectItemAtIndex:0];
            break;
        case katvan::EditorSettings::IndentMode::NORMAL:
            [self.indentModePopup selectItemAtIndex:1];
            break;
        case katvan::EditorSettings::IndentMode::SMART:
            [self.indentModePopup selectItemAtIndex:2];
            break;
    }

    // Indent style
    if (editorSettings.indentStyle() == katvan::EditorSettings::IndentStyle::SPACES) {
        self.indentWithSpacesRadio.state = NSControlStateValueOn;
        self.indentWithTabsRadio.state = NSControlStateValueOff;
    }
    else {
        self.indentWithSpacesRadio.state = NSControlStateValueOff;
        self.indentWithTabsRadio.state = NSControlStateValueOn;
    }

    // Indent/tab widths
    self.indentWidthSpinBox.value = editorSettings.indentWidth();
    self.tabWidthSpinBox.value = editorSettings.tabWidth();

    // Behaviour
    self.autoBracketsCheckbox.state = editorSettings.autoBrackets() ? NSControlStateValueOn : NSControlStateValueOff;
    self.autoCompletionsCheckbox.state = editorSettings.autoTriggerCompletions() ? NSControlStateValueOn : NSControlStateValueOff;

    [self updateControlState];
}

- (void)saveSettings
{
    katvan::EditorSettings editorSettings;

    NSFont* editorFont = self.editorFontPicker.font;
    editorSettings.setFontFamily(QString::fromNSString(editorFont.familyName));
    editorSettings.setFontSize(editorFont.pointSize);

    NSInteger colorSchemeIndex = self.colorSchemePopup.indexOfSelectedItem;
    if (colorSchemeIndex == 0) {
        editorSettings.setColorScheme(QStringLiteral("light"));
    }
    else if (colorSchemeIndex == 1) {
        editorSettings.setColorScheme(QStringLiteral("dark"));
    }
    else if (colorSchemeIndex == 2) {
        editorSettings.setColorScheme(QStringLiteral("auto"));
    }

    NSInteger lineNumIndex = self.lineNumberPopup.indexOfSelectedItem;
    if (lineNumIndex == 0) {
        editorSettings.setLineNumberStyle(katvan::EditorSettings::LineNumberStyle::BOTH_SIDES);
    }
    else if (lineNumIndex == 1) {
        editorSettings.setLineNumberStyle(katvan::EditorSettings::LineNumberStyle::PRIMARY_ONLY);
    }
    else if (lineNumIndex == 2) {
        editorSettings.setLineNumberStyle(katvan::EditorSettings::LineNumberStyle::NONE);
    }

    editorSettings.setShowControlChars(self.showControlCharsCheckbox.state == NSControlStateValueOn);

    NSInteger indentModeIndex = self.indentModePopup.indexOfSelectedItem;
    if (indentModeIndex == 0) {
        editorSettings.setIndentMode(katvan::EditorSettings::IndentMode::NONE);
    }
    else if (indentModeIndex == 1) {
        editorSettings.setIndentMode(katvan::EditorSettings::IndentMode::NORMAL);
    }
    else if (indentModeIndex == 2) {
        editorSettings.setIndentMode(katvan::EditorSettings::IndentMode::SMART);
    }

    if (self.indentWithSpacesRadio.state == NSControlStateValueOn) {
        editorSettings.setIndentStyle(katvan::EditorSettings::IndentStyle::SPACES);
    }
    else {
        editorSettings.setIndentStyle(katvan::EditorSettings::IndentStyle::TABS);
    }

    editorSettings.setIndentWidth(self.indentWidthSpinBox.value);
    editorSettings.setTabWidth(self.tabWidthSpinBox.value);
    editorSettings.setAutoBrackets(self.autoBracketsCheckbox.state == NSControlStateValueOn);
    editorSettings.setAutoTriggerCompletions(self.autoCompletionsCheckbox.state == NSControlStateValueOn);

    KatvanSettingsManager::instance().updateEditorSettings(editorSettings);
}

- (void)updateControlState
{
    self.tabWidthSpinBox.enabled = (self.indentWithSpacesRadio.state == NSControlStateValueOn);
}

- (void)settingsChanged:(id)sender
{
    [self saveSettings];
}

- (void)indentStyleChanged:(id)sender
{
    [self updateControlState];
    [self settingsChanged:sender];
}

@end

@interface KatvanCompilerSettings : NSViewController <NSTableViewDelegate>

@property (nonatomic) NSButton* allowPreviewPackagesCheckbox;
@property (nonatomic) NSButton* enableA11yCheckbox;
@property (nonatomic) NSTextField* cacheSizeLabel;

@property (nonatomic) NSTableView* pathsTableView;
@property (nonatomic) NSArrayController* allowedPathsController;
@property (nonatomic) NSSegmentedControl* addRemoveControl;

@end

@implementation KatvanCompilerSettings

- (void)loadView
{
    self.allowPreviewPackagesCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"Allow download and use of @preview packages", "Compiler setting label")
                                                  target:self
                                                  action:@selector(settingsChanged:)];

    self.enableA11yCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"Enable experimental accessibility features", "Compiler setting label")
                                        target:self
                                        action:@selector(settingsChanged:)];

    self.cacheSizeLabel = [NSTextField labelWithString:@""];

    NSButton* browseCacheButton = [NSButton buttonWithTitle:NSLocalizedString(@"Browse…", "Button in compiler settings to open download cache")
                                            target:self
                                            action:@selector(browseCache)];

    NSView* allowedPathsView = [self makeAllowedPathsView];
    allowedPathsView.translatesAutoresizingMaskIntoConstraints = NO;

    //
    // Layout
    //
    NSGridView* grid = [NSGridView gridViewWithNumberOfColumns:2 rows:0];
    grid.translatesAutoresizingMaskIntoConstraints = NO;
    grid.rowSpacing = kRowSpacing;
    grid.columnSpacing = kColumnSpacing;

    [grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
    [grid columnAtIndex:1].xPlacement = NSGridCellPlacementFill;

    addControlRow(grid, self.allowPreviewPackagesCheckbox, NSLocalizedString(@"Compiler flags:", "Compiler setting label"));
    addControlRow(grid, self.enableA11yCheckbox, nil);
    addControlRow(grid, self.cacheSizeLabel, NSLocalizedString(@"Download cache:", "Compiler setting label"));
    addControlRow(grid, browseCacheButton, nil);
    addSeparatorRow(grid);

    NSView* container = [[NSView alloc] initWithFrame:NSZeroRect];
    [container addSubview:grid];
    [container addSubview:allowedPathsView];

    [NSLayoutConstraint activateConstraints:@[
        [grid.topAnchor constraintEqualToAnchor:container.topAnchor constant:kVerticalMargin],
        [grid.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:kHorizontalMargin],
        [grid.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-kHorizontalMargin],

        [allowedPathsView.topAnchor constraintEqualToAnchor:grid.bottomAnchor constant:kRowSpacing],
        [allowedPathsView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:kHorizontalMargin],
        [allowedPathsView.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-kHorizontalMargin],
        [allowedPathsView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-kVerticalMargin]
    ]];

    self.view = container;
    self.preferredContentSize = [container fittingSize];
}

- (NSView*)makeAllowedPathsView
{
    self.allowedPathsController = [[NSArrayController alloc] init];
    self.allowedPathsController.objectClass = [NSString class];

    NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"path"];
    column.editable = NO;

    [column bind:NSValueBinding
            toObject:self.allowedPathsController
            withKeyPath:@"arrangedObjects.self" // Trick to avoid wrapper model
            options:nil];

    self.pathsTableView = [[NSTableView alloc] init];
    self.pathsTableView.headerView = nil;
    self.pathsTableView.usesAlternatingRowBackgroundColors = YES;
    self.pathsTableView.allowsMultipleSelection = NO;
    self.pathsTableView.focusRingType = NSFocusRingTypeNone;
    self.pathsTableView.delegate = self;

    [self.pathsTableView addTableColumn:column];
    [self.pathsTableView bind:NSContentBinding
                         toObject:self.allowedPathsController
                         withKeyPath:@"arrangedObjects"
                         options:nil];
    [self.pathsTableView bind:NSSelectionIndexesBinding
                         toObject:self.allowedPathsController
                         withKeyPath:@"selectionIndexes"
                         options:nil];

    NSScrollView* scrollView = [[NSScrollView alloc] init];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.hasVerticalRuler = YES;
    scrollView.hasHorizontalScroller = NO;
    scrollView.autohidesScrollers = YES;
    scrollView.borderType = NSBezelBorder;
    scrollView.documentView = self.pathsTableView;

    NSString* labelText = NSLocalizedString(
        @"Allow including resources also from the following directories and their subdirectories:",
        "Introduction label for allowed paths list in compiler settings");

    NSTextField* label = [NSTextField wrappingLabelWithString:labelText];
    label.translatesAutoresizingMaskIntoConstraints = NO;

    self.addRemoveControl = [NSSegmentedControl
        segmentedControlWithImages:@[
            [NSImage imageWithSystemSymbolName:@"plus" accessibilityDescription:nil],
            [NSImage imageWithSystemSymbolName:@"minus" accessibilityDescription:nil]
        ]
        trackingMode:NSSegmentSwitchTrackingMomentary
        target:self
        action:@selector(addRemovePath:)];

    self.addRemoveControl.translatesAutoresizingMaskIntoConstraints = NO;
    self.addRemoveControl.segmentStyle = NSSegmentStyleSmallSquare;

    [self.addRemoveControl setEnabled:NO forSegment:1];

    NSView* container = [[NSView alloc] init];
    [container addSubview:label];
    [container addSubview:scrollView];
    [container addSubview:self.addRemoveControl];

    [NSLayoutConstraint activateConstraints:@[
        [label.topAnchor constraintEqualToAnchor:container.topAnchor],
        [label.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [label.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],

        [scrollView.topAnchor constraintEqualToAnchor:label.bottomAnchor constant:kRowSpacing],
        [scrollView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [scrollView.heightAnchor constraintGreaterThanOrEqualToConstant:250],

        [self.addRemoveControl.topAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
        [self.addRemoveControl.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [self.addRemoveControl.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];

    return container;
}

- (void)viewDidLoad
{
    [self loadSettings];
}

- (void)addRemovePath:(id)sender
{
    if (self.addRemoveControl.selectedSegment == 0) {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.allowsMultipleSelection = NO;

        [panel beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result) {
            if (result == NSModalResponseOK) {
                [self.allowedPathsController addObject:panel.URL.path];
                [self saveSettings];
            }
        }];
    }
    else {
        [self.allowedPathsController remove:sender];
        [self saveSettings];
    }
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification
{
    [self.addRemoveControl setEnabled:self.allowedPathsController.canRemove forSegment:1];
}

- (void)browseCache
{
    QString path = katvan::typstdriver::PackageManager::downloadCacheDirectory();
    NSURL* url = [NSURL fileURLWithPath:path.toNSString()];

    [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)updateCacheSizeLabel
{
    auto stats = katvan::typstdriver::PackageManager::cacheStatistics();

    NSString* size = [NSByteCountFormatter stringFromByteCount:stats.totalSize
                                               countStyle:NSByteCountFormatterCountStyleFile];

    NSNumberFormatter* numberFormatter = [[NSNumberFormatter alloc] init];
    numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;

    NSString* numPackages = [numberFormatter stringFromNumber:@(stats.numPackages)];
    NSString* numPackageVersions = [numberFormatter stringFromNumber:@(stats.numPackageVersions)];

    NSString* label = [NSString
        stringWithFormat:NSLocalizedString(@"%2$@ distinct versions of %1$@ packages (total %3$@)", "Typst download cache statistics"),
        numPackages,
        numPackageVersions,
        size];

    self.cacheSizeLabel.stringValue = label;
}

- (void)loadSettings
{
    if (self.view == nil) {
        return;
    }
    const auto& compilerSettings = KatvanSettingsManager::instance().compilerSettings();

    // Compiler flags
    self.allowPreviewPackagesCheckbox.state = compilerSettings.allowPreviewPackages() ? NSControlStateValueOn : NSControlStateValueOff;
    self.enableA11yCheckbox.state = compilerSettings.enableA11yExtras() ? NSControlStateValueOn : NSControlStateValueOff;

    // Download cache size
    [self updateCacheSizeLabel];

    // Allowed paths
    const auto& allowedPaths = compilerSettings.allowedPaths();

    NSMutableArray* paths = [NSMutableArray arrayWithCapacity:allowedPaths.size()];
    for (const QString& path : allowedPaths) {
        [paths addObject:path.toNSString()];
    }
    [self.allowedPathsController setContent:paths];
}

- (void)saveSettings
{
    katvan::typstdriver::TypstCompilerSettings compilerSettings;

    compilerSettings.setAllowPreviewPackages(self.allowPreviewPackagesCheckbox.state == NSControlStateValueOn);
    compilerSettings.setEnableA11yExtras(self.enableA11yCheckbox.state == NSControlStateValueOn);

    QStringList allowedPaths;
    NSArray<NSString*>* paths = self.allowedPathsController.arrangedObjects;
    for (NSString* path in paths) {
        allowedPaths.append(QString::fromNSString(path));
    }
    compilerSettings.setAllowedPaths(allowedPaths);

    KatvanSettingsManager::instance().updateCompilerSettings(compilerSettings);
}

- (void)settingsChanged:(id)sender
{
    [self saveSettings];
}

@end

@interface KatvanSettingsTabController : NSTabViewController
@end

@implementation KatvanSettingsTabController

- (void)viewWillAppear
{
    [super viewWillAppear];
    [self resizeWindowForTab:self.tabView.selectedTabViewItem animate:NO];
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    [super tabView:tabView didSelectTabViewItem:tabViewItem];
    [self resizeWindowForTab:tabViewItem animate:self.view.window.isVisible];
}

- (void)resizeWindowForTab:(NSTabViewItem*)tabViewItem animate:(BOOL)animate
{
    NSWindow* window = self.view.window;
    if (!window) {
        return;
    }

    NSSize contentSize = tabViewItem.viewController.preferredContentSize;
    if (NSEqualSizes(contentSize, NSZeroSize)) {
        return;
    }

    NSRect windowFrame = window.frame;
    CGFloat chromeHeight = windowFrame.size.height - window.contentLayoutRect.size.height;

    CGFloat newHeight = contentSize.height + chromeHeight;
    CGFloat newWidth = contentSize.width;
    CGFloat deltaHeight = newHeight - windowFrame.size.height;
    CGFloat deltaWidth = newWidth - windowFrame.size.width;

    NSRect newFrame = NSMakeRect(
        windowFrame.origin.x - deltaWidth / 2.0,
        windowFrame.origin.y - deltaHeight,
        newWidth,
        newHeight
    );

    [window setFrame:newFrame display:YES animate:animate];
}

@end

@interface KatvanSettingsDialog ()

@property (nonatomic) KatvanEditorSettings* editorSettingsController;
@property (nonatomic) KatvanCompilerSettings* compilerSettingsController;

@end

@implementation KatvanSettingsDialog

- (instancetype)init
{
    KatvanEditorSettings* editorSettingsController = [[KatvanEditorSettings alloc] init];
    editorSettingsController.title = NSLocalizedString(@"Editor", "Settings dialog tab title");

    KatvanCompilerSettings* compilerSettingsController = [[KatvanCompilerSettings alloc] init];
    compilerSettingsController.title = NSLocalizedString(@"Compiler", "Settings dialog tab title");

    NSTabViewItem* editorTab = [NSTabViewItem tabViewItemWithViewController:editorSettingsController];
    editorTab.image = [NSImage imageWithSystemSymbolName:@"pencil" accessibilityDescription: nil];

    NSTabViewItem* compilerTab = [NSTabViewItem tabViewItemWithViewController:compilerSettingsController];
    compilerTab.image = [NSImage imageWithSystemSymbolName:@"gear" accessibilityDescription: nil];

    KatvanSettingsTabController* tabController = [[KatvanSettingsTabController alloc] init];
    tabController.tabStyle = NSTabViewControllerTabStyleToolbar;
    tabController.tabViewItems = @[editorTab, compilerTab];

    NSWindow* window = [NSWindow windowWithContentViewController:tabController];
    window.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self = [super initWithWindow:window];
    if (self) {
        self.editorSettingsController = editorSettingsController;
        self.compilerSettingsController = compilerSettingsController;
    }
    return self;
}

- (void)showDialog
{
    KatvanSettingsManager::instance().reloadSettings();
    [self.editorSettingsController loadSettings];
    [self.compilerSettingsController loadSettings];

    [self.window makeKeyAndOrderFront:nil];
    [self.window center];
}

@end
