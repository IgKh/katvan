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

#include <QList>
#include <QStringView>

#include <concepts>
#include <functional>

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
    TokenStream(QStringView text): d_tokenizer(text) {}

    bool atEnd();
    Token fetchToken();
    void returnToken(const Token& token);
    void returnTokens(QList<Token>& tokens);

private:
    Tokenizer d_tokenizer;
    QList<Token> d_tokenQueue;
};

template <typename M>
concept Matcher = requires(const M m, TokenStream& stream, QList<Token>& usedTokens) {
    { m.tryMatch(stream, usedTokens) } -> std::same_as<bool>;
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
        MATH_EXPRESSION_CHAIN,
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
};

using ParserStateStack = QList<ParserState>;

class ParsingListener
{
public:
    virtual ~ParsingListener() = default;

    virtual void initializeState(const ParserState& state, size_t endMarker) { Q_UNUSED(state); Q_UNUSED(endMarker); }
    virtual void finalizeState(const ParserState& state, size_t endMarker) { Q_UNUSED(state); Q_UNUSED(endMarker); }
    virtual void handleLooseToken(const Token& t, const ParserState& state) { Q_UNUSED(t); Q_UNUSED(state); }
};

class Parser
{
public:
    Parser(QStringView text, const ParserStateStack* initialState = nullptr);

    ParserStateStack stateStack() const { return d_stateStack; }
    void addListener(ParsingListener& listener);

    void parse();

private:
    bool handleCommentStart();
    bool handleCodeStart();

    template <Matcher M>
    bool match(const M& matcher)
    {
        QList<Token> usedTokens;
        if (!matcher.tryMatch(d_tokenStream, usedTokens)) {
            d_tokenStream.returnTokens(usedTokens);
            return false;
        }

        updateMarkers(usedTokens);
        return true;
    }

    void updateMarkers(const QList<Token>& tokens);
    void updateMarkers(const Token& token);

    void instantState(ParserState::Kind stateKind);
    void pushState(ParserState::Kind stateKind);
    void popState();

    QStringView d_text;
    TokenStream d_tokenStream;
    QList<std::reference_wrapper<ParsingListener>> d_listeners;
    ParserStateStack d_stateStack;

    bool d_enteredContentBlock;
    size_t d_startMarker;
    size_t d_endMarker;
};

struct HiglightingMarker
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

    bool operator==(const HiglightingMarker&) const = default;
};

/**
 * Listener that transforms parser events into syntax highlighting markers
 */
class HighlightingListener : public ParsingListener
{
public:
    QList<HiglightingMarker> markers() const { return d_markers; }

    void initializeState(const ParserState& state, size_t endMarker) override;
    void finalizeState(const ParserState& state, size_t endMarker) override;
    void handleLooseToken(const Token& t, const ParserState& state) override;

private:
    QList<HiglightingMarker> d_markers;
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

}
