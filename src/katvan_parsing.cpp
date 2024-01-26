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
#include "katvan_parsing.h"

namespace katvan::parsing {

static bool isAsciiDigit(QChar ch)
{
    return ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
}

static bool isAsciiHexDigit(QChar ch)
{
    return isAsciiDigit(ch)
        || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))
        || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'));
}

static bool isBaseIndicator(QChar ch)
{
    return ch == QLatin1Char('b') || ch == QLatin1Char('o') || ch == QLatin1Char('x');
}

static bool isWhiteSpace(QChar ch)
{
    return ch.category() == QChar::Separator_Space || ch == QLatin1Char('\t');
}

static bool isLineEnd(QChar ch)
{
    return ch == QLatin1Char('\r')
        || ch == QLatin1Char('\n')
        || ch.category() == QChar::Separator_Line
        || ch.category() == QChar::Separator_Paragraph;
}

Token Tokenizer::nextToken()
{
    if (atEnd()) {
        return { TokenType::TEXT_END };
    }

    QChar ch = d_text[d_pos];
    if (isAsciiDigit(ch) || isBaseIndicator(ch) || ch == QLatin1Char('-') || ch == QLatin1Char('+')) {
        return readCodeNumber();
    }
    else if (ch.isLetterOrNumber()) {
        return readWord();
    }
    else if (ch == QLatin1Char('\\')) {
        return readPossibleEscape();
    }
    else if (isWhiteSpace(ch)) {
        return readWhitespace();
    }
    else if (isLineEnd(ch)) {
        return readLineEnd();
    }
    return readSymbol();
}

Token Tokenizer::buildToken(TokenType type, size_t startPos, size_t length)
{
    return { type, startPos, length, d_text.sliced(startPos, length) };
}

Token Tokenizer::readWord()
{
    size_t start = d_pos;
    size_t len = 0;

    // readWord() also matches code mode identifiers, so we eat any
    // underscores, as long as they are not the leading character
    while (!atEnd() && (d_text[d_pos].isLetterOrNumber()
                        || d_text[d_pos].isMark()
                        || d_text[d_pos] == QLatin1Char('_'))) {
        d_pos++;
        len++;
    }

    return buildToken(TokenType::WORD, start, len);
}

Token Tokenizer::readCodeNumber()
{
    size_t start = d_pos;
    size_t len = 0;

    bool readLeadingUnary = false;
    if (d_text[d_pos] == QLatin1Char('-') || d_text[d_pos] == QLatin1Char('+')) {
        d_pos++;
        if (atEnd() || (!isAsciiHexDigit(d_text[d_pos]) && !isBaseIndicator(d_text[d_pos]))) {
            return buildToken(TokenType::SYMBOL, start, 1);
        }
        else {
            readLeadingUnary = true;
            len++;
        }
    }

    bool isHexBase = false;
    auto isRelevantDigit = [&isHexBase](QChar ch) {
        return isHexBase ? isAsciiHexDigit(ch) : isAsciiDigit(ch);
    };

    if (!atEnd() && isBaseIndicator(d_text[d_pos])) {
        isHexBase = d_text[d_pos] == QLatin1Char('x');

        d_pos++;
        if (atEnd() || !isRelevantDigit(d_text[d_pos])) {
            if (readLeadingUnary) {
                d_pos--;
                return buildToken(TokenType::SYMBOL, start, 1);
            }
            else {
                return buildToken(TokenType::WORD, start, 1);
            }
        }
        else {
            len++;
        }
    }

    // Eat the digits for the integer part
    bool readIntegerPart = false;
    while (!atEnd() && isRelevantDigit(d_text[d_pos])) {
        readIntegerPart = true;
        d_pos++;
        len++;
    }

    if (!atEnd() && d_text[d_pos] == QLatin1Char('.') && readIntegerPart) {
        // Possible decimal point. Whether it is part of the number depends on
        // if the next char is a digit
        d_pos++;
        if (atEnd() || !isRelevantDigit(d_text[d_pos])) {
            d_pos--;
            return buildToken(TokenType::CODE_NUMBER, start, len);
        }
        else {
            len++;
        }
    }

    // Eat digits for the fraction part
    while (!atEnd() && isRelevantDigit(d_text[d_pos])) {
        d_pos++;
        len++;
    }

    // The only thing possibly remaining is an exponent. If there isn't one,
    // we are done
    if (atEnd()
        || !readIntegerPart
        || (d_text[d_pos] != QLatin1Char('e') && d_text[d_pos] != QLatin1Char('E'))) {
        return buildToken(TokenType::CODE_NUMBER, start, len);
    }
    Q_ASSERT(!isHexBase);

    size_t exponentStart = d_pos++;
    len++;

    // Eat exponent unary
    if (!atEnd() && (d_text[d_pos] == QLatin1Char('-') || d_text[d_pos] == QLatin1Char('+'))) {
        d_pos++;
        len++;
    }

    // Eat exponent digits
    bool readExponentDigits = false;
    while (!atEnd() && isAsciiDigit(d_text[d_pos])) {
        readExponentDigits = true;
        d_pos++;
        len++;
    }

    // Take the exponent part only if we read at least one digit
    if (!readExponentDigits) {
        d_pos = exponentStart;
        return buildToken(TokenType::CODE_NUMBER, start, exponentStart - start);
    }
    return buildToken(TokenType::CODE_NUMBER, start, len);
}

