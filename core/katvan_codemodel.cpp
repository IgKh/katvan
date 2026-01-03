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
        // If startPos has no value, it is like negative infinity
        if (!lhs.startPos) {
            return true;
        }
        if (!rhs.startPos) {
            return false;
        }
        return lhs.startPos.value() < rhs.startPos.value();
    }
    if (lhs.endPos != rhs.endPos) {
        // If endPos has no value, it is like infinity
        if (!lhs.endPos) {
            return true;
        }
        if (!rhs.endPos) {
            return false;
        }
        return lhs.endPos.value() > rhs.endPos.value();
    }
    return lhs.spanId < rhs.spanId;
}

constexpr inline size_t qHash(const StateSpan& span, size_t seed = 0) noexcept
{
    return qHashMulti(seed,
        span.spanId,
        span.state,
        span.startPos.value_or(0),
        span.endPos.value_or(0),
        span.implicitlyClosed);
}

size_t StateSpanList::fingerprint() const
{
    return qHashRange(d_elements.begin(), d_elements.end());
}

void StateSpansListener::initializeState(const parsing::ParserState& state, size_t endMarker)
{
    Q_UNUSED(endMarker);

    d_spans.elements().append(StateSpan {
        g_spanIdCounter++,
        state.kind,
        static_cast<int>(state.startPos),
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
            it->endPos = static_cast<int>(endMarker);
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
        static_cast<int>(state.startPos),
        static_cast<int>(endMarker),
        true
    });
}

static constexpr int MAX_BLOCKS_TO_SCAN = 1000;

static std::optional<int> findSpanEndPosition(unsigned long spanId, QTextBlock fromBlock)
{
    int blocksScanned = 0;
    for (QTextBlock block = fromBlock; block.isValid(); block = block.next(), blocksScanned++) {
        if (blocksScanned > MAX_BLOCKS_TO_SCAN) {
            return std::nullopt;
        }

        auto* blockData = BlockData::get<StateSpansBlockData>(block);
        if (!blockData) {
            continue;
        }

        for (const StateSpan& span : blockData->stateSpans()) {
            if (span.spanId == spanId) {
                if (span.endPos) {
                    return block.position() + span.endPos.value();
                }
                else {
                    break;
                }
            }
        }
    }
    return std::nullopt;
}

static std::optional<int> findSpanStartPosition(unsigned long spanId, QTextBlock untilBlock)
{
    int blocksScanned = 0;
    for (QTextBlock block = untilBlock; block.isValid(); block = block.previous(), blocksScanned++) {
        if (blocksScanned > MAX_BLOCKS_TO_SCAN) {
            return std::nullopt;
        }

        auto* blockData = BlockData::get<StateSpansBlockData>(block);
        if (!blockData) {
            continue;
        }

        for (const StateSpan& span : blockData->stateSpans()) {
            if (span.spanId == spanId) {
                if (span.startPos) {
                    return block.position() + span.startPos.value();
                }
                else {
                    break;
                }
            }
        }
    }
    return std::nullopt;
}

static QTextBlock findSpanStartBlock(QTextDocument* doc, const StateSpan& span, QTextBlock untilBlock)
{
    if (span.startPos) {
        // Assumption - span was started in untilBlock
        return untilBlock;
    }

    std::optional<int> startPos = findSpanStartPosition(span.spanId, untilBlock.previous());
    if (!startPos) {
        return untilBlock;
    }

    return doc->findBlock(startPos.value());
}

static bool isDelimitedState(State state)
{
    return state == State::CONTENT_BLOCK
        || state == State::MATH
        || state == State::MATH_ARGUMENTS
        || state == State::CODE_BLOCK
        || state == State::CODE_ARGUMENTS;
}

static bool isIndentingState(State state)
{
    return state == State::CONTENT_BLOCK
        || state == State::CODE_BLOCK
        || state == State::CODE_ARGUMENTS
        || state == State::MATH_ARGUMENTS;
}

