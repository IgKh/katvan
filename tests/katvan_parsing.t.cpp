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

    ASSERT_TRUE(tok.atEnd());
    ASSERT_EQ(tok.nextToken(), (TokenMatcher{ TokenType::TEXT_END }));
    ASSERT_TRUE(tok.atEnd());
}

TEST(TokenizerTests, BasicSanity) {
    auto tokens = tokenizeString(QStringLiteral("a very basic test, with 10 words (or so!)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("very") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("b") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("asic") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("test") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral(",") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("with") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("10") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("words") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("(") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("o") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("r") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("so") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("!") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral(")") }
    }));
}

TEST(TokenizerTests, WhiteSpace) {
    auto tokens = tokenizeString(QStringLiteral(" A   B\tC  \t \nD\r\nE F"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral("   ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("B") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral("\t") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("C") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral("  \t ") },
        TokenMatcher{ TokenType::LINE_END,     QStringLiteral("\n") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("D") },
        TokenMatcher{ TokenType::LINE_END,     QStringLiteral("\r\n") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("E") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("F") }
    }));
}

TEST(TokenizerTests, Escapes) {
    std::vector<Token> tokens;

    tokens = tokenizeString(QStringLiteral(R"(A \$ $\"'\'abc)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\$") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("$") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\\"") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("'") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("\\'") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("abc") }
    }));

    tokens = tokenizeString(QStringLiteral(R"(\\\\\\\\\)"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
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
        TokenMatcher{ TokenType::WORD,         QStringLiteral("שָׁלוֹם") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("עוֹלָם") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("12") }
    }));
}

TEST(TokenizerTests, Identifier) {
    auto tokens = tokenizeString(QStringLiteral("#let a_b3z = [$a$]"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("#") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("let") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("a_b3z") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("=") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
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
        TokenMatcher{ TokenType::WORD,         QStringLiteral("לפני") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("[") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("באמצע") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("]") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("אחרי") }
    }));
}

TEST(TokenizerTests, FullCodeNumber) {
    auto tokens = tokenizeString(QStringLiteral("A -12.4e-15em + 4e2B"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::WORD,         QStringLiteral("A") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("-12.4e-15") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("em") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("+") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("4e2") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("B") }
    }));
}

TEST(TokenizerTests, HexCodeNumber) {
    auto tokens = tokenizeString(QStringLiteral("x10CAFE.b DEADBEEF xavier"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("x10CAFE.b") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("DEADBEEF") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("xa") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("vier") }
    }));
}

TEST(TokenizerTests, CodeNumberBacktracking) {
    auto tokens = tokenizeString(QStringLiteral("-b 12e-"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("-") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("b") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::CODE_NUMBER,  QStringLiteral("12") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("e") },
        TokenMatcher{ TokenType::SYMBOL,       QStringLiteral("-") }
    }));
}

TEST(TokenizerTests, NonLatinNumerals) {
    auto tokens = tokenizeString(QStringLiteral("هناك ١٢ قطط"));
    EXPECT_THAT(tokens, ::testing::ElementsAreArray({
        TokenMatcher{ TokenType::WORD,         QStringLiteral("هناك") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
        TokenMatcher{ TokenType::WORD,         QStringLiteral("١٢") },
        TokenMatcher{ TokenType::WHITESAPCE,   QStringLiteral(" ") },
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
