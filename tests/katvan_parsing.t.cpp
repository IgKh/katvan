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
#include "katvan_parsing.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QString>

#include <iostream>

using namespace katvan::parsing;

namespace katvan::parsing {
    void PrintTo(const Token& token, std::ostream* os) {
        QByteArray tokenText = token.text.toUtf8();

        *os << "Token(" << static_cast<int>(token.type)
            << ", " << token.startPos << ", " << token.length
            << ", \"" << tokenText.data() << "\")";
    }

    void PrintTo(const HighlightingMarker& marker, std::ostream* os) {
        *os << "HighlightingMarker(" << static_cast<int>(marker.kind)
            << ", " << marker.startPos << ", " << marker.length << ")";
    }

    void PrintTo(const ContentSegment& segment, std::ostream* os) {
        *os << "ContentSegment(" << segment.startPos << ", " << segment.length << ")";
    }

    void PrintTo(const IsolateRange& range, std::ostream* os) {
        *os << "IsolateRange(" << range.dir << ", " << range.startPos << ", " << range.endPos << ")";
    }
}

struct TokenMatcher {
    TokenType type;
    QString text = QString();
};

bool operator==(const Token& t, const TokenMatcher& m) {
    return m.type == t.type && m.text == t.text;
}

void PrintTo(const TokenMatcher& m, std::ostream* os) {
    *os << "TokenMatcher(" << static_cast<int>(m.type)
        << ", \"" << m.text.toStdString() << "\")";
}

static std::vector<Token> tokenizeString(const QString& str)
{
    std::vector<Token> result;

    Tokenizer tok(str);
    while (!tok.atEnd()) {
        result.push_back(tok.nextToken());
    }
    return result;
}

TEST(TokenizerTests, Empty) {
    Tokenizer tok { QString() };

    ASSERT_FALSE(tok.atEnd());
    ASSERT_EQ(tok.nextToken(), (TokenMatcher{ TokenType::BEGIN }));
    ASSERT_TRUE(tok.atEnd());
    ASSERT_EQ(tok.nextToken(), (TokenMatcher{ TokenType::TEXT_END }));
    ASSERT_TRUE(tok.atEnd());
}

TEST(TokenizerTests, BasicSanity) {
    auto tokens = tokenizeString(QStringLiteral("a very basic test, with 10 words (or so!)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("very") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("b") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("asic") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("test") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral(",") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("with") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("10") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("words") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("(") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("o") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("r") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("so") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("!") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral(")") }
    }));
}

TEST(TokenizerTests, WhiteSpace) {
    auto tokens = tokenizeString(QStringLiteral(" A   B\tC  \t \nD\r\n\nE F"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral("   ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("B") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral("\t") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("C") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral("  \t ") },
        TokenMatcher{ TokenType::LINE_END,     QStringLiteral("\n") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("D") },
        TokenMatcher{ TokenType::LINE_END,     QStringLiteral("\r\n") },
        TokenMatcher{ TokenType::LINE_END,     QStringLiteral("\n") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("E") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("F") }
    }));
}

TEST(TokenizerTests, Escapes) {
    std::vector<Token> tokens;

    tokens = tokenizeString(QStringLiteral(R"(A \$ $\"'\'abc)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\$") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("$") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\"") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("'") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\'") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("abc") }
    }));

    tokens = tokenizeString(QStringLiteral(R"(\\\\\\\\\)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("\\") }
    }));

    tokens = tokenizeString(QStringLiteral(R"(\u{12e} \u{1f600} \\u{123})"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\u{12e}") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\u{1f600}") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::ESCAPE,       QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("u") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("{") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("123") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("}") }
    }));
}

TEST(TokenizerTests, Niqqud) {
    auto tokens = tokenizeString(QStringLiteral("שָׁלוֹם עוֹלָם 12"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("שָׁלוֹם") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("עוֹלָם") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("12") }
    }));
}

TEST(TokenizerTests, NotIdentifier) {
    auto tokens = tokenizeString(QStringLiteral("a _small_ thing"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("_") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("small") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("_") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("thing") }
    }));
}


