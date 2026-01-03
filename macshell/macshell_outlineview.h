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
#import "macshell_windowcontroller.h"

#import <AppKit/AppKit.h>

namespace katvan::typstdriver { class OutlineNode; }

@interface KatvanOutlineView : NSViewController <NSOutlineViewDataSource, NSOutlineViewDelegate>

@property (nonatomic, weak) id<KatvanGoToBlockTarget> target;

- (void)setOutline:(katvan::typstdriver::OutlineNode*)outline;
- (void)selectEntryForLine:(int)line;

@end
