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

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument* document, SpellChecker* spellChecker);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupFormats();

    void doSyntaxHighlighting(parsing::HighlightingListener& listener, QList<QTextCharFormat>& charFormats);
    void doSpellChecking(const QString& text, parsing::ContentWordsListener& listener, QList<QTextCharFormat>& charFormats);

    SpellChecker* d_spellChecker;

    QHash<parsing::HiglightingMarker::Kind, QTextCharFormat> d_formats;
    QTextCharFormat d_misspelledWordFormat;
};

}
