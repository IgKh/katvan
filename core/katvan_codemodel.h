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

#include "katvan_parsing.h"

#include <QList>
#include <QObject>
#include <QTextBlock>
#include <QTextCursor>

#include <optional>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace katvan {

struct StateSpan
{
    unsigned long spanId;
    parsing::ParserState::Kind state;
    std::optional<int> startPos;
    std::optional<int> endPos;
    bool implicitlyClosed;

    friend bool operator<(const StateSpan& lhs, const StateSpan& rhs);
};

class StateSpanList
{
public:
    const QList<StateSpan>& elements() const { return d_elements; }
    QList<StateSpan>& elements() { return d_elements; }

    using const_iterator = QList<StateSpan>::const_iterator;
    const_iterator begin() const { return d_elements.begin(); }
    const_iterator end() const { return d_elements.end(); }

    size_t fingerprint() const;

private:
    QList<StateSpan> d_elements;
};

class StateSpansListener : public parsing::ParsingListener
{
public:
    StateSpansListener(const StateSpanList& initialSpans)
        : d_spans(initialSpans) {}

    const StateSpanList& spans() const & { return d_spans; }
    StateSpanList spans() const && { return d_spans; }

    void initializeState(const parsing::ParserState& state, size_t endMarker) override;
    void finalizeState(const parsing::ParserState& state, size_t endMarker, bool implicit) override;
    void handleInstantState(const parsing::ParserState& state, size_t endMarker) override;

private:
    StateSpanList d_spans;
};

class CodeModel : public QObject
{
    Q_OBJECT

public:
    enum class EnvironmentType
    {
        UNKNOWN = 0,
        CONTENT,
        CODE,
        MATH,
        OTHER,
    };
    Q_ENUM(EnvironmentType);

    CodeModel(QTextDocument* document)
        : QObject(document)
        , d_document(document) {}

    // Perform rough classification on the type of environment (content, math,
    // etc) that the given global position is in.
    EnvironmentType classifyEnvironment(int pos) const;

    // If there is a delimiting bracket at the given global position,
    // find the position of the matching (opening/closing) bracket.
    std::optional<int> findMatchingBracket(int pos) const;

    // Check if should increase the indent level by one in the next block, if
    // inserting a newline at global position _pos_.
    bool shouldIncreaseIndent(int pos) const;

    // For the given global document position, find a previous block where the
    // indent level of the given position should match that block's indent level.
    // If none, returns the position's own block.
    QTextBlock findMatchingIndentBlock(int pos) const;

    // For the given block, find a previous block that opens a span that is
    // closed on the given one, and therefore should have the same indent
    // level. If none, returns the position's own block.
    QTextBlock findMatchingIndentBlock(QTextBlock block) const;

    // For the given block, check if a "left leaning" scope starts in it. "Left
    // leaning" means a code scope whose a-priori directionality (without knowing
    // the content) is Left-to-Right.
    bool startsLeftLeaningSpan(QTextBlock block) const;

    // For the given block, check if its' initial span allows for it to represent
    // a list item.
    bool canStartWithListItem(QTextBlock block) const;

    // Find closing bracket character that should be automatically appended
    // if _openBracket_ is inserted at the given cursor's position.
    std::optional<QChar> getMatchingCloseBracket(QTextCursor cursor, QChar openBracket) const;

    // Get the correct Typst expression to insert the given symbol at the given
    // global position
    QString getSymbolExpression(const QString& symbolName, int pos) const;

    // Get the correct Typst expression for representing the given color at the
    // given global position
    QString getColorExpression(const QColor& color, int pos) const;

    // Get a Typst expression that refers to the given label at the given global
    // position
    QString getLabelRefExpression(const QString& label, int pos) const;

private:
    // Find the inner most state span still in effect at the given global position
    std::optional<StateSpan> spanAtPosition(QTextBlock block, int globalPos) const;

    // Find the relevant "previous" and "current" states for the cursor to consider
    // which brackets can be auto-inserted.
    std::tuple<parsing::ParserState::Kind, parsing::ParserState::Kind> getStatesForBracketInsertion(QTextCursor cursor) const;

    QTextDocument* d_document;
};

}