static bool isLeftLeaningState(State state)
{
    return state == State::CODE_BLOCK
        || state == State::CODE_LINE
        || state == State::MATH
        || state == State::CONTENT_RAW
        || state == State::CONTENT_RAW_BLOCK;
}

CodeModel::EnvironmentType CodeModel::classifyEnvironment(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return EnvironmentType::UNKNOWN;
    }

    auto span = spanAtPosition(block, pos);
    if (!span) {
        return EnvironmentType::CONTENT;
    }

    if (parsing::isContentHolderStateKind(span->state)) {
        return EnvironmentType::CONTENT;
    }
    else if (parsing::isCodeHolderStateKind(span->state)) {
        return EnvironmentType::CODE;
    }
    else if (parsing::isMathHolderStateKind(span->state)) {
        return EnvironmentType::MATH;
    }
    return EnvironmentType::OTHER;
}

std::optional<int> CodeModel::findMatchingBracket(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return std::nullopt;
    }

    int posInBlock = pos - block.position();

    auto* blockData = BlockData::get<StateSpansBlockData>(block);
    if (!blockData) {
        return std::nullopt;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    for (const StateSpan& span : blockData->stateSpans()) {
        if (span.startPos && span.startPos.value() > posInBlock) {
            break;
        }
        if (!isDelimitedState(span.state)) {
            continue;
        }

        if (span.endPos == posInBlock) {
            if (span.startPos) {
                return block.position() + span.startPos.value();
            }
            return findSpanStartPosition(span.spanId, block.previous());
        }
        else if (span.startPos == posInBlock) {
            if (span.endPos) {
                return block.position() + span.endPos.value();
            }
            return findSpanEndPosition(span.spanId, block.next());
        }
    }
    return std::nullopt;
}

