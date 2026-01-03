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
#include "katvan_editortheme.h"
#include "katvan_highlighter.h"
#include "katvan_spellchecker.h"
#include "katvan_text_utils.h"

#include <QTextDocument>

namespace katvan {

Highlighter::Highlighter(QTextDocument* document, SpellChecker* spellChecker, const EditorTheme& theme)
    : QSyntaxHighlighter(document)
    , d_theme(theme)
    , d_spellChecker(spellChecker)
{
}

static bool isShebangLine(QTextBlock block)
{
    return block.blockNumber() == 0 && block.text().startsWith(QStringLiteral("#!"));
}

static void getBlockInitialParams(QTextBlock block, StateSpanList& initialSpans, QList<parsing::ParserState::Kind>& initialStates)
{
    auto* prevBlockData = BlockData::get<StateSpansBlockData>(block.previous());
    if (prevBlockData != nullptr) {
        for (StateSpan span : prevBlockData->stateSpans())
        {
            if (span.endPos) {
                continue;
            }
            span.startPos.reset();

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
    // markContentDirty being called, and editing updates being lost).

    while (block.isValid()) {
        StateSpansBlockData* blockData = nullptr;

        if (isShebangLine(block)) {
            blockData = new StateSpansBlockData();
        }
        else {
            StateSpanList initialSpans;
            QList<parsing::ParserState::Kind> initialStates;
            getBlockInitialParams(block, initialSpans, initialStates);

            StateSpansListener spanListener(initialSpans);

            QString text = block.text();
            parsing::Parser parser(text, initialStates);

            parser.addListener(spanListener, false);
            parser.parse();

            blockData = new StateSpansBlockData(std::move(spanListener).spans());
        }

        BlockData::set<StateSpansBlockData>(block, blockData);

        // Do not update the user state here, we want a proper rehighlight to
        // happen later if needed.
        if (block.userState() == calculateBlockState(blockData)) {
            break;
        }
        block = block.next();
    }
}

void Highlighter::highlightBlock(const QString& text)
{
    StateSpansBlockData* blockData = nullptr;

    QList<QTextCharFormat> charFormats;
    charFormats.resize(text.size());

    if (isShebangLine(currentBlock())) {
        charFormats.fill(d_theme.highlightingFormat(parsing::HighlightingMarker::Kind::COMMENT));

        blockData = new StateSpansBlockData();
    }
    else {
        StateSpanList initialSpans;
        QList<parsing::ParserState::Kind> initialStates;
        getBlockInitialParams(currentBlock(), initialSpans, initialStates);

        StateSpansListener spanListener(initialSpans);
        parsing::HighlightingListener highlightingListener;
        parsing::ContentWordsListener contentListenger;
        parsing::IsolatesListener isolatesListener;

        parsing::Parser parser(text, initialStates);
        parser.addListener(spanListener, false);
        parser.addListener(highlightingListener, true);
        parser.addListener(contentListenger, true);
        parser.addListener(isolatesListener, true);

        parser.parse();

        doSyntaxHighlighting(highlightingListener, charFormats);
        doSpellChecking(text, contentListenger, charFormats);

        BlockData::set<IsolatesBlockData>(currentBlock(),
            new IsolatesBlockData(std::move(isolatesListener).isolateRanges()));

        blockData = new StateSpansBlockData(std::move(spanListener).spans());
    }

    doShowControlChars(text, charFormats);

    bool formatsChanged = false;
    for (qsizetype i = 0; i < text.size(); i++) {
        setFormat(i, 1, charFormats[i]);
        if (!charFormats[i].isEmpty()) {
            formatsChanged = true;
        }
    }

    // In addition to storing the detailed block data obtained from parsing it as
    // the block's user data, set a hash of that as the block state. This is to
    // force re-highlighting of the next block if something changed - QSyntaxHighlighter
    // only tracks changes to the block state number.
    BlockData::set<StateSpansBlockData>(currentBlock(), blockData);

    int currentState = currentBlockState();
    int newState = calculateBlockState(blockData);
    setCurrentBlockState(newState);

    // If the block state spans have changed, we need to re-layout the block, since
    // some layout decisions are affected by state (e.g base directionality). There
    // is no need to do it if we also set any formats, since in this case
    // QSyntaxHighlighter will call markContentDirty anyway.
    if (!formatsChanged && currentState != newState) {
        document()->markContentsDirty(currentBlock().position(), currentBlock().length());
    }
}

void Highlighter::doSyntaxHighlighting(
    const parsing::HighlightingListener& listener,
    QList<QTextCharFormat>& charFormats)
{
    auto markers = listener.markers();

    for (const auto& m : std::as_const(markers)) {
        QTextCharFormat fmt = d_theme.highlightingFormat(m.kind);

        for (size_t i = m.startPos; i < m.startPos + m.length; i++) {
            charFormats[i].merge(fmt);
        }
    }
}

void Highlighter::doShowControlChars(
    const QString& text,
    QList<QTextCharFormat>& charFormats)
{
    QTextCharFormat controlCharFormat;
    controlCharFormat.setFontFamilies(QStringList() << utils::CONTROL_FONT_FAMILY);

    for (qsizetype i = 0; i < text.size(); i++) {
        if (utils::isBidiControlChar(text[i])) {
            charFormats[i].merge(controlCharFormat);
        }
    }
}

void Highlighter::doSpellChecking(
    const QString& text,
    const parsing::ContentWordsListener& listener,
    QList<QTextCharFormat>& charFormats)
{
    QTextCharFormat misspelledWordFormat;
    misspelledWordFormat.setFontUnderline(true);
    misspelledWordFormat.setUnderlineColor(d_theme.editorColor(EditorTheme::EditorColor::ERROR));
    misspelledWordFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);

    parsing::SegmentList result;
    if (d_spellChecker == nullptr) {
        return;
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

    BlockData::set<SpellingBlockData>(currentBlock(), new SpellingBlockData(std::move(result)));
}

int Highlighter::calculateBlockState(StateSpansBlockData* data)
{
    // Include theme name in hash to ensure it changes when theme changes
    size_t hash = qHashMulti(data->stateSpans().fingerprint(), d_theme.name());

    // The hash is a `size_t`, which is usually 64-bit. But block user state is
    // an `int` which is often 32-bit even on 64-bit systems. Simply truncating
    // can worsen collisions, as I'm not sure that qHash is a particularly
    // high quality hash function. As a mitigation, we can take the remainder
    // from the largest prime just under 2^32.
    static constexpr size_t MAX_PRIME = 4294967231UL;

    return static_cast<int>(hash % MAX_PRIME);
}

}

#include "moc_katvan_highlighter.cpp"
