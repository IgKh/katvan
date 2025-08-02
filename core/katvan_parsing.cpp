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

/*
    Known gaps in syntax highlighting:
    - Special highlighting on show expression selectors
*/

#include "katvan_parsing_matchers.h"
#include "katvan_parsing.h"

namespace katvan::parsing {

Q_GLOBAL_STATIC(QSet<QString>, CODE_KEYWORDS, {
    QStringLiteral("and"),
    QStringLiteral("as"),
    QStringLiteral("auto"),
    QStringLiteral("break"),
    QStringLiteral("context"),
    QStringLiteral("else"),
    QStringLiteral("false"),
    QStringLiteral("for"),
    QStringLiteral("if"),
    QStringLiteral("import"),
    QStringLiteral("in"),
    QStringLiteral("include"),
    QStringLiteral("let"),
    QStringLiteral("none"),
    QStringLiteral("not"),
    QStringLiteral("or"),
    QStringLiteral("return"),
    QStringLiteral("set"),
    QStringLiteral("show"),
    QStringLiteral("true"),
    QStringLiteral("while")
})

Q_GLOBAL_STATIC(QSet<QString>, URL_PROTOCOLS, {
    QStringLiteral("http"),
    QStringLiteral("https")
})

static constexpr QLatin1StringView MATH_NON_OPERATORS = QLatin1StringView("()[]{},;");

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
    if (d_pos < 0) {
        d_pos = 0;
        return { TokenType::BEGIN };
    }

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

    // readWord() also matches code mode identifiers, so we eat any underscores
    // and hyphens, as long as they are not the leading character
    while (!atEnd() && (d_text[d_pos].isLetterOrNumber()
                        || d_text[d_pos].isMark()
                        || d_text[d_pos] == QLatin1Char('_')
                        || d_text[d_pos] == QLatin1Char('-'))) {
        d_pos++;
        len++;
    }

