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
#include "katvan_utils.h"

#include <QTextDocument>

namespace katvan {

Highlighter::Highlighter(QTextDocument* document, SpellChecker* spellChecker, const EditorTheme& theme)
    : QSyntaxHighlighter(document)
    , d_theme(theme)
    , d_spellChecker(spellChecker)
{
}

static void getBlockInitialParams(QTextBlock block, StateSpanList& initialSpans, QList<parsing::ParserState::Kind>& initialStates)
{
    auto* prevBlockData = dynamic_cast<HighlighterStateBlockData*>(block.previous().userData());
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
}

void Highlighter::reparseBlock(QTextBlock block)
{
    // Workaround for QTBUG-130318. This method performs a "light" reparse
    // starting from the given block, in order to update its' code spans for
    // the code model, but without changing any formats (which would lead to
    // markContentDirty being called, and editing updates being lost.

    while (block.isValid()) {
        StateSpanList initialSpans;
        QList<parsing::ParserState::Kind> initialStates;
        getBlockInitialParams(block, initialSpans, initialStates);

        StateSpansListener spanListener(initialSpans, block.position());

        QString text = block.text();
        parsing::Parser parser(text, initialStates);

        parser.addListener(spanListener, false);
        parser.parse();

        auto* blockData = new HighlighterStateBlockData(std::move(spanListener).spans(), parsing::SegmentList{});
        block.setUserData(blockData);

        // Do not update the user state here, we want a proper rehighlight to
        // happen later if needed.
        if (block.userState() == blockData->stateSpans().fingerprint()) {
            break;
        }
        block = block.next();
    }
}

void Highlighter::highlightBlock(const QString& text)
{
    StateSpanList initialSpans;
    QList<parsing::ParserState::Kind> initialStates;
    getBlockInitialParams(currentBlock(), initialSpans, initialStates);

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
    doShowControlChars(text, charFormats);

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

void Highlighter::doSyntaxHighlighting(
    const parsing::HighlightingListener& listener,
    QList<QTextCharFormat>& charFormats)
{
    auto markers = listener.markers();

    for (const auto& m : std::as_const(markers)) {
        for (size_t i = m.startPos; i < m.startPos + m.length; i++) {
            charFormats[i].merge(d_theme.highlightingFormat(m.kind));
        }
    }
}

void Highlighter::doShowControlChars(
    const QString& text,
    QList<QTextCharFormat>& charFormats)
{
    QTextCharFormat controlCharFormat;
    controlCharFormat.setFontFamilies(QStringList() << "Unifont");

    for (qsizetype i = 0; i < text.size(); i++) {
        if (utils::isBidiControlChar(text[i])) {
            charFormats[i].merge(controlCharFormat);
        }
    }
}

parsing::SegmentList Highlighter::doSpellChecking(
    const QString& text,
    const parsing::ContentWordsListener& listener,
    QList<QTextCharFormat>& charFormats)
{
    QTextCharFormat misspelledWordFormat;
    misspelledWordFormat.setFontUnderline(true);
    misspelledWordFormat.setUnderlineColor(d_theme.editorColor(EditorTheme::EditorColor::SPELLING_ERROR));
    misspelledWordFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);

    parsing::SegmentList result;
    if (d_spellChecker == nullptr) {
        return result;
    }

    auto segments = listener.segments();
    for (const auto& segment : std::as_const(segments)) {
        auto misspelledWords = d_spellChecker->checkSpelling(text.sliced(segment.startPos, segment.length));
        for (const auto& [wordPos, len] : std::as_const(misspelledWords)) {
            size_t start = segment.startPos + wordPos;
            for (size_t i = 0; i < len; i++) {
                charFormats[start + i].merge(misspelledWordFormat);
            }

            result.append(parsing::ContentSegment{ start, len });
        }
    }
    return result;
}

}