TEST(TokenizerTests, Identifier) {
    auto tokens = tokenizeString(QStringLiteral("#let a_b3z = [$a$]"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("#") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("let") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a_b3z") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("=") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("[") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("$") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("$") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("]") }
    }));
}

TEST(TokenizerTests, MirroredSymbols) {
    auto tokens = tokenizeString(QStringLiteral("לפני [באמצע] אחרי"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("לפני") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("[") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("באמצע") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("]") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("אחרי") }
    }));
}

TEST(TokenizerTests, FullCodeNumber) {
    auto tokens = tokenizeString(QStringLiteral("A -12.4e-15em + 4e2B"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("-12.4e-15") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("em") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("+") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("4e2") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("B") }
    }));
}

TEST(TokenizerTests, HexCodeNumber) {
    auto tokens = tokenizeString(QStringLiteral("x10CAFE.b DEADBEEF xavier"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("x10CAFE.b") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("DEADBEEF") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("xa") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("vier") }
    }));
}

TEST(TokenizerTests, CodeNumberBacktracking) {
    auto tokens = tokenizeString(QStringLiteral("-b 12e-"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("-") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("b") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("12") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("e-") }
    }));
}

TEST(TokenizerTests, NonLatinNumerals) {
    auto tokens = tokenizeString(QStringLiteral("هناك ١٢ قطط"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("هناك") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("١٢") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("قطط") }
    }));
}

static QList<HighlightingMarker> highlightText(QStringView text)
{
    HighlightingListener listener;
    Parser parser(text);
    parser.addListener(listener, true);
    parser.parse();
    return listener.markers();
}

TEST(HighlightingParserTests, LineComment) {
    auto markers = highlightText(QStringLiteral("a // comment line\nb"));
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT, 2, 16 }
    ));
}

TEST(HighlightingParserTests, BlockComment) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("a /* comment\ncomment\ncomment*/ b"));
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT, 2, 28 }
    ));

    markers = highlightText(QStringLiteral("/* aaa\naaa // aaaaaaa */\naaa*/ aaaa"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT,          0, 24 },
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, 28,  7 }
    ));
}

TEST(HighlightingParserTests, StringLiteral) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("\"not a literal\" $ \"yesliteral\" + 1$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER, 16,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL, 18, 12 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,  31,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER, 34,  1 }

    ));

    markers = highlightText(QStringLiteral("$ \"A /* $ \" */ $"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  0, 1 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  2, 9 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,  12, 1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,  13, 1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER, 15, 1 }
    ));

    markers = highlightText(QStringLiteral("\"not a literal\" #foo(\"yesliteral\")"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  16,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL, 21, 12 }
    ));
}

TEST(HighlightingParserTests, Escapes) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("_\\$ \\_ foo _ \\ More: \"\\u{1f600}\""));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS,  0, 12 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,    1,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,    4,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,   22,  9 }
    ));

    markers = highlightText(QStringLiteral("$ \\u{12} + \"a\\nb\" $"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  0, 1 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,          2, 6 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,   9, 1 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL, 11, 6 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,         13, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER, 18, 1 }
    ));
}

TEST(HighlightingParserTests, Heading) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("=== this is a heading\nthis is not.\n \t= but this is"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::HEADING,  0, 22 },
        HighlightingMarker{ HighlightingMarker::Kind::HEADING, 37, 13 }
    ));

    markers = highlightText(QStringLiteral("a == not header\n=not header too"));
    EXPECT_THAT(markers, ::testing::IsEmpty());
}

TEST(HighlightingParserTests, Emphasis) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("a *bold* _underline_ and _*nested*_"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, 2, 6 },
        HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS, 9, 11 },
        HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS, 25, 10 },
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, 26, 8 }
    ));

    markers = highlightText(QStringLiteral("== for some reason, _emphasis\nextends_ headers"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::HEADING, 0, 46 },
        HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS, 20, 18 }
    ));

    markers = highlightText(QStringLiteral("*bold broken by paragraph break\n  \n*"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS,  0, 35 },
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, 35,  1 }
    ));
}

