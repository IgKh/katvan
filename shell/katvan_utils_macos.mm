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
#include "katvan_utils_macos.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QWidget>

#import <AppKit/AppKit.h>

namespace katvan::utils::macos {

QString getApplicationDir()
{
    NSBundle* bundle = [NSBundle mainBundle];
    NSString* path = [[bundle bundlePath] stringByDeletingLastPathComponent];
    return QString::fromNSString(path);
}

QString showPdfExportDialog(QWidget* parent, const QString& sourceFilePath)
{
    NSPDFPanel* dialog = [NSPDFPanel panel];
    NSPDFInfo* info = [[NSPDFInfo alloc] init];

    if (!sourceFilePath.isEmpty()) {
        QFileInfo fileInfo(sourceFilePath);
        dialog.defaultFileName = fileInfo.baseName().toNSString(); // No .pdf suffix
    }

    NSWindow* window = nil;
    if (parent != nullptr) {
        NSView* view = reinterpret_cast<NSView*>(parent->winId());
        window = [view window];
    }

    __block QEventLoop* loop = new QEventLoop();
    __block NSInteger ok;
    [dialog beginSheetWithPDFInfo: info
            modalForWindow: window
            completionHandler: ^(NSInteger rc) {
                ok = rc;
                loop->quit();
            }
    ];

    loop->exec();
    delete loop;

    QString result;
    if (ok) {
        result = QString::fromNSString([info.URL path]);
    }

    [info release];
    return result;
}

}