Token Tokenizer::readSymbol()
{
    return buildToken(TokenType::SYMBOL, d_pos++, 1);
}

Token Tokenizer::readPossibleEscape()
{
    // If we are at a '\' - it escapes the next char - unless
    // it is a whitespace / end (and then it is just a symbol)
    size_t start = d_pos++;
    if (atEnd() || isWhiteSpace(d_text[d_pos]) || isLineEnd(d_text[d_pos])) {
        return buildToken(TokenType::SYMBOL, start, 1);
    }

    d_pos++;
    return buildToken(TokenType::WORD, start, 2);
}

Token Tokenizer::readWhitespace()
{
    size_t start = d_pos;
    size_t len = 0;
    while (!atEnd() && isWhiteSpace(d_text[d_pos])) {
        d_pos++;
        len++;
    }

    return buildToken(TokenType::WHITESAPCE, start, len);
}

Token Tokenizer::readLineEnd()
{
    size_t start = d_pos;
    size_t len = 0;
    while (!atEnd() && isLineEnd(d_text[d_pos])) {
        d_pos++;
        len++;
    }

    return buildToken(TokenType::LINE_END, start, len);
}

Parser::Parser(QStringView text, const ParserStateStack* initialState)
    : d_text(text)
    , d_tokenizer(text)
    , d_stateStack(initialState != nullptr ? *initialState : ParserStateStack())
    , d_startMarker(0)
    , d_endMarker(0)
{
    if (d_stateStack.isEmpty()) {
        d_stateStack.append(ParserState{ ParserState::Kind::CONTENT, 0 });
    }
    else {
        for (ParserState& state : d_stateStack) {
            state.startPos = 0;
        }
    }
}

static bool isBlockScopedState(const ParserState& state)
{
    return state.kind == ParserState::Kind::COMMENT_LINE;
}

