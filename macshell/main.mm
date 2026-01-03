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
#import "macshell_appdelegate.h"

#include <QString>
#include <QtLogging>

#import <AppKit/AppKit.h>

void logToNsLog(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(context);

    NSString* nsMsg = msg.toNSString();
    switch (type) {
        case QtDebugMsg:
            NSLog(@"Debug: %@", nsMsg);
            break;
        case QtInfoMsg:
            NSLog(@"Info: %@", nsMsg);
            break;
        case QtWarningMsg:
            NSLog(@"Warning: %@", nsMsg);
            break;
        case QtCriticalMsg:
            NSLog(@"Critical: %@", nsMsg);
            break;
        case QtFatalMsg:
            NSLog(@"Fatal: %@", nsMsg);
            abort();
    }
}

int main(int argc, char** argv)
{
    qInstallMessageHandler(logToNsLog);

    NSApplication* app = [NSApplication sharedApplication];
    KatvanAppDelegate* delegate = [[KatvanAppDelegate alloc] initWithArgc:argc Argv:argv];

    app.delegate = delegate;

    [app run];
    return 0;
}
