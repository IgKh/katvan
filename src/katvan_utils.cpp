/*
 * This file is part of Katvan
 * Copyright (c) 2024 Igor Khanin
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
#include "katvan_utils.h"

#include <QDir>
#include <QGuiApplication>

namespace katvan::utils {

static constexpr QChar LRI_MARK = (ushort)0x2066;
static constexpr QChar PDI_MARK = (ushort)0x2069;

QString formatFilePath(QString path)
{
    path = QDir::toNativeSeparators(path);

    if (qGuiApp->layoutDirection() == Qt::RightToLeft) {
        return LRI_MARK + path + PDI_MARK;
    }
    return path;
}

Qt::LayoutDirection naturalTextDirection(const QString& text)
{
    int count = 0;
    for (QChar ch : text) {
        if (count++ > 100) {
            break;
        }

        QChar::Direction direction = ch.direction();
        if (direction == QChar::DirR || direction == QChar::DirAL) {
            return Qt::RightToLeft;
        }
        else if (direction == QChar::DirL) {
            return Qt::LeftToRight;
        }
    }
    return Qt::LayoutDirectionAuto;
}

}
