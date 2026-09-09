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
#import "macshell_previewer.h"
#import "macshell_widgets.h"

#include <QApplication>
#include <QScrollBar>

static const NSInteger kZoomLevelFitPage = -1;
static const NSInteger kZoomLevelFitWidth = -2;
static const NSSize kPageNumberLabelPadding = NSMakeSize(8, 8);

@interface PageNumberLabelCell : NSTextFieldCell
@property (nonatomic) BOOL active;
@end

@implementation PageNumberLabelCell

- (void)setActive:(BOOL)active
{
    _active = active;
    self.textColor = _active
        ? [NSColor alternateSelectedControlTextColor] // Contrasts with accent color
        : [NSColor unemphasizedSelectedTextColor];
}

- (NSSize)cellSizeForBounds:(NSRect)rect
{
    NSSize size = [super cellSizeForBounds:rect];
    size.height += 2 * kPageNumberLabelPadding.height;
    size.width += 2 * kPageNumberLabelPadding.width;
    return size;
}

- (NSRect)drawingRectForBounds:(NSRect)rect
{
    NSRect drawingRect = [super drawingRectForBounds:rect];
    return NSInsetRect(drawingRect, kPageNumberLabelPadding.width, kPageNumberLabelPadding.height);
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    CGFloat radius = 10.0;
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:cellFrame xRadius:radius yRadius:radius];
    NSColor* fillColor = self.active
        ? [NSColor controlAccentColor]
        : [NSColor unemphasizedSelectedContentBackgroundColor];

    [fillColor setFill];
    [path fill];
    [path setClip];

    [super drawWithFrame:cellFrame inView:controlView];
}

- (void)resetCursorRect:(NSRect)cellFrame inView:(NSView*)controlView
{
    // Ensure we don't have the open hand cursor when hovering over the pill
    [controlView addCursorRect:cellFrame cursor:[NSCursor arrowCursor]];
}

@end

@interface KatvanPreviewer ()

@property (nonatomic) katvan::PreviewerView* previewerView;
@property (nonatomic, weak) NSView* previewerNsView;
@property (nonatomic) NSPopUpButton* zoomLevelsPopUp;
@property (nonatomic) NSTextField* currentPageLabel;
@property (nonatomic) NSButton* followCursorButton;

@property (nonatomic) BOOL followEditorCursor;
@property (nonatomic) QPointF pendingScrollPosition;

@end

@implementation KatvanPreviewer

