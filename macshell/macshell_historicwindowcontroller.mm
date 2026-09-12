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
#import "macshell_historicwindowcontroller.h"
#import "macshell_settingsmanager.h"

#include "katvan_editor.h"

#include <QLibraryInfo>
#include <QVersionNumber>

@interface KatvanHistoricWindowController ()

@property (nonatomic) katvan::Editor* editor;

@end

@implementation KatvanHistoricWindowController

- (instancetype)initWithDocument:(katvan::Document*)textDocument
{
    NSRect frame = NSMakeRect(0, 0, 1900, 1000);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:styleMask
                                         backing:NSBackingStoreBuffered
                                         defer:NO];

    self = [super initWithWindow:window];
    if (self) {
        self.editor = new katvan::Editor(textDocument, nullptr);
        if (QLibraryInfo::version() >= QVersionNumber(6, 12)) {
            self.editor->setWindowFlags(self.editor->windowFlags() | Qt::SubWindow);
        }

        NSView* view = (__bridge NSView *)reinterpret_cast<void*>(self.editor->winId());
        view.translatesAutoresizingMaskIntoConstraints = NO;

        [window.contentView addSubview:view];
        [NSLayoutConstraint activateConstraints:@[
            [view.leadingAnchor constraintEqualToAnchor:window.contentView.safeAreaLayoutGuide.leadingAnchor],
            [view.trailingAnchor constraintEqualToAnchor:window.contentView.safeAreaLayoutGuide.trailingAnchor],
            [view.topAnchor constraintEqualToAnchor:window.contentView.safeAreaLayoutGuide.topAnchor],
            [view.bottomAnchor constraintEqualToAnchor:window.contentView.safeAreaLayoutGuide.bottomAnchor]
        ]];

        self.editor->applySettings(KatvanSettingsManager::instance().editorSettings());
        self.editor->setReadOnly(true);
        self.editor->show();
    }
    return self;
}

- (void)dealloc
{
    delete self.editor;
}

@end