TEST(HighlightingParserTests, URL) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("This is from #footnote[https://foo.bar.com/there] here"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME, 13,  9 },
        HighlightingMarker{ HighlightingMarker::Kind::URL,           23, 25 }
    ));

    markers = highlightText(QStringLiteral("This is a url http://example.com but #link(\"https://this.isnt\")"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::URL,            14, 18 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  37,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL, 43, 19 }
    ));

    markers = highlightText(QStringLiteral("ssh://not.a.real.server"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT,         4, 19 }
    ));
}

TEST(HighlightingParserTests, RawContent) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("`` `some $raw$ with _emph_` `raw with\nnewline`"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::RAW, 0, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::RAW, 3, 24 },
        HighlightingMarker{ HighlightingMarker::Kind::RAW, 28, 18 }
    ));

    markers = highlightText(QStringLiteral("```some $raw$ with _emph_` ``` ```raw block with\nnewline```"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::RAW, 0, 30 },
        HighlightingMarker{ HighlightingMarker::Kind::RAW, 31, 28 }
    ));
}

TEST(HighlightingParserTests, ReferenceAndLabel) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("@ref123 foo <a_label> <not a label> //<also_not_label"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::REFERENCE, 0,   7 },
        HighlightingMarker{ HighlightingMarker::Kind::LABEL,     12,  9 },
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT,   36, 17 }
    ));

    markers = highlightText(QStringLiteral("<label_with_trailing_>\n@a_reference_with_trailing__"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LABEL,     0,  22 },
        HighlightingMarker{ HighlightingMarker::Kind::REFERENCE, 23, 28 }
    ));

    markers = highlightText(QStringLiteral("== The nature of @label\n_this is the <label>_"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::HEADING,   0 , 24 },
        HighlightingMarker{ HighlightingMarker::Kind::REFERENCE, 17,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::EMPHASIS,  24, 21 },
        HighlightingMarker{ HighlightingMarker::Kind::LABEL,     37,  7 }
    ));

    markers = highlightText(QStringLiteral("<ref.a_b-d> And @label.a_b-c E"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LABEL,     0 ,  11 },
        HighlightingMarker{ HighlightingMarker::Kind::REFERENCE, 16,  12 }
    ));
}

TEST(HighlightingParserTests, Lists) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("- A- this\n- this\n\t- that"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY,  0, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 10, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 18, 2 }
    ));

    markers = highlightText(QStringLiteral("+ B- this\n+this\n\t+ that"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY,  0, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 17, 2 }
    ));

    markers = highlightText(QStringLiteral("/ This: That\n/Not This: Not that\n/Neither This"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 0, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::TERM,       2, 4 }
    ));

    // Typst 0.13: nested lists and headers in list items
    markers = highlightText(QStringLiteral("- == this\n- - this\n- + that === but not this"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY,  0, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::HEADING,     2, 8 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 10, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 12, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 19, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY, 21, 2 }
    ));
}

TEST(HighlightingParserTests, CodeLineBreaks) {
    auto markers = highlightText(QStringLiteral(
        "#let a = 2\n"
        "while\n"
        "#let b = foo(); bar()"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         0,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  9,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,        17,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  26,  3 }
    ));
}

TEST(HighlightingParserTests, RawContentInCode) {
    auto markers = highlightText(QStringLiteral(
        "#par(\"foo\" + `bar` + ```baz\n"
        "  bong```"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    0,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,   5,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::RAW,             13,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::RAW,             21, 16 }
    ));
}

/*
 * Test cases taken from Typst documentation
 */

TEST(HighlightingParserTests, MathExpressions) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral("$x^2$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,    2,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   4,  1 }
    ));

    markers = highlightText(QStringLiteral("$x &= 2 \\ &= 3$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,    3,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,    4,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,    8,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,   10,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,   11,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  14,  1 }
    ));

    markers = highlightText(QStringLiteral("$#x$, $pi$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,    1,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   3,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   6,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,    7,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   9,  1 }
    ));

    markers = highlightText(QStringLiteral("$arrow.r.long$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,    1,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,    7,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,    9,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  13,  1 }
    ));

    markers = highlightText(QStringLiteral("$floor(x)$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    1,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   9,  1 }
    ));

    markers = highlightText(QStringLiteral("$#rect(width: 1cm) + 1$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    1,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  14,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,   19,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  22,  1 }
    ));

    markers = highlightText(QStringLiteral("$/* comment */$"));
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,   0,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::COMMENT,          1, 13 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  14,  1 }
    ));
}

