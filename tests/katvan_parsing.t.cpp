#include "katvan_parsing.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QString>

#include <iostream>

using namespace katvan::parsing;

namespace katvan::parsing {
    void PrintTo(const Token& token, std::ostream* os) {
        *os << "Token(" << static_cast<int>(token.type)
            << ", " << token.startPos << ", " << token.length
            << ", \"" << token.text.toUtf8().data() << "\")";
    }

    void PrintTo(const HiglightingMarker& marker, std::ostream* os) {
        *os << "HiglightingMarker(" << static_cast<int>(marker.kind)
            << ", " << marker.startPos << ", " << marker.length << ")";
    }
}

struct TokenMatcher {
    TokenType type;
    QString text;
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

TEST(TokenizerTests, TestEmpty) {
    Tokenizer tok(QStringLiteral(""));

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
    auto tokens = tokenizeString(QStringLiteral(" A   B\tC  \t \nD\r\nE F"));
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
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\$") },
        TokenMatcher{ TokenType::WHITESPACE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("$") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\"") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("'") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\'") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("abc") }
    }));

    tokens = tokenizeString(QStringLiteral(R"(\\\\\\\\\)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::BEGIN },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\\") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("\\") }
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
        TokenMatcher{ TokenType::WORD,         QStringLiteral("e") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("-") }
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

TEST(HiglightingParserTests, LineComment) {
    Parser parser(QStringLiteral("a // comment line\nb"));
    auto markers = parser.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::COMMENT, 2, 16 }
    ));
}

TEST(HiglightingParserTests, BlockComment) {
    QList<HiglightingMarker> markers;

    Parser p1(QStringLiteral("a /* comment\ncomment\ncomment*/ b"));
    markers = p1.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::COMMENT, 2, 28 }
    ));

    Parser p2(QStringLiteral("/* aaa\naaa // aaaaaaa */\naaa*/ aaaa"));
    markers = p2.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::COMMENT, 11, 14 },
        HiglightingMarker{ HiglightingMarker::Kind::COMMENT, 0, 30 }
    ));
}

TEST(HiglightingParserTests, StringLiteral) {
    QList<HiglightingMarker> markers;

    Parser p1(QStringLiteral("\"not a literal\" $ \"yesliteral\" + 1$"));
    markers = p1.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::STRING_LITERAL, 18, 12 }
    ));

    Parser p2(QStringLiteral("$ \"A /* $ \" */ $"));
    markers = p2.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::ElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::STRING_LITERAL, 2, 9 }
    ));
}

TEST(HiglightingParserTests, Heading) {
    QList<HiglightingMarker> markers;

    Parser p1(QStringLiteral("=== this is a heading\nthis is not.\n \t= but this is"));
    markers = p1.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::UnorderedElementsAre(
        HiglightingMarker{ HiglightingMarker::Kind::HEADING, 0, 22 },
        HiglightingMarker{ HiglightingMarker::Kind::HEADING, 34, 16 }
    ));

    Parser p2(QStringLiteral("a == not header\n=not header too"));
    markers = p2.getHighlightingMarkers();
    EXPECT_THAT(markers, ::testing::IsEmpty());
}
