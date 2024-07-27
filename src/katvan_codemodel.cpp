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
    return qHashMulti(seed, span.spanId, span.state, span.startPos.value_or(0));
}

int StateSpanList::fingerprint() const
{
    return static_cast<int>(qHashRange(d_elements.begin(), d_elements.end()));
}

void StateSpansListener::initializeState(const parsing::ParserState& state, size_t endMarker)
{
    Q_UNUSED(endMarker);

    d_spans.elements().append(StateSpan { g_spanIdCounter++, state.kind, d_basePos + state.startPos, std::nullopt });
}

void StateSpansListener::finalizeState(const parsing::ParserState& state, size_t endMarker)
{
    // Find last unended state
    for (auto it = d_spans.elements().rbegin(); it != d_spans.elements().rend(); ++it) {
        if (it->endPos) {
            continue;
        }

        // If the state kind differs, it means it is an "instant" state
        if (it->state == state.kind) {
            it->endPos = d_basePos + endMarker;
        }
        break;
    }
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
            Q_ASSERT(span.startPos.has_value());
            return span.startPos.value();
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

}