std::optional<StateSpan> CodeModel::spanAtPosition(QTextBlock block, int globalPos) const
{
    Q_ASSERT(d_document->findBlock(globalPos) == block);

    int posInBlock = globalPos - block.position();

    auto* blockData = BlockData::get<StateSpansBlockData>(block);
    if (!blockData) {
        return std::nullopt;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    const auto& spans = blockData->stateSpans().elements();
    for (auto rit = spans.rbegin(); rit != spans.rend(); ++rit) {
        if ((!rit->startPos.has_value() || rit->startPos.value() < posInBlock) &&
            (!rit->endPos.has_value()   || rit->endPos.value() >= posInBlock)) {
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

    QTextBlock startBlock = findSpanStartBlock(d_document, span.value(), block);
    return startBlock == block;
}

QTextBlock CodeModel::findMatchingIndentBlock(int pos) const
{
    QTextBlock block = d_document->findBlock(pos);
    if (!block.isValid()) {
        return block;
    }

    int posInBlock = pos - block.position();

    auto* blockData = BlockData::get<StateSpansBlockData>(block);
    if (!blockData) {
        return block;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    // Find a relevant state span that ends on pos exactly.
    for (const StateSpan& span : blockData->stateSpans()) {
        if (span.startPos && span.startPos.value() > posInBlock) {
            break;
        }
        if (!isIndentingState(span.state)) {
            continue;
        }

        if (span.endPos == posInBlock) {
            return findSpanStartBlock(d_document, span, block);
        }
    }
    return block;
}

QTextBlock CodeModel::findMatchingIndentBlock(QTextBlock block) const
{
    auto* blockData = BlockData::get<StateSpansBlockData>(block);
    if (!blockData) {
        return block;
    }
    Q_ASSERT(std::is_sorted(blockData->stateSpans().begin(), blockData->stateSpans().end()));

    // Find the first relevant state span that ends in the block
    for (const StateSpan& span : blockData->stateSpans()) {
        if (!isIndentingState(span.state) || !span.endPos) {
            continue;
        }

        return findSpanStartBlock(d_document, span, block);
    }
    return block;
}

bool CodeModel::startsLeftLeaningSpan(QTextBlock block) const
{
    auto* blockData = BlockData::get<StateSpansBlockData>(block);
    if (!blockData) {
        return false;
    }

    for (const StateSpan& span : blockData->stateSpans()) {
        if (span.startPos >= 0 && isLeftLeaningState(span.state)) {
            return true;
        }
    }
    return false;
}

bool CodeModel::canStartWithListItem(QTextBlock block) const
{
    auto span = spanAtPosition(block, block.position());
    return !span
        || span->state == State::CONTENT
        || span->state == State::CONTENT_BLOCK;
}

std::tuple<State, State> CodeModel::getStatesForBracketInsertion(QTextCursor cursor) const
{
    State prevState = State::INVALID;
    State currState = State::INVALID;

    // First, there might be an interesting state that the parser may have
    // already implicitly closed; e.g. block scoped states closed by reaching
    // end of line, or certain "instant" states. These take precedence in
    // being the "current" state - while they are also the "previous" state.
    if (!cursor.atBlockStart()) {
        auto span = spanAtPosition(cursor.block(), cursor.position() - 1);
        if (span) {
            prevState = span->state;
            if (span->implicitlyClosed) {
                currState = span->state;
            }
        }
    }

    if (currState == State::INVALID) {
        auto span = spanAtPosition(cursor.block(), cursor.position());
        if (span) {
            currState = span->state;
        }
        else {
            currState = State::CONTENT;
        }
    }

    return std::make_tuple(prevState, currState);
}

std::optional<QChar> CodeModel::getMatchingCloseBracket(QTextCursor cursor, QChar openBracket) const
{
    auto [prevState, state] = getStatesForBracketInsertion(cursor);

    QChar prevChar;
    if (!cursor.atBlockStart()) {
        prevChar = cursor.block().text().at(cursor.positionInBlock() - 1);
    }

    bool isInCode = parsing::isCodeHolderStateKind(state);
    bool isInMath = parsing::isMathHolderStateKind(state);
    bool isInContent = parsing::isContentHolderStateKind(state);

    bool isCodeFunctionCall = state == State::CODE_VARIABLE_NAME  // Possibly part of a function call
                                || state == State::CODE_FUNCTION_NAME; // Definitely part of a function call

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
        if (isInCode || isCodeFunctionCall || ((isInContent || isInMath) && prevChar == QLatin1Char('#')) ||
            prevState == State::CODE_ARGUMENTS || prevState == State::CONTENT_BLOCK) {
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

QString CodeModel::getSymbolExpression(const QString& symbolName, int pos) const
{
    EnvironmentType env = classifyEnvironment(pos);
    if (env == EnvironmentType::UNKNOWN || symbolName.isEmpty()) {
        return QString();
    }

    if (env == EnvironmentType::OTHER || env == EnvironmentType::CODE) {
        return symbolName;
    }
    if (env == EnvironmentType::MATH && symbolName.startsWith("sym.")) {
        return symbolName.sliced(4);
    }
    return QLatin1Char('#') + symbolName;
}

QString CodeModel::getColorExpression(const QColor& color, int pos) const
{
    EnvironmentType env = classifyEnvironment(pos);
    if (env == EnvironmentType::UNKNOWN || !color.isValid()) {
        return QString();
    }

    QString name;
    if (color.alpha() == 255) {
        name = color.name(QColor::HexRgb);
    }
    else {
        // Qt emits hex in #AARRGGBB format, but Typst expects #RRGGBBAA
        name = color.name(QColor::HexArgb);
        name = QStringLiteral("#%1%2").arg(name.sliced(3), name.sliced(1, 2));
    }

    QString expression = QStringLiteral("rgb(\"%1\")").arg(name);

    if (env == EnvironmentType::CODE) {
        return expression;
    }
    return QLatin1Char('#') + expression;
}

QString CodeModel::getLabelRefExpression(const QString& label, int pos) const
{
    EnvironmentType env = classifyEnvironment(pos);
    if (env == EnvironmentType::UNKNOWN || label.isEmpty()) {
        return QString();
    }

    if (env == EnvironmentType::CONTENT) {
        return QStringLiteral("@%1").arg(label);
    }
    return QStringLiteral("\"%1\"").arg(label);
}

}

#include "moc_katvan_codemodel.cpp"
