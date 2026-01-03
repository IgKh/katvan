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
#import "macshell_colorconverter.h"

#include <QColor>
#include <QVariant>

#import <AppKit/AppKit.h>

QString ColorUtiMimeConverter::mimeForUti(const QString& uti) const
{
    if (uti == QLatin1StringView([NSPasteboardTypeColor UTF8String])) {
        return "application/x-color";
    }
    return QString();
}

QString ColorUtiMimeConverter::utiForMime(const QString& mime) const
{
    if (mime == "application/x-color") {
        return QString::fromNSString(NSPasteboardTypeColor);
    }
    return QString();
}

QVariant ColorUtiMimeConverter::convertToMime(const QString& mime, const QList<QByteArray>& data, const QString& uti) const
{
    Q_UNUSED(mime);
    if (uti != QLatin1StringView([NSPasteboardTypeColor UTF8String]) || data.isEmpty()) {
        return QVariant();
    }

    // Pretty silly, since the data originally came from a NSPasteboard, but we
    // have to put it back into an anonymous one to parse it as a NSColor.
    NSPasteboard* board = [NSPasteboard pasteboardWithUniqueName];
    [board declareTypes:@[NSPasteboardTypeColor] owner:nil];
    [board setData:data[0].toRawNSData() forType:NSPasteboardTypeColor];

    NSColor* color = [NSColor colorFromPasteboard:board];
    [board releaseGlobally];

    if (color == nil) {
        return QVariant();
    }

    NSColor* rgbaColor = [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    return QColor::fromRgba64(
        ushort([rgbaColor redComponent] * 0xFFFF),
        ushort([rgbaColor greenComponent] * 0xFFFF),
        ushort([rgbaColor blueComponent] * 0xFFFF),
        ushort([rgbaColor alphaComponent] * 0xFFFF));
}

QList<QByteArray> ColorUtiMimeConverter::convertFromMime(const QString& mime, const QVariant& data, const QString& uti) const
{
    Q_UNUSED(mime);
    Q_UNUSED(data);
    Q_UNUSED(uti);
    return QList<QByteArray>();
}