    // No trailing underscores, though (actually, they are allowed in
    // identifiers, but catching it at this level messes up emphasis markers)
    while (len > 0 && d_text[start + len - 1] == QLatin1Char('_')) {
        d_pos--;
        len--;
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

    size_t mark = d_pos;

    // Special form of escape - Unicode codepoint escape, e.g \u{1f600}
    if (!atEnd() && d_text[d_pos] == QLatin1Char('u')) {
        if (!tryUnicodeEscape()) {
            d_pos = mark;
        }
    }

    d_pos++;
    return buildToken(TokenType::ESCAPE, start, d_pos - start);
}

bool Tokenizer::tryUnicodeEscape()
{
    d_pos++;
    if (atEnd() || d_text[d_pos] != QLatin1Char('{')) {
        return false;
    }

    d_pos++;
    if (atEnd() || !isAsciiHexDigit(d_text[d_pos])) {
        return false;
    }

    while (!atEnd() && isAsciiHexDigit(d_text[d_pos])) {
        d_pos++;
    }

    if (atEnd() || d_text[d_pos] != QLatin1Char('}')) {
        return false;
    }

    return true;
}

Token Tokenizer::readWhitespace()
{
    size_t start = d_pos;
    size_t len = 0;
    while (!atEnd() && isWhiteSpace(d_text[d_pos])) {
        d_pos++;
        len++;
    }

    return buildToken(TokenType::WHITESPACE, start, len);
}

Token Tokenizer::readLineEnd()
{
    size_t start = d_pos;
    if (d_text[d_pos] == QLatin1Char('\r')) {
        // Make \r\n a single token
        d_pos++;
        if (!atEnd() && d_text[d_pos] == QLatin1Char('\n')) {
            d_pos++;
            return buildToken(TokenType::LINE_END, start, 2);
        }
    }
    else {
        d_pos++;
    }
    return buildToken(TokenType::LINE_END, start, 1);
}

bool TokenStream::atEnd() const
{
    return d_tokenizer.atEnd() && d_pos == d_tokenQueue.size();
}

Token& TokenStream::fetchToken()
{
    // Since the parser backtracks *a lot*, constantly copying tokens in and out
    // of the token queue is inefficient. Instead consumed tokens remain in the
    // queue, and queue's position index demarcates the boundary between consumed
    // and available tokens.

    if (d_pos == d_tokenQueue.size()) {
        d_tokenQueue.push_back(d_tokenizer.nextToken());
    }
    return d_tokenQueue[d_pos++];
}

QStringView TokenStream::peekTokenText()
{
    if (d_pos == d_tokenQueue.size()) {
        d_tokenQueue.push_back(d_tokenizer.nextToken());
    }
    return d_tokenQueue[d_pos].text;
}

void TokenStream::rewindTo(size_t position)
{
    Q_ASSERT(position <= d_pos);
    d_pos = position;
}

std::span<Token> TokenStream::consumedTokens()
{
    return std::span(d_tokenQueue.begin(), d_tokenQueue.begin() + d_pos);
}

void TokenStream::releaseConsumedTokens()
{
    d_tokenQueue.erase(d_tokenQueue.begin(), d_tokenQueue.begin() + d_pos);
    d_pos = 0;
}

bool isContentHolderStateKind(ParserState::Kind state)
{
    // States that can have nested content states in them
    return state == ParserState::Kind::CONTENT
        || state == ParserState::Kind::CONTENT_BLOCK
        || state == ParserState::Kind::CONTENT_HEADING
        || state == ParserState::Kind::CONTENT_EMPHASIS
        || state == ParserState::Kind::CONTENT_STRONG_EMPHASIS;
}

bool isMathHolderStateKind(ParserState::Kind state)
{
    return state == ParserState::Kind::MATH
        || state == ParserState::Kind::MATH_ARGUMENTS;
}

bool isCodeHolderStateKind(ParserState::Kind state)
{
    return state == ParserState::Kind::CODE_BLOCK
        || state == ParserState::Kind::CODE_LINE
        || state == ParserState::Kind::CODE_ARGUMENTS;
}

bool isCodeStateKind(ParserState::Kind state)
{
    return isCodeHolderStateKind(state)
        || state == ParserState::Kind::CODE_VARIABLE_NAME
        || state == ParserState::Kind::CODE_FUNCTION_NAME
        || state == ParserState::Kind::CODE_NUMERIC_LITERAL
        || state == ParserState::Kind::CODE_KEYWORD
        || state == ParserState::Kind::CODE_EXPRESSION_CHAIN
        || state == ParserState::Kind::CODE_STRING_EXPRESSION;
}

static bool isBlockScopedState(const ParserState& state)
{
    return state.kind == ParserState::Kind::COMMENT_LINE
        || state.kind == ParserState::Kind::CONTENT_HEADING
        || state.kind == ParserState::Kind::CONTENT_URL
        || state.kind == ParserState::Kind::CODE_LINE;
}

static bool isContentHolderState(const ParserState& state)
{
    return isContentHolderStateKind(state.kind);
}

static bool isMathHolderState(const ParserState& state)
{
    return isMathHolderStateKind(state.kind);
}

static bool isCodeHolderState(const ParserState& state)
{
    return isCodeHolderStateKind(state.kind);
}

static bool isCodeState(const ParserState& state)
{
    return isCodeStateKind(state.kind);
}

Parser::Parser(QStringView text, const QList<ParserState::Kind>& initialStates)
    : d_text(text)
    , d_tokenStream(text)
    , d_atContentStart(false)
    , d_startMarker(0)
    , d_endMarker(0)
{
    d_stateStack.append(ParserState{ ParserState::Kind::CONTENT, 0, true });
    for (ParserState::Kind state : initialStates) {
        d_stateStack.append(ParserState{ state, 0, true });
    }
}

void Parser::addListener(ParsingListener& listener, bool finalizeOnEnd)
{
    d_listeners.append(std::ref(listener));
    if (finalizeOnEnd) {
        d_finalizingListeners.append(std::ref(listener));
    }
}

void Parser::parse()
{
    namespace m = matchers;

    while (!d_tokenStream.atEnd()) {
        ParserState state = d_stateStack.last();

        if (isBlockScopedState(state) && match(m::TokenType(TokenType::LINE_END))) {
            popState();
            if (isContentHolderState(d_stateStack.last())) {
                d_atContentStart = true;
            }
            continue;
        }

        if (isContentHolderState(state)) {
            bool atContentStart = d_atContentStart;
            d_atContentStart = false;

            if (handleCommentStart()) {
                continue;
            }
            else if (handleCodeStart()) {
                continue;
            }
            else if (state.kind != ParserState::Kind::CONTENT
                && state.kind != ParserState::Kind::CONTENT_BLOCK
                && match(m::All(
                    m::Any(m::TokenType(TokenType::BEGIN), m::TokenType(TokenType::LINE_END)),
                    m::ZeroOrMore(m::TokenType(TokenType::WHITESPACE)),
                    m::Any(m::TokenType(TokenType::TEXT_END), m::TokenType(TokenType::LINE_END))
                ))
            ) {
                // A content holder state is being broken by a paragraph break,
                // without seeing the end symbol for it.
                // XXX this is in principal an error condition, and we might
                // want to signal this somehow.
                popState();
                continue;
            }
            else if (state.kind == ParserState::Kind::CONTENT_BLOCK && match(m::Symbol(QLatin1Char(']')))) {
                popState();

                if (match(m::Symbol(QLatin1Char('[')))) {
                    // Another content block can immediately start
                    pushState(ParserState::Kind::CONTENT_BLOCK);
                }
                else if (!isCodeHolderState(d_stateStack.last()) && match(m::ExpressionChainContinuation())) {
                    // Resume expression chain on the return value of function
                    // the code block was an argument for
                    pushState(ParserState::Kind::CODE_EXPRESSION_CHAIN);
                }
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('$')))) {
                instantState(ParserState::Kind::MATH_DELIMITER);
                pushState(ParserState::Kind::MATH);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('_')))) {
                if (state.kind == ParserState::Kind::CONTENT_EMPHASIS) {
                    popState();
                }
                else {
                    pushState(ParserState::Kind::CONTENT_EMPHASIS);
                }
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('*')))) {
                if (state.kind == ParserState::Kind::CONTENT_STRONG_EMPHASIS) {
                    popState();
                }
                else {
                    pushState(ParserState::Kind::CONTENT_STRONG_EMPHASIS);
                }
                continue;
            }
            else if (match(m::All(
                m::Keyword(*URL_PROTOCOLS),
                m::SymbolSequence(QStringLiteral("://"))
            ))) {
                pushState(ParserState::Kind::CONTENT_URL);
                continue;
            }
            else if (match(m::SymbolSequence(QStringLiteral("```")))) {
                pushState(ParserState::Kind::CONTENT_RAW_BLOCK);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('`')))) {
                pushState(ParserState::Kind::CONTENT_RAW);
                continue;
            }
            else if (match(m::All(
                m::Symbol(QLatin1Char('<')),
                m::LabelName(),
                m::Symbol(QLatin1Char('>'))
            ))) {
                instantState(ParserState::Kind::CONTENT_LABEL);
                continue;
            }
            else if (match(m::All(
                m::Symbol(QLatin1Char('@')),
                m::LabelName()
            ))) {
                instantState(ParserState::Kind::CONTENT_REFERENCE);
                continue;
            }
            else if (match(m::All(
                m::Discard(
                    m::All(
                        m::LineStartAnchor(atContentStart),
                        m::ZeroOrMore(m::TokenType(TokenType::WHITESPACE))
                    )
                ),
                m::OneOrMore(m::Symbol(QLatin1Char('='))),
                m::TokenType(TokenType::WHITESPACE)
            ))) {
                pushState(ParserState::Kind::CONTENT_HEADING);
                continue;
            }
            else if (match(m::All(
                m::Discard(
                    m::All(
                        m::LineStartAnchor(atContentStart),
                        m::ZeroOrMore(m::TokenType(TokenType::WHITESPACE))
                    )
                ),
                m::Any(m::Symbol(QLatin1Char('-')), m::Symbol(QLatin1Char('+'))),
                m::OneOrMore(m::TokenType(TokenType::WHITESPACE))
            ))) {
                instantState(ParserState::Kind::CONTENT_LIST_ENTRY);
                d_atContentStart = true;
                continue;
            }
            else if (match(m::All(
                m::Discard(
                    m::All(
                        m::LineStartAnchor(atContentStart),
                        m::ZeroOrMore(m::TokenType(TokenType::WHITESPACE))
                    )
                ),
                m::Symbol(QLatin1Char('/')),
                m::OneOrMore(m::TokenType(TokenType::WHITESPACE))
            ))) {
                instantState(ParserState::Kind::CONTENT_LIST_ENTRY);
                if (match(m::All(m::TokenType(TokenType::WORD), m::Peek(m::Symbol(QLatin1Char(':')))))) {
                    instantState(ParserState::Kind::CONTENT_TERM);
                }
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::CONTENT_URL) {
            if (match(m::Peek(m::Any(
                    m::TokenType(TokenType::WHITESPACE),
                    m::Symbol(QLatin1Char(']')),
                    m::Symbol(QLatin1Char(')')),
                    m::Symbol(QLatin1Char('}'))
            )))) {
                popState();
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::CONTENT_RAW_BLOCK) {
            if (match(m::SymbolSequence(QStringLiteral("```")))) {
                popState();
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::CONTENT_RAW) {
            if (match(m::Symbol(QLatin1Char('`')))) {
                popState();
                continue;
            }
        }
        else if (isMathHolderState(state)) {
            if (handleCommentStart()) {
                continue;
            }
            else if (handleCodeStart()) {
                continue;
            }
            else if (state.kind == ParserState::Kind::MATH_ARGUMENTS && match(m::Symbol(QLatin1Char(')')))) {
                popState();
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('$')))) {
                instantState(ParserState::Kind::MATH_DELIMITER);

                while (d_stateStack.last().kind == ParserState::Kind::MATH_ARGUMENTS) {
                    popState();
                }
                Q_ASSERT(d_stateStack.last().kind == ParserState::Kind::MATH);

                popState();
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('"')))) {
                pushState(ParserState::Kind::STRING_LITERAL);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('(')))) {
                pushState(ParserState::Kind::MATH_ARGUMENTS);
                continue;
            }
            else if (match(m::All(m::FullWord(), m::Peek(m::Symbol(QLatin1Char('(')))))) {
                if (d_endMarker > d_startMarker) {
                    instantState(ParserState::Kind::MATH_FUNCTION_NAME);
                }
                continue;
            }
            else if (match(m::FullWord())) {
                if (d_endMarker > d_startMarker) {
                    instantState(ParserState::Kind::MATH_SYMBOL_NAME);
                    if (match(m::ExpressionChainContinuation())) {
                        pushState(ParserState::Kind::MATH_EXPRESSION_CHAIN);
                    }
                }
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::MATH_EXPRESSION_CHAIN) {
            if (match(m::All(m::FullWord(), m::Peek(m::Symbol(QLatin1Char('(')))))) {
                instantState(ParserState::Kind::MATH_FUNCTION_NAME);
                popState();
                continue;
            }
            else if (match(m::FullWord())) {
                instantState(ParserState::Kind::MATH_SYMBOL_NAME);
                if (!match(m::ExpressionChainContinuation())) {
                    popState();
                }
                continue;
            }

            // Everything else breaks the chain!
            popState();
            continue;
        }
        else if (isCodeHolderState(state)) {
            if (handleCommentStart()) {
                continue;
            }
            else if (state.kind == ParserState::Kind::CODE_BLOCK && match(m::Symbol(QLatin1Char('}')))) {
                popState();
                continue;
            }
            else if (state.kind != ParserState::Kind::CODE_BLOCK && match(m::Symbol(QLatin1Char(';')))) {
                popState();
                continue;
            }
            else if (state.kind == ParserState::Kind::CODE_ARGUMENTS && match(m::Symbol(QLatin1Char(')')))) {
                popState();

                if (match(m::Symbol(QLatin1Char('[')))) {
                    // A content block argument may start _immediately_ after the
                    // normal argument list of a function call expression.
                    pushState(ParserState::Kind::CONTENT_BLOCK);
                }
                else if (!isCodeHolderState(d_stateStack.last()) && match(m::ExpressionChainContinuation())) {
                    // Resume expression chain, now on the return value
                    pushState(ParserState::Kind::CODE_EXPRESSION_CHAIN);
                }
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('{')))) {
                pushState(ParserState::Kind::CODE_BLOCK);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('(')))) {
                pushState(ParserState::Kind::CODE_ARGUMENTS);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('[')))) {
                pushState(ParserState::Kind::CONTENT_BLOCK);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('"')))) {
                pushState(ParserState::Kind::STRING_LITERAL);
                continue;
            }
            else if (match(m::SymbolSequence(QStringLiteral("```")))) {
                pushState(ParserState::Kind::CONTENT_RAW_BLOCK);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('`')))) {
                pushState(ParserState::Kind::CONTENT_RAW);
                continue;
            }
            else if (match(m::Keyword(*CODE_KEYWORDS))) {
                instantState(ParserState::Kind::CODE_KEYWORD);
                continue;
            }
            else if (match(m::All(
                m::CodeIdentifier(),
                m::Peek(m::Any(m::Symbol(QLatin1Char('(')), m::Symbol(QLatin1Char('['))))
            ))) {
                instantState(ParserState::Kind::CODE_FUNCTION_NAME);
                continue;
            }
            else if (match(m::FullCodeNumber())) {
                instantState(ParserState::Kind::CODE_NUMERIC_LITERAL);
                continue;
            }
            else if (match(m::Symbol(QLatin1Char('$')))) {
                instantState(ParserState::Kind::MATH_DELIMITER);
                pushState(ParserState::Kind::MATH);
                continue;
            }
            else if (match(m::All(
                m::Symbol(QLatin1Char('<')),
                m::LabelName(),
                m::Symbol(QLatin1Char('>'))
            ))) {
                instantState(ParserState::Kind::CONTENT_LABEL);
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::CODE_EXPRESSION_CHAIN) {
            if (match(m::All(
                m::CodeIdentifier(),
                m::Peek(m::Any(m::Symbol(QLatin1Char('(')), m::Symbol(QLatin1Char('['))))
            ))) {
                instantState(ParserState::Kind::CODE_FUNCTION_NAME);
                popState();
                if (match(m::Symbol(QLatin1Char('(')))) {
                    pushState(ParserState::Kind::CODE_ARGUMENTS);
                }
                else if (match(m::Symbol(QLatin1Char('[')))) {
                    pushState(ParserState::Kind::CONTENT_BLOCK);
                }
                continue;
            }
            else if (match(m::CodeIdentifier())) {
                instantState(ParserState::Kind::CODE_VARIABLE_NAME);
                if (!match(m::ExpressionChainContinuation())) {
                    popState();
                }
                continue;
            }

            // Everything else breaks the chain!
            popState();
            continue;
        }
        else if (state.kind == ParserState::Kind::COMMENT_BLOCK) {
            if (match(m::SymbolSequence(QStringLiteral("*/")))) {
                popState();
                continue;
            }
        }
        else if (state.kind == ParserState::Kind::STRING_LITERAL
                || state.kind == ParserState::Kind::CODE_STRING_EXPRESSION) {
            if (match(m::Symbol(QLatin1Char('"')))) {
                popState();

                if (state.kind == ParserState::Kind::CODE_STRING_EXPRESSION
                    && match(m::ExpressionChainContinuation()))
                {
                    // A method/field on a string literal in code mode - continue
                    // with a chain
                    pushState(ParserState::Kind::CODE_EXPRESSION_CHAIN);
                }
                continue;
            }
        }

        // In any other case - just burn a token and continue
        const Token& t = d_tokenStream.fetchToken();
        updateMarkers(t);
        for (auto& listener : d_listeners) {
            listener.get().handleLooseToken(t, state);
        }
        d_tokenStream.releaseConsumedTokens();
    }

    d_endMarker = d_text.size() - 1;

    // Clear the state stack - finalize open states only on listeners
    // that ask for it, but states that are automatically closed by
    // the text block ending are finalized on all listeners.
    bool removeBlockScoped = true;
    while (!d_stateStack.isEmpty()) {
        if (isBlockScopedState(d_stateStack.last()) && removeBlockScoped) {
            for (auto& listener : d_listeners) {
                listener.get().finalizeState(d_stateStack.last(), d_endMarker, true);
            }
        }
        else {
            removeBlockScoped = false;
            for (auto& listener : d_finalizingListeners) {
                listener.get().finalizeState(d_stateStack.last(), d_endMarker, true);
            }
        }
        d_stateStack.removeLast();
    }
}

