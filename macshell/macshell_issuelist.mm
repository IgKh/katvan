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

@property (nonatomic) katvan::DiagnosticsModel* model;

@end

@implementation KatvanIssueList

- (instancetype)initWithModel:(katvan::DiagnosticsModel*)model
{
    self = [super init];
    if (self) {
        self.model = model;

        __weak __typeof__(self) weakSelf = self;
        QObject::connect(self.model, &QAbstractItemModel::modelReset,
                         self.model, [weakSelf]() {
            [weakSelf.tableView reloadData];
        });
        QObject::connect(self.model, &QAbstractItemModel::rowsInserted,
                         self.model, [weakSelf](const QModelIndex&, int first, int last) {
            NSRange range = NSMakeRange(first, last - first + 1);
            NSIndexSet* indexSet = [NSIndexSet indexSetWithIndexesInRange:range];
            [weakSelf.tableView insertRowsAtIndexes:indexSet withAnimation:NSTableViewAnimationEffectNone];
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

- (void)issueSelected:(id)sender
{
    QModelIndex index = self.model->index(self.tableView.clickedRow, 0);

    auto location = self.model->getSourceLocation(index);
    if (location) {
        const auto [line, column] = *location;
        [self.target goToBlock:line column:column];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    return self.model->rowCount(QModelIndex());
}

- (id)tableView:(NSTableView*)tableView objectValueForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row
{
    return nil;
}

- (NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)tableColumn row:(NSInteger)row
{
    IssueLabel* view = [tableView makeViewWithIdentifier:@"issueLabel" owner:self];
    if (view == nil) {
        view = [[IssueLabel alloc] init];
        view.identifier = @"issueLabel";
    }

    QModelIndex messageIndex = self.model->index(row, katvan::DiagnosticsModel::COLUMN_MESSAGE);
    QModelIndex locationIndex = self.model->index(row, katvan::DiagnosticsModel::COLUMN_SOURCE_LOCATION);
    QModelIndex severityIndex = self.model->index(row, katvan::DiagnosticsModel::COLUMN_SEVERITY);

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

@end
