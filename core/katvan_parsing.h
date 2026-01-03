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

#include <QList>
#include <QStringView>

#include <concepts>
#include <functional>
#include <span>
#include <vector>

namespace katvan::parsing {

enum class TokenType
{
    INVALID,
    BEGIN,
    WORD,
    CODE_NUMBER,
    ESCAPE,
    SYMBOL,
    WHITESPACE,
    LINE_END,
    TEXT_END
};

struct Token
{
    TokenType type = TokenType::INVALID;
    size_t startPos = 0;
    size_t length = 0;
    QStringView text = QStringView();
    bool discard = false;
};

class Tokenizer
{
public:
    Tokenizer(QStringView text): d_text(text), d_pos(-1) {}

    bool atEnd() const { return d_pos >= d_text.size(); }
    Token nextToken();

private:
    Token buildToken(TokenType type, size_t startPos, size_t length);

    Token readWord();
    Token readCodeNumber();
    Token readSymbol();
    Token readPossibleEscape();
    Token readWhitespace();
    Token readLineEnd();

    bool tryUnicodeEscape();

    QStringView d_text;
    qsizetype d_pos;
};

class TokenStream
{
public:
    TokenStream(QStringView text): d_tokenizer(text), d_pos(0) {}

    bool atEnd() const;
    size_t position() const { return d_pos; }

    Token& fetchToken();
    QStringView peekTokenText();
    void rewindTo(size_t position);

    std::span<Token> consumedTokens();
    void releaseConsumedTokens();

private:
    Tokenizer d_tokenizer;
    std::vector<Token> d_tokenQueue;
    size_t d_pos;
};

template <typename M>
concept Matcher = requires(const M m, TokenStream& stream) {
    { m.tryMatch(stream) } -> std::same_as<bool>;
};

struct ParserState
{
    enum class Kind {
        INVALID,
        CONTENT,
        CONTENT_BLOCK,
        CONTENT_HEADING,
        CONTENT_EMPHASIS,
        CONTENT_STRONG_EMPHASIS,
        CONTENT_URL,
        CONTENT_RAW,
        CONTENT_RAW_BLOCK,
        CONTENT_LABEL,
        CONTENT_REFERENCE,
        CONTENT_LIST_ENTRY,
        CONTENT_TERM,
        MATH,
        MATH_DELIMITER,
        MATH_SYMBOL_NAME,
        MATH_FUNCTION_NAME,
        MATH_EXPRESSION_CHAIN,
        MATH_ARGUMENTS,
        CODE_VARIABLE_NAME,
        CODE_FUNCTION_NAME,
        CODE_NUMERIC_LITERAL,
        CODE_KEYWORD,
        CODE_LINE,
        CODE_BLOCK,
        CODE_ARGUMENTS,
        CODE_EXPRESSION_CHAIN,
        CODE_STRING_EXPRESSION,
        COMMENT_LINE,
        COMMENT_BLOCK,
        STRING_LITERAL,
    };

    Kind kind = Kind::INVALID;
    size_t startPos = 0;
    bool rolledOver = false; // State continues from previous parse
};

using ParserStateStack = QList<ParserState>;

bool isContentHolderStateKind(ParserState::Kind state);
bool isMathHolderStateKind(ParserState::Kind state);
bool isCodeHolderStateKind(ParserState::Kind state);
bool isCodeStateKind(ParserState::Kind state);

class ParsingListener
{
public:
    virtual ~ParsingListener() = default;

    virtual void initializeState(const ParserState& state, size_t endMarker) {
        Q_UNUSED(state);
        Q_UNUSED(endMarker);
    }

    virtual void finalizeState(const ParserState& state, size_t endMarker, bool implicit) {
        Q_UNUSED(state);
        Q_UNUSED(endMarker);
        Q_UNUSED(implicit);
    }

    virtual void handleInstantState(const ParserState& state, size_t endMarker) {
        finalizeState(state, endMarker, false);
    }