bool Parser::handleCommentStart()
{
    namespace m = matchers;

    // Short circuit - if next token is not a "/", there is no chance it will
    // start a comment.
    if (d_tokenStream.peekTokenText() != QLatin1Char('/')) {
        return false;
    }

    if (match(m::SymbolSequence(QStringLiteral("//")))) {
        pushState(ParserState::Kind::COMMENT_LINE);
        return true;
    }
    if (match(m::SymbolSequence(QStringLiteral("/*")))) {
        pushState(ParserState::Kind::COMMENT_BLOCK);
        return true;
    }
    return false;
}

bool Parser::handleCodeStart()
{
    namespace m = matchers;

    // Short circuit - if next token is not a hash, there is no chance it will
    // start a code block.
    if (d_tokenStream.peekTokenText() != QLatin1Char('#')) {
        return false;
    }

    if (match(m::All(
        m::Symbol(QLatin1Char('#')),
        m::Keyword(*CODE_KEYWORDS)
    ))) {
        pushState(ParserState::Kind::CODE_LINE);
        return true;
    }
    if (match(m::All(
        m::Symbol(QLatin1Char('#')),
        m::CodeIdentifier(),
        m::Peek(m::Any(m::Symbol(QLatin1Char('(')), m::Symbol(QLatin1Char('['))))
    ))) {
        // Function call expression - followed by either a normal argument list
        // or a content block argument.
        instantState(ParserState::Kind::CODE_FUNCTION_NAME);
        if (match(m::Symbol(QLatin1Char('(')))) {
            pushState(ParserState::Kind::CODE_ARGUMENTS);
        }
        else if (match(m::Symbol(QLatin1Char('[')))) {
            pushState(ParserState::Kind::CONTENT_BLOCK);
        }
        return true;
    }
    if (match(m::All(
        m::Symbol(QLatin1Char('#')),
        m::FullCodeNumber()
    ))) {
        instantState(ParserState::Kind::CODE_NUMERIC_LITERAL);
        if (match(m::ExpressionChainContinuation())) {
            pushState(ParserState::Kind::CODE_EXPRESSION_CHAIN);
        }
        return true;
    }
    if (match(m::SymbolSequence(QStringLiteral("#\"")))) {
        pushState(ParserState::Kind::CODE_STRING_EXPRESSION);
        return true;
    }
    if (match(m::All(
        m::Symbol(QLatin1Char('#')),
        m::CodeIdentifier()
    ))) {
        instantState(ParserState::Kind::CODE_VARIABLE_NAME);
        if (match(m::ExpressionChainContinuation())) {
            pushState(ParserState::Kind::CODE_EXPRESSION_CHAIN);
        }
        return true;
    }
    if (match(m::All(
        m::Discard(m::Symbol(QLatin1Char('#'))),
        m::Symbol(QLatin1Char('{'))
    ))) {
        pushState(ParserState::Kind::CODE_BLOCK);
        return true;
    }
    if (match(m::All(
        m::Discard(m::Symbol(QLatin1Char('#'))),
        m::Symbol(QLatin1Char('['))
    ))) {
        pushState(ParserState::Kind::CONTENT_BLOCK);
        return true;
    }
    if (match(m::All(
        m::Discard(m::Symbol(QLatin1Char('#'))),
        m::Symbol(QLatin1Char('('))
    ))) {
        pushState(ParserState::Kind::CODE_ARGUMENTS);
        return true;
    }
    return false;
}

