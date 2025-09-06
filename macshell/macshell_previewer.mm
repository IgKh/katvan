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
#import "macshell_previewer.h"

static const NSInteger kZoomLevelFitPage = -1;
static const NSInteger kZoomLevelFitWidth = -2;
static const NSSize kPageNumberLabelPadding = NSMakeSize(10, 10);

@interface PageNumberLabelCell : NSTextFieldCell
@end

@implementation PageNumberLabelCell

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

    [[NSColor controlAccentColor] setFill];
    [path fill];
    [path setClip];

    [super drawWithFrame:cellFrame inView:controlView];
}

@end

@interface KatvanPreviewer ()

@property (nonatomic) katvan::PreviewerView* previewerView;
@property (nonatomic) NSPopUpButton* zoomLevelsPopUp;
@property (nonatomic) NSTextField* currentPageLabel;

@end

@implementation KatvanPreviewer

- (instancetype)initWithDriver:(katvan::TypstDriverWrapper *)driver
{
    self = [super init];
    if (self) {
        self.previewerView = new katvan::PreviewerView(driver);

        __weak __typeof__(self) weakSelf = self;

        QObject::connect(driver, &katvan::TypstDriverWrapper::previewReady,
                         self.previewerView, [weakSelf](QList<katvan::typstdriver::PreviewPageData> pages) {
                            weakSelf.previewerView->setPages(pages);
                            [weakSelf updatePageLabel];
                         });

        QObject::connect(self.previewerView, &katvan::PreviewerView::currentPageChanged,
                         self.previewerView, [weakSelf]() {
                            [weakSelf updatePageLabel];
                         });
    }
    return self;
}

- (void)dealloc
{
    delete self.previewerView;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    NSView* previewerNsView = (__bridge NSView *)reinterpret_cast<void*>(self.previewerView->winId());

    [self.view addSubview:previewerNsView];

    previewerNsView.translatesAutoresizingMaskIntoConstraints = NO;

    self.currentPageLabel = [NSTextField labelWithString:@""];
    self.currentPageLabel.cell = [[PageNumberLabelCell alloc] init];
    self.currentPageLabel.textColor = [NSColor alternateSelectedControlTextColor]; // Contrasts with accent color
    self.currentPageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.currentPageLabel];

    [NSLayoutConstraint activateConstraints:@[
        [previewerNsView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [previewerNsView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [previewerNsView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [previewerNsView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.currentPageLabel.centerXAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
        [self.currentPageLabel.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-20],
        [self.view.widthAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumWidth()],
        [self.view.heightAnchor constraintGreaterThanOrEqualToConstant:self.previewerView->minimumHeight()],
    ]];

    self.previewerView->show();
}

- (NSPopUpButton*)makeZoomLevelPopup
{
    NSMenu* levelsMenu = [[NSMenu alloc] init];
    NSMenuItem* item;

    // A pull-down button always displays its menu's first item on the button
    // itself, so create a placeholder
    [levelsMenu addItemWithTitle:@"[Placeholder]" action:nil keyEquivalent:@""];

    item = [levelsMenu addItemWithTitle:@"Fit Page" action:@selector(setZoomLevel:) keyEquivalent:@""];
    [item setTarget:self];
    [item setTag:kZoomLevelFitPage];

    item = [levelsMenu addItemWithTitle:@"Fit Width" action:@selector(setZoomLevel:) keyEquivalent:@""];
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

    [self.zoomLevelsPopUp setTitle:@"100%"];

    return self.zoomLevelsPopUp;
}

- (qreal)currentRoundZoomFactor
{
    qreal factor = self.previewerView->effectiveZoom(self.previewerView->currentPage());
    return qRound(factor * 20) / 20.0; // Round to nearest multiple of 5%
}

- (void)setCustomZoom:(qreal)factor
{
    self.previewerView->setCustomZoomFactor(factor);
    if (self.zoomLevelsPopUp != nil) {
        NSString* title = [NSString stringWithFormat:@"%d%%", qRound(factor * 100)];
        [self.zoomLevelsPopUp setTitle:title];
    }
}

- (void)updatePageLabel
{
    int pageCount = self.previewerView->pageCount();
    int currentPage = self.previewerView->currentPage();
    NSString* page = self.previewerView->pageLabel(currentPage).toNSString();

    NSString* label = [NSString stringWithFormat:NSLocalizedString(@"Page %@ of %d", nil), page, pageCount];
    self.currentPageLabel.stringValue = label;
}

- (void)zoomToActualSize:(id)sender
{
    [self setCustomZoom:1.0];
}

- (void)zoomIn:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] + 0.05;
    [self setCustomZoom:factor];
}

- (void)zoomOut:(id)sender
{
    qreal factor = [self currentRoundZoomFactor] - 0.05;
    [self setCustomZoom:factor];
}

- (void)setZoomLevel:(id)sender
{
    NSInteger tag = [sender tag];
    if (tag >= 0) {
        qreal factor = tag / 100.0;
        [self setCustomZoom:factor];
    }
    else {
        if (tag == kZoomLevelFitPage) {
            self.previewerView->setZoomMode(katvan::PreviewerView::ZoomMode::FitToPage);
        }
        else if (tag == kZoomLevelFitWidth) {
            self.previewerView->setZoomMode(katvan::PreviewerView::ZoomMode::FitToWidth);
        }

        [self.zoomLevelsPopUp setTitle:[sender title]];
    }
}

@end