- (instancetype)initWithDriver:(katvan::TypstDriverWrapper *)driver
{
    self = [super init];
    if (self) {
        _followEditorCursor = NO;

        self.identifier = [self className];
        self.previewerView = new katvan::PreviewerView(driver);

        __weak __typeof__(self) weakSelf = self;

        QObject::connect(driver, &katvan::TypstDriverWrapper::previewReady,
                         self.previewerView, [weakSelf](QList<katvan::typstdriver::PreviewPageData> pages) {
            [weakSelf setPreviewPages:pages];
        });

        QObject::connect(self.previewerView, &katvan::PreviewerView::currentPageChanged,
                         self.previewerView, [weakSelf]() {
            [weakSelf updatePageLabel];
        });

        QObject::connect(self.previewerView, &katvan::PreviewerView::zoomedByScrolling,
                         self.previewerView, [weakSelf](int units) {
            [weakSelf zoomByScrolling:units];
        });

        QObject::connect(self.previewerView->verticalScrollBar(), &QScrollBar::valueChanged,
                         self.previewerView, [weakSelf]() {
            [weakSelf invalidateRestorableState];
        });

        QObject::connect(self.previewerView->horizontalScrollBar(), &QScrollBar::valueChanged,
                         self.previewerView, [weakSelf]() {
            [weakSelf invalidateRestorableState];
        });
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    delete self.previewerView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.previewerNsView = (__bridge NSView *)reinterpret_cast<void*>(self.previewerView->winId());
    self.previewerNsView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.previewerNsView];

    KatvanAuxToolBar* toolbar = [self makePreviewToolbar];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:toolbar];

    self.currentPageLabel = [NSTextField labelWithString:@""];
    self.currentPageLabel.cell = [[PageNumberLabelCell alloc] init];
    self.currentPageLabel.hidden = YES;
    self.currentPageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.currentPageLabel];

    [NSLayoutConstraint activateConstraints:@[
        [toolbar.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [toolbar.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [toolbar.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.previewerNsView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.previewerNsView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.previewerNsView.topAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
        [self.previewerNsView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.currentPageLabel.centerXAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
        [self.currentPageLabel.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-20],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumHeight()],
    ]];

    self.previewerView->show();

    [[NSNotificationCenter defaultCenter] addObserver:self
                                          selector:@selector(windowDidUpdate:)
                                          name:NSWindowDidUpdateNotification
                                          object:self.view.window];
}

- (void)setupZoomLevelPopup
{
    NSMenu* levelsMenu = [[NSMenu alloc] init];
    NSMenuItem* item;

    // A pull-down button always displays its menu's first item on the button
    // itself, so create a placeholder
    [levelsMenu addItemWithTitle:@"[Placeholder]" action:nil keyEquivalent:@""];

    item = [levelsMenu addItemWithTitle:NSLocalizedString(@"Fit Page", "Preview zoom level option")
                       action:@selector(setZoomLevel:)
                       keyEquivalent:@""];
    [item setTarget:self];
    [item setTag:kZoomLevelFitPage];

    item = [levelsMenu addItemWithTitle:NSLocalizedString(@"Fit Width", "Preview zoom level option")
                       action:@selector(setZoomLevel:)
                       keyEquivalent:@""];
    [item setTarget:self];
    [item setTag:kZoomLevelFitWidth];

    [levelsMenu addItem:[NSMenuItem separatorItem]];

    NSArray* levels = @[@(50), @(75), @(100), @(125), @(150), @(200)];
    for (NSNumber* level in levels) {
        NSString* title = [NSString stringWithFormat:@"%d%%", [level intValue]];

        item = [levelsMenu addItemWithTitle:title action:@selector(setZoomLevel:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:[level intValue]];
    }

    self.zoomLevelsPopUp = [[NSPopUpButton alloc] init];
    self.zoomLevelsPopUp.pullsDown = YES;
    self.zoomLevelsPopUp.menu = levelsMenu;
    self.zoomLevelsPopUp.bordered = NO;
    self.zoomLevelsPopUp.toolTip = NSLocalizedString(@"Zoom level", "Tooltip for preview zoom pull-down");

    [self.zoomLevelsPopUp setTitle:@"100%"];
}

- (KatvanAuxToolBar*)makePreviewToolbar
{
    KatvanAuxToolBar* toolbar = [[KatvanAuxToolBar alloc] init];

    NSImage* zoomOutIcon = [NSImage imageWithSystemSymbolName:@"minus.magnifyingglass"
                                    accessibilityDescription:@"Magnifying glass with minus"];
    NSImage* zoomInIcon = [NSImage imageWithSystemSymbolName:@"plus.magnifyingglass"
                                   accessibilityDescription:@"Magnifying glass with plus"];
    NSImage* invertColorsIcon = [NSImage imageWithSystemSymbolName:@"circle.lefthalf.filled"
                                         accessibilityDescription:@"Circle half filled"];
    NSImage* followCursorIcon = [NSImage imageWithSystemSymbolName:@"character.cursor.ibeam"
                                         accessibilityDescription:@"Text cursor"];

    [toolbar addButtonWithIcon:zoomOutIcon
             toolTip:NSLocalizedString(@"Zoom out preview", "Tooltip for command button")
             inGravity:NSStackViewGravityLeading
             target:self
             action:@selector(zoomOut:)];

    [self setupZoomLevelPopup];
    [toolbar addView:self.zoomLevelsPopUp inGravity:NSStackViewGravityLeading];

    [toolbar addButtonWithIcon:zoomInIcon
             toolTip:NSLocalizedString(@"Zoom in preview", "Tooltip for command button")
             inGravity:NSStackViewGravityLeading
             target:self
             action:@selector(zoomIn:)];

    [toolbar addButtonWithIcon:invertColorsIcon
             toolTip:NSLocalizedString(@"Invert preview colors", "Tooltip for command button")
             inGravity:NSStackViewGravityTrailing
             target:self
             action:@selector(invertPreviewColors:)];

    self.followCursorButton = [toolbar addButtonWithIcon:followCursorIcon
             toolTip:NSLocalizedString(@"Follow editor cursor", "Tooltip for command button")
             inGravity:NSStackViewGravityTrailing
             target:self
             action:@selector(toggleFollowEditorCursor:)];

    self.followCursorButton.buttonType = NSButtonTypePushOnPushOff;

    return toolbar;
}

- (void)restoreStateWithCoder:(NSCoder*)coder
{
    // Decode zoom level
    if ([coder containsValueForKey:@"zoomLevel"]) {
        NSInteger level = [coder decodeIntegerForKey:@"zoomLevel"];
        [self setZoomLevelByLevel:level];
    }

    // Decode color invert flag
    if ([coder containsValueForKey:@"invertColors"]) {
        BOOL invert = [coder decodeBoolForKey:@"invertColors"];
        self.previewerView->setInvertColors(invert);
    }

    // Decode follow cursor flag
    if ([coder containsValueForKey:@"followCursor"]) {
        BOOL followCursor = [coder decodeBoolForKey:@"followCursor"];
        [self setFollowEditorCursor:followCursor];
    }

    // Decode scroll position
    if ([coder containsValueForKey:@"scrollPos"]) {
        NSPoint scrollPos = [coder decodePointForKey:@"scrollPos"];
        self.pendingScrollPosition = QPointF::fromCGPoint(scrollPos);
    }

    [super restoreStateWithCoder:coder];
}

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder
{
    // Encode zoom level
    NSInteger zoomLevel;
    switch (self.previewerView->zoomMode()) {
        case katvan::PreviewerView::ZoomMode::Custom:
            zoomLevel = qRound(self.previewerView->zoomFactor() * 100);
            break;
        case katvan::PreviewerView::ZoomMode::FitToPage:
            zoomLevel = kZoomLevelFitPage;
            break;
        case katvan::PreviewerView::ZoomMode::FitToWidth:
            zoomLevel = kZoomLevelFitWidth;
            break;
    }

    [coder encodeInteger:zoomLevel forKey:@"zoomLevel"];

    // Encode color invert flag
    [coder encodeBool:self.previewerView->areColorsInverted() forKey:@"invertColors"];

    // Encode follow cursor flag
    [coder encodeBool:_followEditorCursor forKey:@"followCursor"];

    // Encode scroll position
    int scrollX = self.previewerView->horizontalScrollBar()->value();
    int scrollY = self.previewerView->verticalScrollBar()->value();
    [coder encodePoint:NSMakePoint(scrollX, scrollY) forKey:@"scrollPos"];

    [super encodeRestorableStateWithCoder:coder];
}

- (void)windowDidUpdate:(NSNotification*)notification
{
    BOOL active = self.view.window.firstResponder == self.previewerNsView;
    PageNumberLabelCell* cell = (PageNumberLabelCell*)self.currentPageLabel.cell;
    if (cell.active != active) {
        cell.active = active;
        [self.currentPageLabel setNeedsDisplay:YES];
    }
}

- (void)resetAppearance
{
    // Embedded Qt widgets don't get the ApplicationPaletteChange event
    // because they are not top-level but still backed by a QWindow... To
    // workaround we need to send it ourselves.
    QEvent event(QEvent::ApplicationPaletteChange);
    QApplication::sendEvent(self.previewerView, &event);
}

- (int)currentPage
{
    return self.previewerView->currentPage();
}

- (void)setFollowEditorCursor:(BOOL)follow
{
    _followEditorCursor = follow;
    self.followCursorButton.state = follow ? NSControlStateValueOn : NSControlStateValueOff;
    self.followCursorButton.contentTintColor = follow ? [NSColor controlAccentColor] : nil;
    [self invalidateRestorableState];
}

- (void)setPreviewPages:(QList<katvan::typstdriver::PreviewPageData>)pages
{
    self.previewerView->setPages(pages);

    QPointF scrollPos = self.pendingScrollPosition;
    if (!scrollPos.isNull()) {
        self.previewerView->horizontalScrollBar()->setValue(scrollPos.x());
        self.previewerView->verticalScrollBar()->setValue(scrollPos.y());
        self.pendingScrollPosition = QPointF();
    }

    self.currentPageLabel.hidden = NO;
    [self updatePageLabel];
}

- (qreal)currentRoundZoomFactor
{
    qreal factor = self.previewerView->effectiveZoom(self.previewerView->currentPage());
    return qRound(factor * 20) / 20.0; // Round to nearest multiple of 5%
}

- (void)setCustomZoom:(qreal)factor
{
    factor = qBound(0.05, factor, 10.0);

    self.previewerView->setCustomZoomFactor(factor);
    if (self.zoomLevelsPopUp != nil) {
        NSString* title = [NSString stringWithFormat:@"%d%%", qRound(factor * 100)];
        [self.zoomLevelsPopUp setTitle:title];
    }
}

- (void)setZoomLevelByLevel:(NSInteger)level
{
    if (level >= 0) {
        qreal factor = level / 100.0;
        [self setCustomZoom:factor];
    }
    else {
        if (level == kZoomLevelFitPage) {
            self.previewerView->setZoomMode(katvan::PreviewerView::ZoomMode::FitToPage);
            [self.zoomLevelsPopUp setTitle:NSLocalizedString(@"Fit Page", nil)];
        }
        else if (level == kZoomLevelFitWidth) {
            self.previewerView->setZoomMode(katvan::PreviewerView::ZoomMode::FitToWidth);
            [self.zoomLevelsPopUp setTitle:NSLocalizedString(@"Fit Width", nil)];
        }
    }
}

- (void)updatePageLabel
{
    int pageCount = self.previewerView->pageCount();
    int currentPage = self.previewerView->currentPage();
    NSString* page = self.previewerView->pageLabel(currentPage).toNSString();

    NSString* format = NSLocalizedString(@"Page %@ of %d", "Preview current page label");
    NSString* label = [NSString stringWithFormat:format, page, pageCount];
    self.currentPageLabel.stringValue = label;
}

- (void)zoomByScrolling:(int)units
{
    qreal factor = [self currentRoundZoomFactor] + units * 0.05;
    [self setCustomZoom:factor];
    [self invalidateRestorableState];
}

- (void)zoomToActualSize:(id)sender
{
    [self setCustomZoom:1.0];
    [self invalidateRestorableState];
}

- (void)zoomIn:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] + 0.05;
    [self setCustomZoom:factor];
    [self invalidateRestorableState];
}

- (void)zoomOut:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] - 0.05;
    [self setCustomZoom:factor];
    [self invalidateRestorableState];
}

- (void)setZoomLevel:(id)sender
{
    [self setZoomLevelByLevel:[sender tag]];
    [self invalidateRestorableState];
}

- (void)invertPreviewColors:(id)sender
{
    bool inverted = !self.previewerView->areColorsInverted();
    self.previewerView->setInvertColors(inverted);

    [self invalidateRestorableState];
}

- (void)toggleFollowEditorCursor:(id)sender
{
    [self setFollowEditorCursor:(self.followCursorButton.state == NSControlStateValueOn)];
}

@end
