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

#include <algorithm>

//
// Parser Combinator library for the Katvan Typst parser
//
namespace katvan::parsing::matchers {

namespace detail {
    template <size_t I = 0,
            typename Callback,
            typename... TTypes>
    bool tupleForEach(const std::tuple<TTypes...>& t, Callback&& cb)
    {
        if constexpr (I < sizeof...(TTypes)) {
            if (!cb(std::get<I>(t))) {
                return false;
            }
            return tupleForEach<I + 1>(t, std::forward<Callback>(cb));
        }
        else {
            Q_UNUSED(t);
            Q_UNUSED(cb);
            return true;
        }
    }
}

template <Matcher ...Matchers>
class All
{
    std::tuple<Matchers...> d_matchers;

public:
    All(Matchers ...matchers): d_matchers(matchers...) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        return detail::tupleForEach(d_matchers, [&](Matcher auto&& m) {
            return m.tryMatch(stream, usedTokens);
        });
    }
};

template <Matcher ...Matchers>
class Any
{
    std::tuple<Matchers...> d_matchers;

public:
    Any(Matchers ...matchers): d_matchers(matchers...) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        return !detail::tupleForEach(d_matchers, [&](Matcher auto&& m) {
            QList<Token> nested;
            if (m.tryMatch(stream, nested)) {
                usedTokens.append(nested);
                return false;
            }
            else {
                stream.returnTokens(nested);
                return true;
            }
        });
    }
};

template <Matcher M>
class Optionally
{
    M d_matcher;

public:
    Optionally(const M& matcher): d_matcher(matcher) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        QList<Token> nested;
        bool ok = d_matcher.tryMatch(stream, nested);
        if (ok) {
            usedTokens.append(nested);
        }
        else {
            stream.returnTokens(nested);
        }
        return true;
    }
};

template <Matcher M>
class OneOrMore
{
    M d_matcher;

public:
    OneOrMore(const M& matcher): d_matcher(matcher) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        if (!d_matcher.tryMatch(stream, usedTokens)) {
            return false;
        }
        while (true) {
            QList<Token> nested;
            if (!d_matcher.tryMatch(stream, nested)) {
                stream.returnTokens(nested);
                break;
            }
            usedTokens.append(nested);
        }
        return true;
    }
};

template <Matcher M>
auto ZeroOrMore(const M& matcher)
{
    return Optionally(OneOrMore(matcher));
}

template <Matcher M>
class Peek
{
    M d_matcher;

public:
    Peek(const M& matcher): d_matcher(matcher) {}

    bool tryMatch(TokenStream& stream, QList<Token>&) const
    {
        QList<Token> nested;
        bool matched = d_matcher.tryMatch(stream, nested);
        stream.returnTokens(nested);
        return matched;
    }
};

template <Matcher M>
class Discard
{
    M d_matcher;

public:
    Discard(const M& matcher): d_matcher(matcher) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        QList<Token> nested;
        bool matched = d_matcher.tryMatch(stream, nested);
        if (matched) {
            for (Token& token : nested) {
                token.discard = true;
            }
        }
        usedTokens.append(nested);
        return matched;
    }
};

class Condition
{
    bool d_condition;

public:
    Condition(bool condition): d_condition(condition) {}

    bool tryMatch(TokenStream&, QList<Token>&) const
    {
        return d_condition;
    }
};

class TokenType
{
    parsing::TokenType d_type;

public:
    TokenType(parsing::TokenType type): d_type(type) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        Token t = stream.fetchToken();
        usedTokens.append(t);
        return t.type == d_type;
    }
};

class Symbol
{
    QChar d_symbol;

public:
    Symbol(QChar symbol): d_symbol(symbol) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        Token t = stream.fetchToken();
        usedTokens.append(t);
        return t.type == parsing::TokenType::SYMBOL && t.text == d_symbol;
    }
};

class SymbolSequence
{
    QStringView d_symbols;

public:
    SymbolSequence(QStringView symbols): d_symbols(symbols) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        for (QChar ch : d_symbols) {
            Token t = stream.fetchToken();
            usedTokens.append(t);

            if (t.type != parsing::TokenType::SYMBOL || t.text != ch) {
                return false;
            }
        }
        return true;
    }
};

// Because of our tokenizer design (which doesn't backtrack by itself) number
// base prefixes can cause an otherwise continues word to be broken into
// multiple word tokens (e.g "break" -> "b" + "reak"). This covers up for it.
auto FullWord() {
    return OneOrMore(TokenType(parsing::TokenType::WORD));
}

// A number literal in code, with possible trailing units
auto FullCodeNumber() {
    return All(
        TokenType(parsing::TokenType::CODE_NUMBER),
        Optionally(Any(TokenType(parsing::TokenType::WORD), Symbol(QLatin1Char('%'))))
    );
}

// Start of content line
auto LineStartAnchor(bool enteredContentBlock) {
    return Any(
        TokenType(parsing::TokenType::BEGIN),
        TokenType(parsing::TokenType::LINE_END),
        Condition(enteredContentBlock)
    );
}

class Keyword
{
    const QStringList& d_keywords;

public:
    Keyword(const QStringList& keywords): d_keywords(keywords) {}

    bool tryMatch(TokenStream& stream, QList<Token>& usedTokens) const
    {
        QList<Token> nested;
        bool ok = FullWord().tryMatch(stream, nested);

        usedTokens.append(nested);
        if (!ok) {
            return false;
        }

        QString word;
        for (const Token& t : nested) {
            word.append(t.text);
        }
        return std::binary_search(d_keywords.begin(), d_keywords.end(), word);
    }
};

}
