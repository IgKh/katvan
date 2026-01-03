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

#include "katvan_codemodel.h"
#include "katvan_document.h"
#include "katvan_parsing.h"

#include <QHash>
#include <QSyntaxHighlighter>

namespace katvan {

class EditorTheme;
class SpellChecker;

class StateSpansBlockData : public QTextBlockUserData
{
public:
    static constexpr BlockDataKind DATA_KIND = BlockDataKind::STATE_SPANS;

    StateSpansBlockData() {}

    StateSpansBlockData(StateSpanList&& stateSpans)
        : d_stateSpans(std::move(stateSpans)) {}

    const StateSpanList& stateSpans() const { return d_stateSpans; }

private:
    StateSpanList d_stateSpans;
};

class SpellingBlockData : public QTextBlockUserData
{
public:
    static constexpr BlockDataKind DATA_KIND = BlockDataKind::SPELLING;

    SpellingBlockData(parsing::SegmentList&& misspelledWords)
        : d_misspelledWords(std::move(misspelledWords)) {}

    const parsing::SegmentList& misspelledWords() const { return d_misspelledWords; }

private:
    parsing::SegmentList d_misspelledWords;
};

class IsolatesBlockData : public QTextBlockUserData
{
public:
    static constexpr BlockDataKind DATA_KIND = BlockDataKind::ISOLATES;

    IsolatesBlockData(parsing::IsolateRangeList&& ranges)
        : d_ranges(std::move(ranges)) {}

    const parsing::IsolateRangeList& isolates() const & { return d_ranges; }
    parsing::IsolateRangeList isolates() const && { return d_ranges; }

private:
    parsing::IsolateRangeList d_ranges;
};

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument* document, SpellChecker* spellChecker, const EditorTheme& theme);

    void reparseBlock(QTextBlock block);

protected:
    void highlightBlock(const QString& text) override;

private:
    void doSyntaxHighlighting(
        const parsing::HighlightingListener& listener,
        QList<QTextCharFormat>& charFormats);

    void doShowControlChars(
        const QString& text,
        QList<QTextCharFormat>& charFormats);

    void doSpellChecking(
        const QString& text,
        const parsing::ContentWordsListener& listener,
        QList<QTextCharFormat>& charFormats);

    int calculateBlockState(StateSpansBlockData* data);

    const EditorTheme& d_theme;
    SpellChecker* d_spellChecker;
};

}
