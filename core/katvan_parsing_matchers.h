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

#include "katvan_parsing.h"

#include <QSet>

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

    bool tryMatch(TokenStream& stream) const
    {
        return detail::tupleForEach(d_matchers, [&](Matcher auto&& m) {
            return m.tryMatch(stream);
        });
    }
};

template <Matcher ...Matchers>
class Any
{
    std::tuple<Matchers...> d_matchers;

public:
    Any(Matchers ...matchers): d_matchers(matchers...) {}

    bool tryMatch(TokenStream& stream) const
    {
        return !detail::tupleForEach(d_matchers, [&](Matcher auto&& m) {
            size_t pos = stream.position();
            if (m.tryMatch(stream)) {
                return false;
            }
            else {
                stream.rewindTo(pos);
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

    bool tryMatch(TokenStream& stream) const
    {
        size_t pos = stream.position();
        bool ok = d_matcher.tryMatch(stream);
        if (!ok) {
            stream.rewindTo(pos);
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

    bool tryMatch(TokenStream& stream) const
    {
        if (!d_matcher.tryMatch(stream)) {
            return false;
        }
        while (true) {
            size_t pos = stream.position();
            if (!d_matcher.tryMatch(stream)) {
                stream.rewindTo(pos);
                break;
            }
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

    bool tryMatch(TokenStream& stream) const
    {
        size_t pos = stream.position();
        bool matched = d_matcher.tryMatch(stream);
        stream.rewindTo(pos);
        return matched;
    }
};

template <Matcher M>
class Discard
{
    M d_matcher;

public:
    Discard(const M& matcher): d_matcher(matcher) {}

    bool tryMatch(TokenStream& stream) const
    {
        bool matched = d_matcher.tryMatch(stream);
        if (matched) {
            for (Token& token : stream.consumedTokens()) {
                token.discard = true;
            }
        }
        return matched;
    }
};

class Condition
{
    bool d_condition;

public:
    Condition(bool condition): d_condition(condition) {}

    bool tryMatch(TokenStream&) const
    {
        return d_condition;
    }
};

class TokenType
{
    parsing::TokenType d_type;

public:
    TokenType(parsing::TokenType type): d_type(type) {}

    bool tryMatch(TokenStream& stream) const
    {
        const Token& t = stream.fetchToken();
        return t.type == d_type;
    }
};

class Symbol
{
    QChar d_symbol;

public:
    Symbol(QChar symbol): d_symbol(symbol) {}

    bool tryMatch(TokenStream& stream) const
    {
        const Token& t = stream.fetchToken();
        return t.type == parsing::TokenType::SYMBOL && t.text == d_symbol;
    }
};

class SymbolSequence
{
    QStringView d_symbols;

public:
    SymbolSequence(QStringView symbols): d_symbols(symbols) {}

    bool tryMatch(TokenStream& stream) const
    {
        for (QChar ch : d_symbols) {
            const Token& t = stream.fetchToken();

            if (t.type != parsing::TokenType::SYMBOL || t.text != ch) {
                return false;
            }
        }
        return true;
    }
};

// Because of our tokenizer design (which doesn't backtrack by itself) number
// base prefixes can cause an otherwise continuous word to be broken into
// multiple word tokens (e.g "break" -> "b" + "reak"). This covers up for it.
auto FullWord() {
    return OneOrMore(TokenType(parsing::TokenType::WORD));
}

auto CodeIdentifier() {
    return All(
        FullWord(),
        ZeroOrMore(Symbol(QLatin1Char('_')))
    );
}

// A number literal in code, with possible trailing units
auto FullCodeNumber() {
    return All(
        TokenType(parsing::TokenType::CODE_NUMBER),
        Optionally(Any(TokenType(parsing::TokenType::WORD), Symbol(QLatin1Char('%'))))
    );
}

// Start of content line
auto LineStartAnchor(bool atContentStart) {
    return Any(
        TokenType(parsing::TokenType::BEGIN),
        TokenType(parsing::TokenType::LINE_END),
        Condition(atContentStart)
    );
}

auto LabelName() {
    return OneOrMore(
        Any(
            TokenType(parsing::TokenType::WORD),
            TokenType(parsing::TokenType::CODE_NUMBER),
            Symbol(QLatin1Char('_')),
            Symbol(QLatin1Char('-')),
            Symbol(QLatin1Char('.'))
        )
    );
}

auto ExpressionChainContinuation() {
    return All(
        Symbol(QLatin1Char('.')),
        Peek(TokenType(parsing::TokenType::WORD))
    );
}

class Keyword
{
    const QSet<QString>& d_keywords;

public:
    Keyword(const QSet<QString>& keywords): d_keywords(keywords) {}

    bool tryMatch(TokenStream& stream) const
    {
        size_t startPos = stream.position();
        bool ok = FullWord().tryMatch(stream);
        if (!ok) {
            return false;
        }

        auto wordTokens = stream.consumedTokens().subspan(startPos);

        QString word;
        for (const Token& t : wordTokens) {
            word.append(t.text);
        }
        return d_keywords.contains(word);
    }
};

}