void Parser::updateMarkers(std::span<const Token> tokens)
{
    if (tokens.empty()) {
        return;
    }

    // Leading tokens that were marked by the "Discard" matcher are
    // not part of the match
    Token startToken;
    for (const Token& token : tokens) {
        if (!token.discard) {
            startToken = token;
            break;
        }
    }
    Q_ASSERT(startToken.type != TokenType::INVALID);

    d_startMarker = startToken.startPos;
    d_endMarker = tokens.back().startPos + tokens.back().length - 1;

    Q_ASSERT(d_startMarker <= d_endMarker);
}

void Parser::updateMarkers(const Token& token)
{
    if (!token.discard) {
        d_startMarker = token.startPos;
        d_endMarker = token.startPos + token.length - 1;
    }
}

void Parser::instantState(ParserState::Kind stateKind)
{
    ParserState state { stateKind, d_startMarker };
    for (auto& listener : d_listeners) {
        listener.get().handleInstantState(state, d_endMarker);
    }
}

void Parser::pushState(ParserState::Kind stateKind)
{
    if (stateKind == ParserState::Kind::CONTENT_BLOCK) {
        d_atContentStart = true;
    }

    d_stateStack.append(ParserState{ stateKind, d_startMarker });
    for (auto& listener : d_listeners) {
        listener.get().initializeState(d_stateStack.last(), d_endMarker);
    }
}