    virtual void handleLooseToken(const Token& t, const ParserState& state) {
        Q_UNUSED(t);
        Q_UNUSED(state);
    }
};

class Parser
{
public:
    Parser(QStringView text, const QList<ParserState::Kind>& initialStates = QList<ParserState::Kind>());

    void addListener(ParsingListener& listener, bool finalizeOnEnd);

    void parse();

private:
    bool handleCommentStart();
    bool handleCodeStart();

    template <Matcher M>
    bool match(const M& matcher) {
        size_t pos = d_tokenStream.position();
        if (!matcher.tryMatch(d_tokenStream)) {
            d_tokenStream.rewindTo(pos);
            return false;
        }

        updateMarkers(d_tokenStream.consumedTokens());
        d_tokenStream.releaseConsumedTokens();
        return true;
    }

    void updateMarkers(std::span<const Token> tokens);
    void updateMarkers(const Token& token);

    void instantState(ParserState::Kind stateKind);
    void pushState(ParserState::Kind stateKind);
    void popState(bool implicit = false);

    QStringView d_text;
    TokenStream d_tokenStream;
    QList<std::reference_wrapper<ParsingListener>> d_listeners;
    QList<std::reference_wrapper<ParsingListener>> d_finalizingListeners;
    ParserStateStack d_stateStack;

    bool d_atContentStart;
    size_t d_startMarker;
    size_t d_endMarker;
};

struct HighlightingMarker
{
    enum class Kind {
        HEADING,
        EMPHASIS,
        STRONG_EMPHASIS,
        URL,
        RAW,
        LABEL,
        REFERENCE,
        LIST_ENTRY,
        TERM,
        MATH_DELIMITER,
        MATH_OPERATOR,
        VARIABLE_NAME,
        FUNCTION_NAME,
        KEYWORD,
        ESCAPE,
        COMMENT,
        NUMBER_LITERAL,
        STRING_LITERAL
    };

    Kind kind;
    size_t startPos = 0;
    size_t length = 0;

    bool operator==(const HighlightingMarker&) const = default;
};

/**
 * Listener that transforms parser events into syntax highlighting markers
 */
class HighlightingListener : public ParsingListener
{
public:
    QList<HighlightingMarker> markers() const { return d_markers; }

    void initializeState(const ParserState& state, size_t endMarker) override;
    void finalizeState(const ParserState& state, size_t endMarker, bool implicit) override;
    void handleLooseToken(const Token& t, const ParserState& state) override;

private:
    QList<HighlightingMarker> d_markers;
};

struct ContentSegment
{
    size_t startPos = 0;
    size_t length = 0;

    bool operator==(const ContentSegment&) const = default;
};

using SegmentList = QList<ContentSegment>;

/**
 * Listener for extracting natural text from a Typst document
 */
class ContentWordsListener : public ParsingListener
{
public:
    SegmentList segments() const { return d_segments; }

    void handleLooseToken(const Token& t, const ParserState& state) override;

private:
    SegmentList d_segments;
    Token d_prevToken;
};

struct IsolateRange
{
    Qt::LayoutDirection dir;
    size_t startPos;
    size_t endPos;
    bool discard = false;

    bool operator==(const IsolateRange& other) const {
        return dir == other.dir && startPos == other.startPos && endPos == other.endPos;
    }
};

using IsolateRangeList = QList<IsolateRange>;

/**
 * Listener for determining text areas whose BiDi algorithm directionality
 * should be isolated.
 */
class IsolatesListener : public ParsingListener
{
public:
    IsolatesListener();

    IsolateRangeList isolateRanges() const;

    void initializeState(const ParserState& state, size_t endMarker) override;
    void finalizeState(const ParserState& state, size_t endMarker, bool implicit) override;

private:
    IsolateRange* createOrUpdateCodeRange(ParserState::Kind state, size_t startPos, size_t endPos);
    void discardRedundantCodeRanges();

    IsolateRangeList d_ranges;
    QList<QList<qsizetype>> d_codeSequenceRangesForLevel;
};

}
