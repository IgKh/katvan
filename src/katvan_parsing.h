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

namespace katvan::parsing {

enum class TokenType
{
    INVALID,
    WORD,
    CODE_NUMBER,
    SYMBOL,
    WHITESAPCE,
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
    Tokenizer(QStringView text): d_text(text), d_pos(0) {}

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
    size_t d_pos;
};

struct HiglightingMarker
{
    enum class Kind {
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

    bool atEnd();
    Token fetchToken();
    void returnToken(const Token& token);
    void returnTokens(QList<Token>& tokens);
    void setMarkers(const Token& start, const Token& end);

    bool matchSymbol(QChar symbol);
    bool matchSymbolSequence(QStringView symbols);
    bool matchTokenType(TokenType type);

    void pushState(ParserState::Kind stateKind);
    void popState(QList<HiglightingMarker>& markers);
    void finalizeState(const ParserState& state, QList<HiglightingMarker>& markers);

    QStringView d_text;
    Tokenizer d_tokenizer;
    ParserStateStack d_stateStack;
    QList<Token> d_tokenQueue;

    size_t d_startMarker;
    size_t d_endMarker;
};

}