TEST(HighlightingParserTests, SetRules) {
    QList<HighlightingMarker> markers;

    markers = highlightText(QStringLiteral(
        "#set heading(numbering: \"I.\")\n"
        "#set text(\n"
        "  font: \"New Computer Modern\"\n"
        ")\n\n"
        "= Introduction"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          0,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    5,  7 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  24,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         30,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   35,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  49, 21 },
        HighlightingMarker{ HighlightingMarker::Kind::HEADING,         74, 14 }
    ));

    markers = highlightText(QStringLiteral(
        "#let task(body, critical: false) = {\n"
        "  set text(red) if critical\n"
        "  [- #body]\n"
        "}\n\n"
        "#task(critical: true)[Food today?]\n"
        "#task(critical: false)[Work deadline]"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          0, 4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    5, 4 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         26, 5 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         39, 3 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   43, 4 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         53, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::LIST_ENTRY,      68, 2 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   70, 5 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   80, 5 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         96, 4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  115, 5 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,        131, 5 }
    ));
}

TEST(HighlightingParserTests, ShowRules) {
    auto markers = highlightText(QStringLiteral(
        "#show heading: it => [\n"
        "  #set align(center)\n"
        "  #set text(font: \"Inria Serif\")\n"
        "  \\~ #emph(it.body)\n"
        "      #counter(heading).display() \\~\n"
        "]"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          0,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         25,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   30,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         46,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   51,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  62, 13 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,          79,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   82,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  103,  8 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,  121,  7 },
        HighlightingMarker{ HighlightingMarker::Kind::ESCAPE,         131,  2 }
    ));
}

TEST(HighlightingParserTests, CodeExpressions) {
    auto markers = highlightText(QStringLiteral(
        "#emph[Hello] \\\n"
        "#emoji.face \\\n"
        "#\"hello\".len().a\n"
        "#(40em.abs.inches(), 12%)\n"
        "#40em.abs.inches()\n"
        "#this-and-that_"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    0,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   15,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   22,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  29,  8 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   38,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   44,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  48,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   57,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  67,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  72,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   78,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   82,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   91, 15 }
    ));
}

TEST(HighlightingParserTests, Blocks) {
    auto markers = highlightText(QStringLiteral(
        "#{\n"
        "let a = [from]\n"
        "let b = [*world*]\n"
        "[hello ]\n"
        "a + [ the ] + b\n"
        "}"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          3,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         18,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::STRONG_EMPHASIS, 27,  7 }
    ));
}

TEST(HighlightingParserTests, Loops) {
    auto markers = highlightText(QStringLiteral(
        "#for c in \"ABC\" [\n"
        "  #c is a letter.\n"
        "]\n\n"
        "#let n = 2\n"
        "#while n < 10 {\n"
        "  n = (n * 2) - 1\n"
        "}"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          0,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,          7,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::STRING_LITERAL,  10,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   20,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         39,  4 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  48,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::KEYWORD,         50,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  61,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  77,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::NUMBER_LITERAL,  82,  1 }
    ));
}

TEST(HighlightingParserTests, MathInCode) {
    auto markers = highlightText(QStringLiteral(
        "#align(center, table(\n"
        "  columns: count,\n"
        "  ..nums.map(n => $F_#n$),\n"
        "  ..nums.map(n => str(fib(n)),\n"
        "))"));

    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,    0,  6 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   15,  5 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   49,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  58,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_OPERATOR,   60,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::VARIABLE_NAME,   61,  2 },
        HighlightingMarker{ HighlightingMarker::Kind::MATH_DELIMITER,  63,  1 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   76,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   85,  3 },
        HighlightingMarker{ HighlightingMarker::Kind::FUNCTION_NAME,   89,  3 }
    ));
}

static QList<ContentSegment> extractContent(QStringView text)
{
    ContentWordsListener listener;
    Parser parser(text);
    parser.addListener(listener, true);
    parser.parse();
    return listener.segments();
}

