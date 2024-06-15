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
#pragma once

#include "katvan_parsing.h"

#include <QHash>
#include <QSyntaxHighlighter>

namespace katvan {

class SpellChecker;

class HighlighterStateBlockData : public QTextBlockUserData
{
public:
    HighlighterStateBlockData(
        parsing::ParserStateStack&& stateStack,
        parsing::SegmentList&& misspelledWords)
        : d_stateStack(std::move(stateStack))
        , d_misspelledWords(std::move(misspelledWords)) {}

    const parsing::ParserStateStack* stateStack() const { return &d_stateStack; }
    const parsing::SegmentList& misspelledWords() const { return d_misspelledWords; }

    int fingerprint() const;

private:
    parsing::ParserStateStack d_stateStack;
    parsing::SegmentList d_misspelledWords;
};

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument* document, SpellChecker* spellChecker);

    void setupFormats();

protected:
    void highlightBlock(const QString& text) override;

private:
    void doSyntaxHighlighting(
        parsing::HighlightingListener& listener,
        QList<QTextCharFormat>& charFormats);

    parsing::SegmentList doSpellChecking(
        const QString& text,
        parsing::ContentWordsListener& listener,
        QList<QTextCharFormat>& charFormats);

    SpellChecker* d_spellChecker;

    QHash<parsing::HiglightingMarker::Kind, QTextCharFormat> d_formats;
    QTextCharFormat d_misspelledWordFormat;
};

}
