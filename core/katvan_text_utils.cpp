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
#include "katvan_text_utils.h"

namespace katvan::utils {

bool isBidiControlChar(QChar ch)
{
    return ch == ALM_MARK
        || ch == LRM_MARK
        || ch == RLM_MARK
        || ch == LRI_MARK
        || ch == RLI_MARK
        || ch == PDI_MARK;
}

Qt::LayoutDirection naturalTextDirection(const QString& text)
{
    int count = 0;
    const QChar* ptr = text.constData();
    const QChar* end = ptr + text.length();

    while (ptr < end) {
        if (count++ > 100) {
            break;
        }

        uint codepoint = ptr->unicode();
        if (QChar::isHighSurrogate(codepoint) && ptr + 1 < end) {
            ushort low = ptr[1].unicode();
            if (QChar::isLowSurrogate(low)) {
                codepoint = QChar::surrogateToUcs4(codepoint, low);
                ptr++;
            }
        }

        QChar::Direction direction = QChar::direction(codepoint);
        if (direction == QChar::DirR || direction == QChar::DirAL) {
            return Qt::RightToLeft;
        }
        else if (direction == QChar::DirL) {
            return Qt::LeftToRight;
        }
        ptr++;
    }
    return Qt::LayoutDirectionAuto;
}

}
