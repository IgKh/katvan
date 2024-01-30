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
#include "katvan_highlighter.h"

#include <QTextDocument>

namespace katvan {

class HighlighterStateBlockData : public QTextBlockUserData
{
public:
    HighlighterStateBlockData(parsing::ParserStateStack&& stateStack)
        : d_stateStack(std::move(stateStack)) {}

    const parsing::ParserStateStack* stateStack() const { return &d_stateStack; }

private:
    parsing::ParserStateStack d_stateStack;
};

Highlighter::Highlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
    setupFormats();
}

void Highlighter::setupFormats()
{
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(0x8a, 0x8a, 0x8a));
    d_formats.insert(parsing::HiglightingMarker::Kind::COMMENT, commentFormat);

    QTextCharFormat stringLiteralFormat;
    stringLiteralFormat.setForeground(QColor(0x29, 0x8e, 0x0d));
    d_formats.insert(parsing::HiglightingMarker::Kind::STRING_LITERAL, stringLiteralFormat);

    QTextCharFormat headingFormat;
    headingFormat.setFontWeight(QFont::DemiBold);
    headingFormat.setFontUnderline(true);
    d_formats.insert(parsing::HiglightingMarker::Kind::HEADING, headingFormat);
}

void Highlighter::highlightBlock(const QString& text)
{
    auto* prevBlockData = dynamic_cast<HighlighterStateBlockData*>(currentBlock().previous().userData());
    const parsing::ParserStateStack* initialState = nullptr;
    if (prevBlockData != nullptr) {
        initialState = prevBlockData->stateStack();
    }

    parsing::Parser parser(text, initialState);
    auto markers = parser.getHighlightingMarkers();
    for (const auto& m : markers) {
        setFormat(m.startPos, m.length, d_formats.value(m.kind));
    }

    setCurrentBlockUserData(new HighlighterStateBlockData(parser.stateStack()));
}

}