void Parser::popState(bool implicit)
{
    ParserState state = d_stateStack.takeLast();
    for (auto& listener : d_listeners) {
        listener.get().finalizeState(state, d_endMarker, implicit);
    }
}

void HighlightingListener::initializeState(const ParserState& state, size_t endMarker)
{
    size_t start = state.startPos;
    size_t length = endMarker - state.startPos + 1;

    switch (state.kind) {
    case ParserState::Kind::CODE_LINE:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::KEYWORD, start, length });
        break;
    }
}

void HighlightingListener::finalizeState(const ParserState& state, size_t endMarker, bool implicit)
{
    Q_UNUSED(implicit);

    size_t start = state.startPos;
    size_t length = endMarker - state.startPos + 1;

    switch (state.kind) {
    case ParserState::Kind::COMMENT_LINE:
    case ParserState::Kind::COMMENT_BLOCK:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::COMMENT, start, length });
        break;
    case ParserState::Kind::STRING_LITERAL:
    case ParserState::Kind::CODE_STRING_EXPRESSION:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL, start, length });
        break;
    case ParserState::Kind::MATH_DELIMITER:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER, endMarker, 1 });
        break;
    case ParserState::Kind::CONTENT_HEADING:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::HEADING, start, length });
        break;
    case ParserState::Kind::CONTENT_EMPHASIS:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS, start, length });
        break;
    case ParserState::Kind::CONTENT_STRONG_EMPHASIS:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, start, length });
        break;
    case ParserState::Kind::CONTENT_URL:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::URL, start, length });
        break;
    case ParserState::Kind::CONTENT_RAW_BLOCK:
    case ParserState::Kind::CONTENT_RAW:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::RAW, start, length });
        break;
    case ParserState::Kind::CONTENT_LABEL:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::LABEL, start, length });
        break;
    case ParserState::Kind::CONTENT_REFERENCE:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::REFERENCE, start, length });
        break;
    case ParserState::Kind::CONTENT_LIST_ENTRY:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, start, length });
        break;
    case ParserState::Kind::CONTENT_TERM:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::TERM, start, length });
        break;
    case ParserState::Kind::CODE_VARIABLE_NAME:
    case ParserState::Kind::MATH_SYMBOL_NAME:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME, start, length });
        break;
    case ParserState::Kind::CODE_FUNCTION_NAME:
    case ParserState::Kind::MATH_FUNCTION_NAME:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME, start, length });
        break;
    case ParserState::Kind::CODE_KEYWORD:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::KEYWORD, start, length });
        break;
    case ParserState::Kind::CODE_NUMERIC_LITERAL:
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL, start, length });
        break;
    }
}