QList<HiglightingMarker> Parser::getHighlightingMarkers()
{
    QList<HiglightingMarker> result;

    while (!atEnd()) {
        ParserState state = d_stateStack.last();

        if (state.kind == ParserState::Kind::CONTENT) {
            if (handleCommentStart()) {
                continue;
            }
            else if (matchSymbol(QLatin1Char('$'))) {
                pushState(ParserState::Kind::MATH);
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::MATH) {
            if (handleCommentStart()) {
                continue;
            }
            if (matchSymbol(QLatin1Char('$'))) {
                popState(result);
                continue;
            }
            else if (matchSymbol(QLatin1Char('"'))) {
                pushState(ParserState::Kind::STRING_LITERAL);
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::COMMENT_LINE) {
            if (matchTokenType(TokenType::LINE_END)) {
                popState(result);
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::COMMENT_BLOCK) {
            if (matchSymbolSequence(QStringLiteral("*/"))) {
                popState(result);
                continue;
            }
            else if (matchSymbolSequence(QStringLiteral("//"))) {
                pushState(ParserState::Kind::COMMENT_LINE);
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::STRING_LITERAL) {
            if (matchSymbol(QLatin1Char('"'))) {
                popState(result);
                continue;
            }
        }

        // In any other case - just burn a token and continue
        fetchToken();
    }

    d_endMarker = d_text.size() - 1;
    for (const ParserState& state : d_stateStack) {
        finalizeState(state, result);
    }

    // Pop all states starting from the first block scoped one
    auto it = std::find_if(d_stateStack.begin(), d_stateStack.end(), isBlockScopedState);
    if (it != d_stateStack.end()) {
        d_stateStack.erase(it, d_stateStack.end());
    }

    return result;
}

bool Parser::handleCommentStart()
{
    if (matchSymbolSequence(QStringLiteral("//"))) {
        pushState(ParserState::Kind::COMMENT_LINE);
        return true;
    }
    if (matchSymbolSequence(QStringLiteral("/*"))) {
        pushState(ParserState::Kind::COMMENT_BLOCK);
        return true;
    }
    return false;
}

bool Parser::atEnd()
{
    return d_tokenizer.atEnd() && d_tokenQueue.isEmpty();
}

Token Parser::fetchToken()
{
    if (!d_tokenQueue.isEmpty()) {
        return d_tokenQueue.takeFirst();
    }
    return d_tokenizer.nextToken();
}

void Parser::returnToken(const Token& token)
{
    d_tokenQueue.prepend(token);
}

void Parser::returnTokens(QList<Token>& tokens)
{
    while (!tokens.isEmpty()) {
        d_tokenQueue.prepend(tokens.takeLast());
    }
}

void Parser::setMarkers(const Token& start, const Token& end)
{
    d_startMarker = start.startPos;
    d_endMarker = end.startPos + end.length - 1;
}

bool Parser::matchSymbol(QChar symbol)
{
    Token t = fetchToken();
    if (t.type == TokenType::SYMBOL && t.text == symbol) {
        setMarkers(t, t);
        return true;
    }

    returnToken(t);
    return false;
}

bool Parser::matchSymbolSequence(QStringView symbols)
{
    QList<Token> tokens;
    for (QChar ch : symbols) {
        Token t = fetchToken();
        tokens.append(t);

        if (t.type != TokenType::SYMBOL || t.text != ch) {
            returnTokens(tokens);
            return false;
        }
    }

    setMarkers(tokens.first(), tokens.last());
    return true;
}

bool Parser::matchTokenType(TokenType type)
{
    Token t = fetchToken();
    if (t.type == type) {
        setMarkers(t, t);
        return true;
    }

    returnToken(t);
    return false;
}

void Parser::pushState(ParserState::Kind stateKind)
{
    d_stateStack.append(ParserState{ stateKind, d_startMarker });
}

void Parser::popState(QList<HiglightingMarker>& markers)
{
    finalizeState(d_stateStack.takeLast(), markers);
}

void Parser::finalizeState(const ParserState& state, QList<HiglightingMarker>& markers)
{
    if (state.kind == ParserState::Kind::COMMENT_LINE || state.kind == ParserState::Kind::COMMENT_BLOCK) {
        markers.append(HiglightingMarker{ HiglightingMarker::Kind::COMMENT, state.startPos, d_endMarker - state.startPos + 1 });
    }
    else if (state.kind == ParserState::Kind::STRING_LITERAL) {
        markers.append(HiglightingMarker{ HiglightingMarker::Kind::STRING_LITERAL, state.startPos, d_endMarker - state.startPos + 1 });
    }
}

}
