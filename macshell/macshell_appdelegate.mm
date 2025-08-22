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
#import "macshell_appdelegate.h"
#import "macshell_mainmenu.h"

#include "katvan_aboutdialog.h"

#include <AppKit/AppKit.h>

@implementation KatvanAppDelegate

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    [KatvanMainMenu setupMainMenu];
}

- (void)showAboutDialog:(id)sender
{
    katvan::AboutDialog dialog;
    dialog.exec();
}

- (void)openTypstDocs:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://typst.app/docs/"]];
}

@end