TEST(ContentParserTests, Empty)
{
    auto segments = extractContent(QString());
    EXPECT_THAT(segments, ::testing::IsEmpty());
}

TEST(ContentParserTests, Sanity)
{
    auto segments = extractContent(QStringLiteral(
        "#for c in \"ABC\" [\n"
        "  #c is a letter.\n"
        "]\n\n"
        "// A comment\n"
        "= Some\theading \\#!\n"
        "_*Body* text_ 12 with some $\"math\" + 1$ in it."));

    EXPECT_THAT(segments, ::testing::ElementsAre(
        ContentSegment{  17,  3 }, // "\n  "
        ContentSegment{  22, 14 }, // " is a letter.\n"
        ContentSegment{  38,  1 }, // "\n"
        ContentSegment{  54, 16 }, // "Some\theading \#!"
        ContentSegment{  73,  4 }, // "Body"
        ContentSegment{  78,  5 }, // " text"
        ContentSegment{  84, 14 }, // " 12 with some "
        ContentSegment{ 110,  7 }  // " in it."
    ));
}

static IsolateRangeList extractIsolates(QStringView text)
{
    IsolatesListener listener;
    Parser parser(text);
    parser.addListener(listener, true);
    parser.parse();
    return listener.isolateRanges();
}

TEST(IsolatesListenerTests, Basic)
{
    auto isolates = extractIsolates(QStringLiteral(
        "Trying a @label and another @label in some $x + 1$ and #[content]"));

    EXPECT_THAT(isolates, ::testing::UnorderedElementsAre(
        IsolateRange { Qt::LayoutDirectionAuto,  9, 14 }, // @label
        IsolateRange { Qt::LayoutDirectionAuto, 28, 33 }, // @label
        IsolateRange { Qt::LeftToRight,         43, 49 }, // $x + 1$
        IsolateRange { Qt::LayoutDirectionAuto, 57, 63 }  // content
    ));
}

TEST(IsolatesListenerTests, Math)
{
    auto isolates = extractIsolates(QStringLiteral("$f(x) = x dot sin(pi/2 + x)$"));

    EXPECT_THAT(isolates, ::testing::ElementsAre(
        IsolateRange { Qt::LeftToRight, 0, 27 } // The whole thing
    ));
}

TEST(IsolatesListenerTests, CodeNumbers)
{
    auto isolates = extractIsolates(QStringLiteral("#par(leading: 1em, spacing: 2px, text: `foo`)"));

    EXPECT_THAT(isolates, ::testing::ElementsAre(
        IsolateRange { Qt::LeftToRight, 0, 44 } // The whole thing
    ));
}

TEST(IsolatesListenerTests, CodeLine)
{
    auto isolates = extractIsolates(QStringLiteral("#set text(lang: \"he\")"));

    EXPECT_THAT(isolates, ::testing::IsEmpty());
}

TEST(IsolatesListenerTests, FieldAccess)
{
    auto isolates = extractIsolates(QStringLiteral("Checking #test.test.test. Like that!"));

    EXPECT_THAT(isolates, ::testing::ElementsAre(
        IsolateRange { Qt::LeftToRight, 9, 23 } // "#test.test.test" (without final period)
    ));
}

TEST(IsolatesListenerTests, Nesting)
{
    auto isolates = extractIsolates(QStringLiteral(
        "#text(dir: ltr)[Size is #\"aa\".len and $#rect[A]$].fields()"));

    EXPECT_THAT(isolates, ::testing::UnorderedElementsAre(
        IsolateRange { Qt::LeftToRight,          0, 57 }, // Whole line is one long code expression
        IsolateRange { Qt::LayoutDirectionAuto, 16, 47 }, // The big content block argument to text()
        IsolateRange { Qt::LeftToRight,         24, 32 }, // #"aa".len
        IsolateRange { Qt::LeftToRight,         38, 47 }, // Math block
        IsolateRange { Qt::LeftToRight,         39, 46 }, // #rect[A]
        IsolateRange { Qt::LayoutDirectionAuto, 45, 45 }  // A
    ));
}