void HighlightingListener::handleLooseToken(const Token& t, const ParserState& state)
{
    if (t.type == TokenType::ESCAPE && (isContentHolderState(state)
            || state.kind == ParserState::Kind::MATH
            || state.kind == ParserState::Kind::STRING_LITERAL)) {
        d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::ESCAPE, t.startPos, t.length });
    }
    else if (t.type == TokenType::SYMBOL && state.kind == ParserState::Kind::MATH) {
        if (!MATH_NON_OPERATORS.contains(t.text)) {
            d_markers.append(HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR, t.startPos, t.length });
        }
    }
}

void ContentWordsListener::handleLooseToken(const Token& t, const ParserState& state)
{
    if (!isContentHolderState(state)
        || t.type == TokenType::BEGIN
        || t.type == TokenType::TEXT_END) {
        return;
    }

    // Try to create the segments as long as possible, and include all real token
    // types (not just words, but also symbols, whitespace, etc). This is to provide
    // the word boundary detection algorithm that will run on natural text segments
    // later as much context to work with as possible.
    if (d_prevToken.type != TokenType::INVALID && t.startPos == d_prevToken.startPos + d_prevToken.length) {
        d_segments.last().length += t.length;
    }
    else {
        d_segments.append(ContentSegment{ t.startPos, t.length });
    }
    d_prevToken = t;
}

