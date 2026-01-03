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
#pragma once

#include "katvan_parsing.h"

#include <QColor>
#include <QHash>
#include <QPalette>
#include <QString>
#include <QTextCharFormat>

namespace katvan {

class EditorTheme
{
public:
    enum class EditorColor {
        BACKGROUND,
        FOREGROUND,
        GUTTER,
        CURRENT_LINE,
        ERROR,
        WARNING,
        MATCHING_BRACKET
    };

    static bool isAppInDarkMode();

    static EditorTheme& defaultTheme();
    static EditorTheme& lightTheme();
    static EditorTheme& darkTheme();

    EditorTheme() {};

    QString name() const { return d_name; }

    QPalette adjustPalette(QPalette original) const;

    QTextCharFormat highlightingFormat(parsing::HighlightingMarker::Kind marker) const { return d_highlightingFormats[marker]; }
    QColor editorColor(EditorColor color) const { return d_editorColors[color]; }

private:
    explicit EditorTheme(const QString& themeJsonFile);

    QString d_name;
    QHash<parsing::HighlightingMarker::Kind, QTextCharFormat> d_highlightingFormats;
    QHash<EditorColor, QColor> d_editorColors;
};

}
