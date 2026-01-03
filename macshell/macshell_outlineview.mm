// -*- mode: objective-cpp -*-
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
#import "macshell_outlineview.h"

#include "katvan_outlinemodel.h"

#include <algorithm>
#include <set>
#include <vector>

struct IndexPathComparator
{
public:
    bool operator()(NSIndexPath* rhs, NSIndexPath* lhs) const
    {
        return [rhs compare:lhs] == NSOrderedDescending;
    }
};

@interface KatvanOutlineView ()

@property (nonatomic) NSScrollView* scrollView;
@property (nonatomic) NSOutlineView* outlineView;

@property (nonatomic) katvan::OutlineModel* model;

@end

@implementation KatvanOutlineView
{
    std::set<NSIndexPath*, IndexPathComparator> d_items;
    NSInteger d_currentLine;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.model = new katvan::OutlineModel();

        d_currentLine = -1;
    }
    return self;
}

- (void)dealloc
{
    delete self.model;
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

    self.outlineView = [[NSOutlineView alloc] init];
    self.outlineView.delegate = self;
    self.outlineView.dataSource = self;
    self.outlineView.headerView = nil;
    self.outlineView.focusRingType = NSFocusRingTypeNone;
    self.outlineView.target = self;
    self.outlineView.action = @selector(outlineEntrySelected:);

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"headingsColumn"];
    [self.outlineView addTableColumn:column];

    self.outlineView.outlineTableColumn = column;
    self.scrollView.documentView = self.outlineView;

    [self.view addSubview:self.scrollView];

    [NSLayoutConstraint activateConstraints:@[
        [self.scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
    ]];
}

- (void)setOutline:(katvan::typstdriver::OutlineNode*)outline
{
    self.model->setOutline(outline);

    d_currentLine = -1;
    d_items.clear();

    [self.outlineView reloadData];
    [self.outlineView expandItem:Nil expandChildren:YES];

    NSUserInterfaceLayoutDirection dir = self.model->isRightToLeft()
        ? NSUserInterfaceLayoutDirectionRightToLeft
        : NSUserInterfaceLayoutDirectionLeftToRight;

    self.outlineView.userInterfaceLayoutDirection = dir;
}

- (void)selectEntryForLine:(int)line
{
    if (d_currentLine == line) {
        return;
    }
    d_currentLine = line;

    QModelIndex index = self.model->indexForDocumentLine(line);
    if (!index.isValid()) {
        return;
    }

    NSIndexPath* item = [self modelIndexToItem:index];

    // Find lowest visible parent of the item
    NSInteger rowToSelect = [self.outlineView rowForItem:item];
    while (rowToSelect == -1 && item != nil) {
        item = [self memoizePath:[item indexPathByRemovingLastIndex]];
        rowToSelect = [self.outlineView rowForItem:item];
    }

    [self.outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:rowToSelect] byExtendingSelection:NO];
}

- (void)outlineEntrySelected:(id)sender
{
    id item = [self.outlineView itemAtRow:self.outlineView.selectedRow];

    QModelIndex index = [self itemToModelIndex:item];
    if (!index.isValid()) {
        return;
    }

    int line = index.data(katvan::OutlineModel::POSITION_LINE_ROLE).toInt();
    int column = index.data(katvan::OutlineModel::POSITION_COLUMN_ROLE).toInt();

    [self.target goToBlock:line column:column];
}

- (NSIndexPath*)memoizePath:(NSIndexPath*)path
{
    auto it = d_items.find(path);
    if (it != d_items.end()) {
        return *it;
    }

    d_items.insert(path);
    return path;
}

- (QModelIndex)itemToModelIndex:(id)item
{
    if (item == nil) {
        return QModelIndex();
    }
    NSIndexPath* path = item;

    QModelIndex modelIndex;
    for (NSUInteger i = 0; i < [path length]; i++) {
        modelIndex = self.model->index([path indexAtPosition:i], 0, modelIndex);
    }
    return modelIndex;
}

- (id)modelIndexToItem:(QModelIndex)index
{
    if (!index.isValid()) {
        return nil;
    }

    std::vector<NSUInteger> indexes;
    while (index.parent().isValid()) {
        indexes.push_back(index.row());
        index = index.parent();
    }
    std::reverse(indexes.begin(), indexes.end());

    NSIndexPath* path = [NSIndexPath indexPathWithIndexes:indexes.data() length:indexes.size()];
    return [self memoizePath:path];
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item
{
    QModelIndex index = [self itemToModelIndex:item];
    return self.model->rowCount(index);
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item
{
    QModelIndex index = [self itemToModelIndex:item];
    return self.model->hasChildren(index);
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(id)item
{
    if (item == nil) {
        return [self memoizePath:[NSIndexPath indexPathWithIndex:index]];
    }

    NSIndexPath* path = item;
    return [self memoizePath:[path indexPathByAddingIndex:index]];
}

- (id)outlineView:(NSOutlineView*)outlineView objectValueForTableColumn:(NSTableColumn*)tableColumn byItem:(id)item
{
    return item;
}

- (NSView*)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn*)tableColumn item:(id)item
{
    NSTableCellView* view = [self.outlineView makeViewWithIdentifier:@"headingLabel" owner:self];
    if (view == nil) {
        NSTextField* label = [NSTextField labelWithString:@""];
        label.translatesAutoresizingMaskIntoConstraints = NO;

        view = [[NSTableCellView alloc] init];
        view.identifier = @"headingLabel";

        [view addSubview:label];
        [view setTextField:label];

        [NSLayoutConstraint activateConstraints:@[
            [label.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:8],
            [label.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-8],
            [label.centerYAnchor constraintEqualToAnchor:view.centerYAnchor]
        ]];
    }

    QModelIndex modelIndex = [self itemToModelIndex:item];

    view.textField.stringValue = modelIndex.data().toString().toNSString();
    view.textField.alignment = (outlineView.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft)
        ? NSTextAlignmentRight
        : NSTextAlignmentLeft;

    return view;
}

@end
