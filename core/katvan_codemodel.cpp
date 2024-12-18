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

#include "katvan_codemodel.h"
#include "katvan_highlighter.h"

#include <QHash>
#include <QTextDocument>

#include <algorithm>

namespace katvan {

using State = parsing::ParserState::Kind;

static unsigned long g_spanIdCounter = 0;

bool operator<(const StateSpan& lhs, const StateSpan& rhs)
{
    if (lhs.startPos != rhs.startPos) {
        return lhs.startPos < rhs.startPos;
    }
    if (lhs.endPos != rhs.endPos) {
        // If endPos has no value, it is like infinity
        return !lhs.endPos.has_value() || lhs.endPos > rhs.endPos;
    }
    return lhs.spanId < rhs.spanId;
}

constexpr inline size_t qHash(const StateSpan& span, size_t seed = 0) noexcept
{
    return qHashMulti(seed, span.spanId, span.state, span.startPos, span.endPos.value_or(0));
}

int StateSpanList::fingerprint() const
{
    return static_cast<int>(qHashRange(d_elements.begin(), d_elements.end()));
}

void StateSpansListener::initializeState(const parsing::ParserState& state, size_t endMarker)
{
    Q_UNUSED(endMarker);

    d_spans.elements().append(StateSpan {
        g_spanIdCounter++,
        state.kind,
        d_basePos + static_cast<int>(state.startPos),
        std::nullopt,
        false
    });
}

void StateSpansListener::finalizeState(const parsing::ParserState& state, size_t endMarker, bool implicit)
{
    // Find last unended state
    for (auto it = d_spans.elements().rbegin(); it != d_spans.elements().rend(); ++it) {
        if (it->endPos) {
            continue;
        }

        if (it->state == state.kind) {
            it->endPos = d_basePos + static_cast<int>(endMarker);
            it->implicitlyClosed = implicit;
        }
        break;
    }
}

void StateSpansListener::handleInstantState(const parsing::ParserState& state, size_t endMarker)
{
    if (state.kind != State::CODE_VARIABLE_NAME && state.kind != State::CODE_FUNCTION_NAME) {
        return;
    }

    d_spans.elements().append(StateSpan {
        g_spanIdCounter++,
        state.kind,
        d_basePos + static_cast<int>(state.startPos),
        d_basePos + static_cast<int>(endMarker),
        true
    });
}

static std::optional<int> findSpanEndPosition(unsigned long spanId, QTextBlock fromBlock)
{
    static constexpr int MAX_BLOCKS_TO_SCAN = 1000;

    int blocksScanned = 0;
    for (QTextBlock block = fromBlock; block.isValid(); block = block.next(), blocksScanned++) {
        if (blocksScanned > MAX_BLOCKS_TO_SCAN) {
            return std::nullopt;
        }

        auto* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
        if (!blockData) {
            continue;
        }

        for (const StateSpan& span : blockData->stateSpans()) {
            if (span.spanId == spanId) {
                if (span.endPos) {
                    return span.endPos.value();
                }
                else {
                    break;
                }
            }
        }
    }
    return std::nullopt;
}

static bool isDelimitedState(State state)
{
    return state == State::CONTENT_BLOCK
        || state == State::MATH
        || state == State::MATH_ARGUMENTS
        || state == State::CODE_BLOCK
        || state == State::CODE_ARGUMENTS;
}

std::optional<int> CodeModel::findMatchingBracket(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return std::nullopt;
    }

    auto* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
    if (!blockData) {
        return std::nullopt;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    for (const StateSpan& span : blockData->stateSpans()) {
        if (span.startPos > pos) {
            break;
        }
        if (!isDelimitedState(span.state)) {
            continue;
        }

        if (span.endPos == pos) {
            return span.startPos;
        }
        else if (span.startPos == pos) {
            if (span.endPos) {
                return span.endPos.value();
            }
            return findSpanEndPosition(span.spanId, block.next());
        }
    }
    return std::nullopt;
}

static bool isIndentingState(State state)
{
    return state == State::CONTENT_BLOCK
        || state == State::CODE_BLOCK
        || state == State::CODE_ARGUMENTS
        || state == State::MATH_ARGUMENTS;
}

std::optional<StateSpan> CodeModel::spanAtPosition(QTextBlock block, int globalPos) const
{
    Q_ASSERT(d_document->findBlock(globalPos) == block);

    auto* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
    if (!blockData) {
        return std::nullopt;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    const auto& spans = blockData->stateSpans().elements();
    for (auto rit = spans.rbegin(); rit != spans.rend(); ++rit) {
        if (rit->startPos < globalPos && (!rit->endPos.has_value() || rit->endPos.value() >= globalPos)) {
            return *rit;
        }
    }
    return std::nullopt;
}

bool CodeModel::shouldIncreaseIndent(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return false;
    }

    auto span = spanAtPosition(block, pos);
    if (!span) {
        return false;
    }

    if (!isIndentingState(span->state)) {
        return false;
    }

    QTextBlock startBlock = d_document->findBlock(span->startPos);
    return startBlock == block;
}

QTextBlock CodeModel::findMatchingIndentBlock(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return block;
    }