IsolatesListener::IsolatesListener()
{
    d_codeSequenceRangesForLevel.append(QList<qsizetype>());
}

IsolateRangeList IsolatesListener::isolateRanges() const
{
    IsolateRangeList result = d_ranges;

    auto it = std::remove_if(result.begin(), result.end(), [](const IsolateRange& range) {
        return range.discard;
    });
    result.erase(it, result.end());

    return result;
}

void IsolatesListener::initializeState(const ParserState& state, size_t endMarker)
{
    if (state.rolledOver) {
        return;
    }

    if (state.kind == ParserState::Kind::CONTENT_BLOCK || state.kind == ParserState::Kind::MATH) {
        d_codeSequenceRangesForLevel.append(QList<qsizetype>());
    }
    else if (state.kind == ParserState::Kind::CODE_EXPRESSION_CHAIN) {
        // For an expression chain, extend the current isolated code range on
        // seeing the "." - this papers over a peculiarity of the parser
        if (!d_codeSequenceRangesForLevel.last().isEmpty()) {
            auto& existingRange = d_ranges[d_codeSequenceRangesForLevel.last().last()];
            existingRange.endPos = endMarker;
        }
    }
}

void IsolatesListener::finalizeState(const ParserState& state, size_t endMarker, bool implicit)
{
    // What do we want to isolate the directionality of? Ideally short and
    // _continuous_ bits of text associated with a state that typically involves
    // characters with weak or no directionality. Math, inline code, content bits
    // marked by non-symmetric symbols (i.e references) and content blocks inline
    // with any of the above.

    if (state.rolledOver) {
        return;
    }

    if (state.kind == ParserState::Kind::CONTENT_BLOCK || state.kind == ParserState::Kind::MATH) {
        d_codeSequenceRangesForLevel.takeLast();
    }

    if (!implicit) {
        if (state.kind == ParserState::Kind::CONTENT_REFERENCE) {
            d_ranges.append(IsolateRange{ Qt::LayoutDirectionAuto, state.startPos, endMarker });
        }
        else if (state.kind == ParserState::Kind::CONTENT_BLOCK) {
            // Don't include the square brackets in the isolate
            d_ranges.append(IsolateRange{ Qt::LayoutDirectionAuto, state.startPos + 1, endMarker - 1 });
        }
        else if (state.kind == ParserState::Kind::MATH) {
            d_ranges.append(IsolateRange{ Qt::LeftToRight, state.startPos, endMarker });
        }
    }

    // Basically we want ANY code state to be considered so we have maximally
    // long isolate ranges; but if any of it is something we don't want to
    // isolate (code blocks, full lines, and parameter lists that spill to the
    // next line), we can discard the whole thing.
    if (isCodeState(state) || state.kind == ParserState::Kind::CONTENT_BLOCK) {
        IsolateRange* codeRange = createOrUpdateCodeRange(state.kind, state.startPos, endMarker);
        if (!codeRange) {
            return;
        }

        if (implicit || state.kind == ParserState::Kind::CODE_BLOCK || state.kind == ParserState::Kind::CODE_LINE) {
            codeRange->discard = true;
        }
    }
}

