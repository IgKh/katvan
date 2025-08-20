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
#pragma once

#include <QIcon>
#include <QString>

namespace katvan::utils {

static constexpr QLatin1StringView CONTROL_FONT_FAMILY = QLatin1StringView("KatvanControl");
static constexpr QLatin1StringView BLANK_FONT_FAMILY = QLatin1StringView("Adobe Blank");
static constexpr QLatin1StringView SYMBOL_FONT_FAMILY = QLatin1StringView("Noto Sans Math");

static constexpr QChar ALM_MARK = (ushort)0x061c;
static constexpr QChar LRM_MARK = (ushort)0x200e;
static constexpr QChar RLM_MARK = (ushort)0x200f;
static constexpr QChar LRI_MARK = (ushort)0x2066;
static constexpr QChar RLI_MARK = (ushort)0x2067;
static constexpr QChar FSI_MARK = (ushort)0x2068;
static constexpr QChar PDI_MARK = (ushort)0x2069;

bool isBidiControlChar(QChar ch);
bool isSingleBidiMark(QChar ch);
bool isWhitespace(QChar ch);
bool isAllWhitespace(const QString& text);

Qt::LayoutDirection naturalTextDirection(const QString& text);

char32_t firstCodepointOf(const QString& str);

QIcon fontIcon(QChar ch);
QIcon fontIcon(QChar ch, const QFont& font);
QIcon fontIcon(char32_t codepoint, const QFont& font);

void loadAuxiliaryFonts();

}