    auto* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
    if (!blockData) {
        return block;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    // Find a relevant state span that ends on pos exactly.
    for (const StateSpan& span : blockData->stateSpans()) {
        if (span.startPos > pos) {
            break;
        }
        if (!isIndentingState(span.state)) {
            continue;
        }

        if (span.endPos == pos) {
            return d_document->findBlock(span.startPos);
        }
    }
    return block;
}

QTextBlock CodeModel::findMatchingIndentBlock(QTextBlock block) const
{
    auto* blockData = dynamic_cast<HighlighterStateBlockData*>(block.userData());
    if (!blockData) {
        return block;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    // Find the first relevant state span that intersects with the block
    for (const StateSpan& span : blockData->stateSpans()) {
        if (!isIndentingState(span.state)) {
            continue;
        }
        return d_document->findBlock(span.startPos);
    }
    return block;
}

parsing::ParserState::Kind CodeModel::getStateForBracketInsertion(QTextCursor cursor) const
{
    // First, there might be an interesting state that the parser may have
    // already implicitly closed; e.g. block scoped states closed by reaching
    // end of line, or certain "instant" states. These take precedence.
    if (!cursor.atBlockStart()) {
        auto span = spanAtPosition(cursor.block(), cursor.position() - 1);
        if (span && span->implicitlyClosed) {
            return span->state;
        }
    }

    auto span = spanAtPosition(cursor.block(), cursor.position());
    if (span) {
        return span->state;
    }
    return State::CONTENT;
}

std::optional<QChar> CodeModel::getMatchingCloseBracket(QTextCursor cursor, QChar openBracket) const
{
    State state = getStateForBracketInsertion(cursor);

    QChar prevChar;
    if (!cursor.atBlockStart()) {
        prevChar = cursor.block().text().at(cursor.positionInBlock() - 1);
    }

    bool isInCode = state == State::CODE_BLOCK
                    || state == State::CODE_ARGUMENTS
                    || state == State::CODE_LINE;

    bool isCodeFunctionCall = state == State::CODE_VARIABLE_NAME  // Possibly part of a function call
                                || state == State::CODE_FUNCTION_NAME; // Definitely part of a function call

    bool isInMath = state == State::MATH
                    || state == State::MATH_ARGUMENTS;

    bool isInContent = state == State::CONTENT
                        || state == State::CONTENT_BLOCK
                        || state == State::CONTENT_HEADING
                        || state == State::CONTENT_EMPHASIS
                        || state == State::CONTENT_STRONG_EMPHASIS;

    bool isInRaw = state == State::CONTENT_RAW
                    || state == State::CONTENT_RAW_BLOCK;

    if (openBracket == QLatin1Char('(')) {
        if (isInCode || isCodeFunctionCall || isInMath || (isInContent && prevChar == QLatin1Char('#'))) {
            return QLatin1Char(')');
        }
    }
    else if (openBracket == QLatin1Char('{')) {
        if (isInCode || ((isInContent || isInMath) && prevChar == QLatin1Char('#'))) {
            return QLatin1Char('}');
        }
    }
    else if (openBracket == QLatin1Char('[')) {
        if (isInCode || isCodeFunctionCall || ((isInContent || isInMath) && prevChar == QLatin1Char('#'))) {
            return QLatin1Char(']');
        }
    }
    else if (openBracket == QLatin1Char('$')) {
        if (isInCode || (isInContent && prevChar != QLatin1Char('\\'))) {
            return QLatin1Char('$');
        }
    }
    else if (openBracket == QLatin1Char('"')) {
        if (isInCode || isInMath
            || ((isInContent || isInRaw) &&
                prevChar != QLatin1Char('\\') &&
                (prevChar.script() != QChar::Script_Hebrew || cursor.hasSelection()))) {
            return QLatin1Char('"');
        }
    }
    else if (openBracket == QLatin1Char('<')) {
        if (isInContent) {
            return QLatin1Char('>');
        }
    }
    else if (openBracket == QLatin1Char('*')) {
        if (isInContent && state != State::CONTENT_STRONG_EMPHASIS) {
            return QLatin1Char('*');
        }
    }
    else if (openBracket == QLatin1Char('_')) {
        if (isInContent && state != State::CONTENT_EMPHASIS) {
            return QLatin1Char('_');
        }
    }
    return std::nullopt;
}

}
