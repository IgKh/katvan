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
#import "macshell_sidebar.h"

@interface KatvanSidebar ()

@property (nonatomic) NSSegmentedControl* sidebarTabList;
@property (nonatomic) NSTabView* sidebarTabView;
@property (nonatomic) NSMutableArray<NSViewController*>* controllers;

@end

@implementation KatvanSidebar

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.controllers = [NSMutableArray array];

        [self setupSidebarTabList];
        [self setupSidebarTabView];
    }
    return self;
}

- (void)setupSidebarTabList
{
    self.sidebarTabList = [[NSSegmentedControl alloc] init];
    self.sidebarTabList.segmentStyle = NSSegmentStyleSeparated;
    self.sidebarTabList.trackingMode = NSSegmentSwitchTrackingSelectOne;
    self.sidebarTabList.target = self;
    self.sidebarTabList.action = @selector(sidebarTabClicked:);
    self.sidebarTabList.translatesAutoresizingMaskIntoConstraints = NO;

    [self.view addSubview:self.sidebarTabList];

    [NSLayoutConstraint activateConstraints:@[
        [self.sidebarTabList.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:8],
        [self.sidebarTabList.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:8],
        [self.sidebarTabList.trailingAnchor constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-8],
        [self.sidebarTabList.centerXAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    ]];
}

- (void)setupSidebarTabView
{
    self.sidebarTabView = [[NSTabView alloc] init];
    self.sidebarTabView.tabViewType = NSNoTabsNoBorder;
    self.sidebarTabView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.view addSubview:self.sidebarTabView];

    [NSLayoutConstraint activateConstraints:@[
        [self.sidebarTabView.topAnchor constraintEqualToAnchor:self.sidebarTabList.bottomAnchor constant:8],
        [self.sidebarTabView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.sidebarTabView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.sidebarTabView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
    ]];
}

- (void)addTabController:(NSViewController*)controller
        icon:(NSImage*)icon
        toolTip:(NSString*)toolTip
{
    if (!controller) {
        return;
    }

    [self.controllers addObject:controller];

    // Add to tab list
    NSInteger count = self.sidebarTabList.segmentCount;
    self.sidebarTabList.segmentCount = count + 1;

    if (icon) {
        [self.sidebarTabList setImage:icon forSegment:count];
    }
    if (toolTip != nil) {
        [self.sidebarTabList setToolTip:toolTip forSegment:count];
    }

    // Add to tab view
    NSTabViewItem* item = [[NSTabViewItem alloc] initWithIdentifier:nil];
    item.view = controller.view;

    [self.sidebarTabView addTabViewItem:item];
}

- (void)ensureControllerSelected:(NSViewController*)controller
{
    NSInteger index = [self.controllers indexOfObject:controller];
    if (index < 0) {
        return;
    }

    self.sidebarTabList.selectedSegment = index;

    NSTabViewItem* item = [self.sidebarTabView tabViewItemAtIndex:index];
    [self.sidebarTabView selectTabViewItem:item];
}

- (void)sidebarTabClicked:(id)sender
{
    NSInteger index = self.sidebarTabList.selectedSegment;
    if (index < 0) {
        return;
    }

    NSTabViewItem* item = [self.sidebarTabView tabViewItemAtIndex:index];
    [self.sidebarTabView selectTabViewItem:item];
}

@end
