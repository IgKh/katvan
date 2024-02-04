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

#include <vector>

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

    QTextCharFormat escapeFormat;
    escapeFormat.setForeground(QColor(0x29, 0x8e, 0x0d));
    d_formats.insert(parsing::HiglightingMarker::Kind::ESCAPE, escapeFormat);

    QTextCharFormat mathDelimiterFormat;
    mathDelimiterFormat.setForeground(QColor(0x1d, 0x6c, 0x76));
    d_formats.insert(parsing::HiglightingMarker::Kind::MATH_DELIMITER, mathDelimiterFormat);

    QTextCharFormat headingFormat;
    headingFormat.setFontWeight(QFont::DemiBold);
    headingFormat.setFontUnderline(true);
    d_formats.insert(parsing::HiglightingMarker::Kind::HEADING, headingFormat);

    QTextCharFormat emphasisFormat;
    emphasisFormat.setFontItalic(true);
    d_formats.insert(parsing::HiglightingMarker::Kind::EMPHASIS, emphasisFormat);

    QTextCharFormat strongEmphasisFormat;
    strongEmphasisFormat.setFontWeight(QFont::Bold);
    d_formats.insert(parsing::HiglightingMarker::Kind::STRONG_EMPHASIS, strongEmphasisFormat);

    QTextCharFormat rawFormat;
    rawFormat.setForeground(QColor(0x81, 0x81, 0x81));
    d_formats.insert(parsing::HiglightingMarker::Kind::RAW, rawFormat);

    QTextCharFormat labelAndReferenceFormat;
    labelAndReferenceFormat.setForeground(QColor(0x1d, 0x6c, 0x76));
    d_formats.insert(parsing::HiglightingMarker::Kind::LABEL, labelAndReferenceFormat);
    d_formats.insert(parsing::HiglightingMarker::Kind::REFERENCE, labelAndReferenceFormat);

    QTextCharFormat listEntryFormat;
    listEntryFormat.setForeground(QColor(0x8b, 0x41, 0xb1));
    d_formats.insert(parsing::HiglightingMarker::Kind::LIST_ENTRY, listEntryFormat);

    QTextCharFormat termFormat;
    termFormat.setFontWeight(QFont::ExtraBold);
    d_formats.insert(parsing::HiglightingMarker::Kind::TERM, termFormat);
}

void Highlighter::highlightBlock(const QString& text)
{
    auto* prevBlockData = dynamic_cast<HighlighterStateBlockData*>(currentBlock().previous().userData());
    const parsing::ParserStateStack* initialState = nullptr;
    if (prevBlockData != nullptr) {
        initialState = prevBlockData->stateStack();
    }

    parsing::HighlightingListener listener;
    parsing::Parser parser(text, listener, initialState);

    parser.parse();
    auto markers = listener.markers();

    std::vector<QTextCharFormat> charFormats;
    charFormats.resize(text.size());
    for (const auto& m : markers) {
        for (size_t i = m.startPos; i < m.startPos + m.length; i++) {
            charFormats[i].merge(d_formats.value(m.kind));
        }
    }

    for (size_t i = 0; i < text.size(); i++) {
        setFormat(i, 1, charFormats[i]);
    }

    setCurrentBlockUserData(new HighlighterStateBlockData(parser.stateStack()));
}

}
