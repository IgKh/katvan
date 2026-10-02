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
#import "macshell_issuelist.h"

#include "katvan_diagnosticsmodel.h"

@interface IssueLabel : NSTableCellView

@property (nonatomic) NSTextField* locationField;

@end

@implementation IssueLabel

- (instancetype)init
{
    self = [super init];
    if (self) {
        NSTextField* label = [NSTextField wrappingLabelWithString:@""];
        label.translatesAutoresizingMaskIntoConstraints = NO;
        label.alignment = NSTextAlignmentLeft;
        label.font = [NSFont systemFontOfSize:NSFont.systemFontSize];

        self.locationField = [NSTextField labelWithString:@""];
        self.locationField.translatesAutoresizingMaskIntoConstraints = NO;
        self.locationField.alignment = NSTextAlignmentLeft;
        self.locationField.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
        self.locationField.textColor = NSColor.secondaryLabelColor;

        NSImageView* iconView = [[NSImageView alloc] init];
        iconView.translatesAutoresizingMaskIntoConstraints = NO;
        iconView.symbolConfiguration = [NSImageSymbolConfiguration configurationWithTextStyle:NSFontTextStyleBody];

        [iconView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
        [iconView setContentCompressionResistancePriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];

        [self addSubview:label];
        [self addSubview:self.locationField];
        [self addSubview:iconView];

        [self setTextField:label];
        [self setImageView:iconView];

        const CGFloat padding = 6.0;
        const CGFloat iconTextSpacing = 8.0;

        // Explicitly use leftAnchor/rightAnchor here instead of leading/trailing, because
        // issue labels are supposed to be LTR always (or at least until Typst has localized
        // error messages, if ever)
        [NSLayoutConstraint activateConstraints:@[
            [iconView.leftAnchor constraintEqualToAnchor:self.leftAnchor constant:padding],
            [iconView.bottomAnchor constraintEqualToAnchor:label.firstBaselineAnchor],

            [label.topAnchor constraintEqualToAnchor:self.topAnchor constant:padding],
            [label.leftAnchor constraintEqualToAnchor:iconView.rightAnchor constant:iconTextSpacing],
            [label.rightAnchor constraintEqualToAnchor:self.rightAnchor constant:-padding],

            [self.locationField.topAnchor constraintEqualToAnchor:label.bottomAnchor constant:2.0],
            [self.locationField.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-padding],
            [self.locationField.leftAnchor constraintEqualToAnchor:iconView.rightAnchor constant:iconTextSpacing],
            [self.locationField.rightAnchor constraintEqualToAnchor:self.rightAnchor constant:-padding],
        ]];
    }
    return self;
}

@end

@interface KatvanIssueList ()

@property (nonatomic) NSScrollView* scrollView;
@property (nonatomic) NSTableView* tableView;

@property (nonatomic) katvan::DiagnosticsModel* compilationModel;
@property (nonatomic) katvan::DiagnosticsModel* exportModel;

@end

@implementation KatvanIssueList

- (instancetype)initWithDriver:(katvan::TypstDriverWrapper*)driver
{
    self = [super init];
    if (self) {
        self.compilationModel = driver->compilationDiagnosticsModel();
        self.exportModel = driver->exportDiagnosticsModel();

        __weak __typeof__(self) weakSelf = self;

        QObject::connect(self.compilationModel, &QAbstractItemModel::modelReset,
                         self.compilationModel, [weakSelf]() {
            [weakSelf.tableView reloadData];
        });
        QObject::connect(self.compilationModel, &QAbstractItemModel::rowsInserted,
                         self.compilationModel, [weakSelf](const QModelIndex&, int first, int last) {
            [weakSelf relayAddedRowsFromFirst:first toLast:last withOffset:1];
        });

        QObject::connect(self.exportModel, &QAbstractItemModel::modelReset,
                         self.exportModel, [weakSelf]() {
            [weakSelf.tableView reloadData];
        });
        QObject::connect(self.exportModel, &QAbstractItemModel::rowsInserted,
                         self.exportModel, [weakSelf](const QModelIndex&, int first, int last) {
            [weakSelf relayAddedRowsFromFirst:first toLast:last withOffset:[weakSelf numberOfCompilationIssues] + 2];
        });
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.scrollView = [[NSScrollView alloc] init];
    self.scrollView.hasVerticalScroller = YES;
    self.scrollView.hasHorizontalScroller = NO;
    self.scrollView.autohidesScrollers = YES;
    self.scrollView.borderType = NSNoBorder;
    self.scrollView.drawsBackground = NO;
    self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;

    self.tableView = [[NSTableView alloc] init];
    self.tableView.delegate = self;
    self.tableView.dataSource = self;
    self.tableView.headerView = nil;
    self.tableView.focusRingType = NSFocusRingTypeNone;
    self.tableView.usesAutomaticRowHeights = YES;
    self.tableView.target = self;
    self.tableView.action = @selector(issueSelected:);

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"issuesColumn"];
    [self.tableView addTableColumn:column];

    self.scrollView.documentView = self.tableView;

    [self.view addSubview:self.scrollView];

    [NSLayoutConstraint activateConstraints:@[
        [self.scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
    ]];
}

- (void)scrollToExportIssues
{
    [self.tableView scrollRowToVisible:[self numberOfCompilationIssues] + 2];
}

- (NSInteger)numberOfCompilationIssues
{
    return self.compilationModel->rowCount(QModelIndex());
}

- (NSInteger)numberOfExportIssues
{
    return self.exportModel->rowCount(QModelIndex());
}

- (std::tuple<katvan::DiagnosticsModel*, int>)modelAndActualRowForRow:(NSInteger)row
{
    if (row > 0 && row <= [self numberOfCompilationIssues]) {
        return std::make_tuple(self.compilationModel, row - 1);
    }
    if (row > [self numberOfCompilationIssues] + 1) {
        return std::make_tuple(self.exportModel, row - [self numberOfCompilationIssues] - 2);
    }
    return std::make_tuple(nullptr, 0);
}

- (void)relayAddedRowsFromFirst:(int)first toLast:(int)last withOffset:(int)offset
{
    NSInteger location = first + offset;
    NSInteger num = last - first + 1;
    if (location > [self.tableView numberOfRows]) {
        // In case this adds the optional "Export" group header
        location -= 1;
        num += 1;
    }

    NSRange range = NSMakeRange(location, num);
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndexesInRange:range];
    [self.tableView insertRowsAtIndexes:indexSet withAnimation:NSTableViewAnimationEffectNone];
}

- (void)issueSelected:(id)sender
{
    auto [model, actualRow] = [self modelAndActualRowForRow:self.tableView.clickedRow];
    if (!model) {
        return;
    }

    QModelIndex index = model->index(actualRow, 0);

    auto location = model->getSourceLocation(index);
    if (location) {
        const auto [line, column] = *location;
        [self.target goToBlock:line column:column];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    NSInteger count = 1 + [self numberOfCompilationIssues];

    NSInteger exportIssuesCount = [self numberOfExportIssues];
    if (exportIssuesCount > 0) {
        count = count + 1 + exportIssuesCount;
    }
    return count;
}

- (BOOL)tableView:(NSTableView*)tableView isGroupRow:(NSInteger)row
{
    return row == 0 || row == ([self numberOfCompilationIssues] + 1);
}

- (id)tableView:(NSTableView*)tableView objectValueForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row
{
    return nil;
}

- (NSView*)tableView:(NSTableView*)tableView viewForHeaderRow:(NSInteger)row
{
    NSTextField* view = [tableView makeViewWithIdentifier:@"headerView" owner:self];
    if (!view) {
        view = [NSTextField labelWithString:@""];
        view.identifier = @"headerView";
    }

    if (row == 0) {
        view.stringValue = NSLocalizedString(@"Compilation", "Issues section header");
    }
    else if (row == [self numberOfCompilationIssues] + 1) {
        NSDate* timestamp = self.exportModel->lastTimestamp().toNSDate();

        NSDateFormatterStyle dateStyle = NSDateFormatterShortStyle;
        if ([[NSCalendar currentCalendar] isDateInToday:timestamp]) {
            dateStyle = NSDateFormatterNoStyle;
        }

        NSString* timestampStr = [NSDateFormatter
            localizedStringFromDate:timestamp
            dateStyle:dateStyle
            timeStyle:NSDateFormatterShortStyle];

        NSString* label = NSLocalizedString(@"Last export", "Issues section header");
        view.stringValue = [NSString stringWithFormat:@"%@ (%@)", label, timestampStr];
    }
    return view;
}

- (NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row
{
    if ([self tableView:tableView isGroupRow:row]) {
        return [self tableView:tableView viewForHeaderRow:row];
    }

    IssueLabel* view = [tableView makeViewWithIdentifier:@"issueLabel" owner:self];
    if (view == nil) {
        view = [[IssueLabel alloc] init];
        view.identifier = @"issueLabel";
    }

    auto [model, actualRow] = [self modelAndActualRowForRow:row];
    if (!model) {
        return nil;
    }

    QModelIndex messageIndex = model->index(actualRow, katvan::DiagnosticsModel::COLUMN_MESSAGE);
    QModelIndex locationIndex = model->index(actualRow, katvan::DiagnosticsModel::COLUMN_SOURCE_LOCATION);
    QModelIndex severityIndex = model->index(actualRow, katvan::DiagnosticsModel::COLUMN_SEVERITY);

    view.textField.stringValue = messageIndex.data().toString().toNSString();
    [view.textField invalidateIntrinsicContentSize];

    view.locationField.stringValue = locationIndex.data().toString().toNSString();

    auto kind = severityIndex
        .data(katvan::DiagnosticsModel::ROLE_DIAGNOSTIC_KIND)
        .value<katvan::typstdriver::Diagnostic::Kind>();

    NSImage* icon = nil;
    switch (kind) {
        case katvan::typstdriver::Diagnostic::Kind::NOTE:
            icon = [NSImage imageWithSystemSymbolName:@"info.circle" accessibilityDescription:nil];
            view.imageView.contentTintColor = NSColor.systemBlueColor;
            break;
        case katvan::typstdriver::Diagnostic::Kind::WARNING:
            icon = [NSImage imageWithSystemSymbolName:@"exclamationmark.circle" accessibilityDescription:nil];
            view.imageView.contentTintColor = NSColor.systemOrangeColor;
            break;
        case katvan::typstdriver::Diagnostic::Kind::ERROR:
            icon = [NSImage imageWithSystemSymbolName:@"x.circle" accessibilityDescription:nil];
            view.imageView.contentTintColor = NSColor.systemRedColor;
            break;
    }

    view.imageView.image = icon;
    view.imageView.toolTip = severityIndex.data(Qt::ToolTipRole).toString().toNSString();

    return view;
}

- (BOOL)tableView:(NSTableView*)tableView shouldSelectRow:(NSInteger)row
{
    return ![self tableView:tableView isGroupRow:row];
}

@end