IsolateRange* IsolatesListener::createOrUpdateCodeRange(ParserState::Kind state, size_t startPos, size_t endPos)
{
    if (!d_codeSequenceRangesForLevel.last().isEmpty()) {
        auto& existingRange = d_ranges[d_codeSequenceRangesForLevel.last().last()];

        if (startPos == existingRange.endPos + 1) {
            // Extends existing isolated code range
            existingRange.endPos = endPos;
            return &existingRange;
        }
        else if (existingRange.startPos <= endPos && existingRange.endPos >= startPos) {
            // Intersects with existing isolated code range
            size_t origStartPos = existingRange.startPos;
            existingRange.startPos = qMin(existingRange.startPos, startPos);
            existingRange.endPos = qMax(existingRange.endPos, endPos);

            if (existingRange.startPos < origStartPos) {
                discardRedundantCodeRanges();
            }
            return &existingRange;
        }
    }

    // No existing isolated code range in this nesting level, or does not
    // intersect. Start a new one, but not for content blocks as they have
    // special handling in finalizeState.
    if (state != ParserState::Kind::CONTENT_BLOCK) {
        d_codeSequenceRangesForLevel.last().append(d_ranges.size());
        d_ranges.append(IsolateRange{ Qt::LeftToRight, startPos, endPos });
        return &d_ranges.last();
    }
    return nullptr;
}

void IsolatesListener::discardRedundantCodeRanges()
{
    auto& levelCodeRanges = d_codeSequenceRangesForLevel.last();
    Q_ASSERT(!levelCodeRanges.isEmpty());

    auto& reference = d_ranges[levelCodeRanges.last()];

    while (levelCodeRanges.size() > 1) {
        qsizetype i = levelCodeRanges.size() - 2;
        auto& candidate = d_ranges[levelCodeRanges[i]];

        if (reference.startPos == candidate.endPos + 1) {
            // Candidate and reference ranges are continuous
            reference.startPos = candidate.startPos;
            candidate.discard = true;
            levelCodeRanges.remove(i);
        }
        else if (reference.startPos <= candidate.endPos && reference.endPos >= candidate.endPos) {
            // Reference range contains/extends candidate range
            reference.startPos = qMin(reference.startPos, candidate.startPos);
            candidate.discard = true;
            levelCodeRanges.remove(i);
        }
        else {
            return;
        }
    }
}

}
