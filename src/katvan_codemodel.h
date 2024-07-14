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

#include <QList>
#include <QObject>

#include <optional>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace katvan {

struct StateSpan
{
    unsigned long spanId;
    parsing::ParserState::Kind state;
    std::optional<size_t> startPos;
    std::optional<size_t> endPos;

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

    int fingerprint() const;

private:
    QList<StateSpan> d_elements;
};

class StateSpansListener : public parsing::ParsingListener
{
public:
    StateSpansListener(const StateSpanList& initialSpans, size_t basePos)
        : d_spans(initialSpans), d_basePos(basePos) {}

    const StateSpanList& spans() const & { return d_spans; }
    StateSpanList spans() const && { return d_spans; }

    void initializeState(const parsing::ParserState& state, size_t endMarker) override;
    void finalizeState(const parsing::ParserState& state, size_t endMarker) override;

private:
    StateSpanList d_spans;
    size_t d_basePos;
};

class CodeModel : public QObject
{
    Q_OBJECT

public:
    CodeModel(QTextDocument* document, QObject* parent = nullptr)
        : QObject(parent)
        , d_document(document) {}

    std::optional<int> findMatchingBracket(int pos) const;

private:
    QTextDocument* d_document;
};

}
