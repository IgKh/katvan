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
#include "katvan_editortheme.h"
#include "katvan_highlighter.h"
#include "katvan_spellchecker.h"

#include <QTextDocument>

namespace katvan {

Highlighter::Highlighter(QTextDocument* document, SpellChecker* spellChecker)
    : QSyntaxHighlighter(document)
    , d_spellChecker(spellChecker)
{
    setupFormats();
}

void Highlighter::setupFormats()
{
    bool isDark = EditorTheme::isAppInDarkMode();

    d_formats.clear();

    QTextCharFormat commentFormat;
    commentFormat.setForeground(isDark ? QColor(0xb0, 0xb3, 0xc2) : QColor(0x8a, 0x8a, 0x8a));
    d_formats.insert(parsing::HiglightingMarker::Kind::COMMENT, commentFormat);

    QTextCharFormat stringLiteralFormat;
    stringLiteralFormat.setForeground(isDark ? QColor(0x80, 0xf4, 0xb6) : QColor(0x29, 0x8e, 0x0d));
    d_formats.insert(parsing::HiglightingMarker::Kind::STRING_LITERAL, stringLiteralFormat);

    QTextCharFormat numberLiteralFormat;
    numberLiteralFormat.setForeground(isDark ? QColor(0xff, 0x7d, 0x79) : QColor(0xb6, 0x01, 0x57));
    d_formats.insert(parsing::HiglightingMarker::Kind::NUMBER_LITERAL, numberLiteralFormat);

    QTextCharFormat escapeFormat;
    escapeFormat.setForeground(isDark ? QColor(0xd9, 0xf8, 0xf4) : QColor(0x1d, 0x6c, 0x76));
    d_formats.insert(parsing::HiglightingMarker::Kind::ESCAPE, escapeFormat);
    d_formats.insert(parsing::HiglightingMarker::Kind::MATH_OPERATOR, escapeFormat);

    QTextCharFormat mathDelimiterFormat;
    mathDelimiterFormat.setForeground(isDark ? QColor(0x80, 0xf4, 0xb6) : QColor(0x29, 0x8e, 0x0d));
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

    QTextCharFormat urlFormat;
    urlFormat.setFontUnderline(true);
    d_formats.insert(parsing::HiglightingMarker::Kind::URL, urlFormat);

    QTextCharFormat rawFormat;
    rawFormat.setForeground(QColor(0x81, 0x81, 0x81));
    d_formats.insert(parsing::HiglightingMarker::Kind::RAW, rawFormat);

    QTextCharFormat labelAndReferenceFormat;
    labelAndReferenceFormat.setForeground(isDark ? QColor(0xd9, 0xf8, 0xf4) : QColor(0x1d, 0x6c, 0x76));
    d_formats.insert(parsing::HiglightingMarker::Kind::LABEL, labelAndReferenceFormat);
    d_formats.insert(parsing::HiglightingMarker::Kind::REFERENCE, labelAndReferenceFormat);

    QTextCharFormat listEntryFormat;
    listEntryFormat.setForeground(isDark ? QColor(0xca, 0xa8, 0xff) : QColor(0x8b, 0x41, 0xb1));
    d_formats.insert(parsing::HiglightingMarker::Kind::LIST_ENTRY, listEntryFormat);

    QTextCharFormat termFormat;
    termFormat.setFontWeight(QFont::ExtraBold);
    d_formats.insert(parsing::HiglightingMarker::Kind::TERM, termFormat);

    QTextCharFormat variableNameFormat;
    variableNameFormat.setForeground(isDark ? QColor(0xca, 0xa8, 0xff) : QColor(0x8b, 0x41, 0xb1));
    d_formats.insert(parsing::HiglightingMarker::Kind::VARIABLE_NAME, variableNameFormat);

    QTextCharFormat functionNameFormat;
    functionNameFormat.setForeground(isDark ? QColor(0x7c, 0xd5, 0xff) : QColor(0x4b, 0x69, 0xc6));
    d_formats.insert(parsing::HiglightingMarker::Kind::FUNCTION_NAME, functionNameFormat);

    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(isDark ? QColor(0xff, 0xa4, 0x9d) : QColor(0xd7, 0x3a, 0x49));
    d_formats.insert(parsing::HiglightingMarker::Kind::KEYWORD, keywordFormat);

    d_misspelledWordFormat.setFontUnderline(true);
    d_misspelledWordFormat.setUnderlineColor(Qt::red);
    d_misspelledWordFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
}

void Highlighter::highlightBlock(const QString& text)
{
    auto* prevBlockData = dynamic_cast<HighlighterStateBlockData*>(currentBlock().previous().userData());

    StateSpanList initialSpans;
    QList<parsing::ParserState::Kind> initialStates;
    if (prevBlockData != nullptr) {
        for (const StateSpan& span : prevBlockData->stateSpans())
        {
            if (span.endPos) {
                continue;
            }
            initialSpans.elements().append(span);
            initialStates.append(span.state);
        }
    }

    StateSpansListener spanListener(initialSpans, currentBlock().position());
    parsing::HighlightingListener highlightingListener;
    parsing::ContentWordsListener contentListenger;

    parsing::Parser parser(text, initialStates);
    parser.addListener(spanListener, false);
    parser.addListener(highlightingListener, true);
    parser.addListener(contentListenger, true);

    parser.parse();

    QList<QTextCharFormat> charFormats;
    charFormats.resize(text.size());

    doSyntaxHighlighting(highlightingListener, charFormats);

    parsing::SegmentList misspelledWords;
    misspelledWords = doSpellChecking(text, contentListenger, charFormats);

    for (qsizetype i = 0; i < text.size(); i++) {
        setFormat(i, 1, charFormats[i]);
    }

    // In addition to storing the detailed block data obtained from parsing it as
    // the block's user data, set a hash of that as the block state. This is to
    // force re-highlighting of the next block if something changed - QSyntaxHighlighter
    // only tracks changes to the block state number.
    auto* blockData = new HighlighterStateBlockData(std::move(spanListener).spans(), std::move(misspelledWords));
    setCurrentBlockState(blockData->stateSpans().fingerprint());
    setCurrentBlockUserData(blockData);
}

void Highlighter::doSyntaxHighlighting(parsing::HighlightingListener& listener, QList<QTextCharFormat>& charFormats)
{
    auto markers = listener.markers();

    for (const auto& m : markers) {
        for (size_t i = m.startPos; i < m.startPos + m.length; i++) {
            charFormats[i].merge(d_formats.value(m.kind));
        }
    }
}

parsing::SegmentList Highlighter::doSpellChecking(
    const QString& text,
    parsing::ContentWordsListener& listener,
    QList<QTextCharFormat>& charFormats)
{
    auto segments = listener.segments();

    parsing::SegmentList result;

    for (const auto& segment : segments) {
        auto misspelledWords = d_spellChecker->checkSpelling(text.sliced(segment.startPos, segment.length));
        for (const auto& [wordPos, len] : misspelledWords) {
            size_t start = segment.startPos + wordPos;
            for (size_t i = 0; i < len; i++) {
                charFormats[start + i].merge(d_misspelledWordFormat);
            }

            result.append(parsing::ContentSegment{ start, len });
        }
    }
    return result;
}

}
