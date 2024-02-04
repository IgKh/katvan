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

namespace katvan::parsing {

enum class TokenType
{
    INVALID,
    BEGIN,
    WORD,
    CODE_NUMBER,
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
    QStringView text;
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

    QStringView d_text;
    ssize_t d_pos;
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

struct HiglightingMarker
{
    enum class Kind {
        HEADING,
        EMPHASIS,
        STRONG_EMPHASIS,
        RAW,
        COMMENT,
        STRING_LITERAL
    };

    Kind kind;
    size_t startPos = 0;
    size_t length = 0;

    bool operator==(const HiglightingMarker&) const = default;
};

struct ParserState
{
    enum class Kind {
        INVALID,
        CONTENT,
        CONTENT_HEADING,
        CONTENT_EMPHASIS,
        CONTENT_STRONG_EMPHASIS,
        CONTENT_RAW,
        CONTENT_RAW_BLOCK,
        MATH,
        COMMENT_LINE,
        COMMENT_BLOCK,
        STRING_LITERAL,
    };

    Kind kind = Kind::INVALID;
    size_t startPos = 0;
};

using ParserStateStack = QList<ParserState>;

class Parser
{
public:
    Parser(QStringView text, const ParserStateStack* initialState = nullptr);

    ParserStateStack stateStack() const { return d_stateStack; }

    QList<HiglightingMarker> getHighlightingMarkers();

private:
    bool handleCommentStart();

    template <Matcher M>
    bool match(const M& matcher)
    {
        QList<Token> usedTokens;
        if (!matcher.tryMatch(d_tokenStream, usedTokens)) {
            d_tokenStream.returnTokens(usedTokens);
            return false;
        }

        Q_ASSERT(!usedTokens.isEmpty());
        d_startMarker = usedTokens.first().startPos;
        d_endMarker = usedTokens.last().startPos + usedTokens.last().length - 1;
        return true;
    }

    void pushState(ParserState::Kind stateKind);
    void popState(QList<HiglightingMarker>& markers);
    void finalizeState(const ParserState& state, QList<HiglightingMarker>& markers);

    QStringView d_text;
    TokenStream d_tokenStream;
    ParserStateStack d_stateStack;

    size_t d_startMarker;
    size_t d_endMarker;
};

}
