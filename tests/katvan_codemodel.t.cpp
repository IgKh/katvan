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
#include "katvan_testutils.h"

#include "katvan_codemodel.h"
#include "katvan_editortheme.h"
#include "katvan_highlighter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace katvan;

static std::unique_ptr<QTextDocument> buildDocument(const QStringList& lines)
{
    auto doc = std::make_unique<QTextDocument>();

    QTextCursor cursor(doc.get());
    for (const QString& line : lines) {
        cursor.insertText(line);
        cursor.insertBlock();
    }

    Highlighter highlighter(doc.get(), nullptr, EditorTheme::defaultTheme());
    highlighter.rehighlight();

    return doc;
}

TEST(CodeModelTests, FindMatchingBracket_Simple) {
    auto doc = buildDocument({
        QStringLiteral("#align(center, canvas({"),              // 0 - 23
        QStringLiteral("    plot.plot( "),                      // 24 - 39
        QStringLiteral("        size: (10, 5),"),               // 40 - 62
        QStringLiteral("        x-label: $C + sqrt(i)$,"),      // 63 - 93
        QStringLiteral("        y-grid: \"both()\","),          // 94 - 120
        QStringLiteral("        ["),                            // 121 - 130
        QStringLiteral("            _foo_ (bar)"),              // 131 - 154
        QStringLiteral("        ]"),                            // 155 - 164
        QStringLiteral("    )"),                                // 165 - 170
        QStringLiteral("}))")                                   // 171 - 174
    });

    CodeModel model(doc.get());
    std::optional<int> res;

    res = model.findMatchingBracket(5000);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(-1);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(0);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    // Code round brackets, multiline
    res = model.findMatchingBracket(6);
    EXPECT_THAT(res, ::testing::Eq(173));

    res = model.findMatchingBracket(173);
    EXPECT_THAT(res, ::testing::Eq(6));

    res = model.findMatchingBracket(21);
    EXPECT_THAT(res, ::testing::Eq(172));

    res = model.findMatchingBracket(172);
    EXPECT_THAT(res, ::testing::Eq(21));

    // Code block, multiline
    res = model.findMatchingBracket(22);
    EXPECT_THAT(res, ::testing::Eq(171));

    res = model.findMatchingBracket(171);
    EXPECT_THAT(res, ::testing::Eq(22));

    // Code round brackets, same line
    res = model.findMatchingBracket(54);
    EXPECT_THAT(res, ::testing::Eq(60));

    res = model.findMatchingBracket(60);
    EXPECT_THAT(res, ::testing::Eq(54));

    // Math delimiters
    res = model.findMatchingBracket(80);
    EXPECT_THAT(res, ::testing::Eq(92));

    res = model.findMatchingBracket(92);
    EXPECT_THAT(res, ::testing::Eq(80));

    // Function params in math mode
    res = model.findMatchingBracket(89);
    EXPECT_THAT(res, ::testing::Eq(91));

    res = model.findMatchingBracket(91);
    EXPECT_THAT(res, ::testing::Eq(89));

    // Round brackets inside a string literal
    res = model.findMatchingBracket(116);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(117);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    // Content block delimiters
    res = model.findMatchingBracket(129);
    EXPECT_THAT(res, ::testing::Eq(163));

    res = model.findMatchingBracket(163);
    EXPECT_THAT(res, ::testing::Eq(129));

    // Round brackets in content mode
    res = model.findMatchingBracket(149);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(153);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));
}

TEST(CodeModelTests, FindMatchingBracket_CodeExpression) {
    auto doc = buildDocument({
        QStringLiteral("#{"),                   // 0 - 2
        QStringLiteral("    let x = 2"),        // 3 - 16
        QStringLiteral("}")                     // 17 - 18
    });

    CodeModel model(doc.get());
    std::optional<int> res;

    res = model.findMatchingBracket(0);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(1);
    EXPECT_THAT(res, ::testing::Eq(17));

    res = model.findMatchingBracket(17);
    EXPECT_THAT(res, ::testing::Eq(1));
}

TEST(CodeModelTests, FindMatchingBracket_MathBrackets) {
    auto doc = buildDocument({
        QStringLiteral("$ (ln(2) + (7) $")
    });

    CodeModel model(doc.get());
    std::optional<int> res;

    res = model.findMatchingBracket(0);
    EXPECT_THAT(res, ::testing::Eq(15));

    res = model.findMatchingBracket(15);
    EXPECT_THAT(res, ::testing::Eq(0));

    res = model.findMatchingBracket(2);
    EXPECT_THAT(res, ::testing::Eq(15));

    res = model.findMatchingBracket(5);
    EXPECT_THAT(res, ::testing::Eq(7));

    res = model.findMatchingBracket(7);
    EXPECT_THAT(res, ::testing::Eq(5));

    res = model.findMatchingBracket(11);
    EXPECT_THAT(res, ::testing::Eq(13));

    res = model.findMatchingBracket(13);
    EXPECT_THAT(res, ::testing::Eq(11));
}

TEST(CodeModelTests, ShouldIncreaseIndent)
{
    auto doc = buildDocument({
        "#if 5 > 2 { pagebreak()",      // 0  - 23
        "table(",                       // 24 - 30
        "..nums.map(n => $F_#n$) + 1",  // 31 - 58
        "[Final]) }"                    // 59 - 69
    });

    CodeModel model(doc.get());

    EXPECT_FALSE(model.shouldIncreaseIndent(5000));
    EXPECT_FALSE(model.shouldIncreaseIndent(-1));

    EXPECT_FALSE(model.shouldIncreaseIndent(0));
    EXPECT_FALSE(model.shouldIncreaseIndent(9));
    EXPECT_FALSE(model.shouldIncreaseIndent(10));
    EXPECT_TRUE (model.shouldIncreaseIndent(11));
    EXPECT_TRUE (model.shouldIncreaseIndent(22));
    EXPECT_TRUE (model.shouldIncreaseIndent(23));

    EXPECT_FALSE(model.shouldIncreaseIndent(24));
    EXPECT_FALSE(model.shouldIncreaseIndent(29));
    EXPECT_TRUE (model.shouldIncreaseIndent(30));

    EXPECT_FALSE(model.shouldIncreaseIndent(31));
    EXPECT_FALSE(model.shouldIncreaseIndent(41));
    EXPECT_TRUE (model.shouldIncreaseIndent(42));
    EXPECT_TRUE (model.shouldIncreaseIndent(47));
    EXPECT_FALSE(model.shouldIncreaseIndent(52));
    EXPECT_TRUE (model.shouldIncreaseIndent(53));
    EXPECT_FALSE(model.shouldIncreaseIndent(54));
    EXPECT_FALSE(model.shouldIncreaseIndent(58));

    EXPECT_FALSE(model.shouldIncreaseIndent(59));
    EXPECT_TRUE (model.shouldIncreaseIndent(60));
    EXPECT_TRUE (model.shouldIncreaseIndent(65));
    EXPECT_FALSE(model.shouldIncreaseIndent(66));
}

TEST(CodeModelTests, FindMatchingIndentBlock)
{
    auto doc = buildDocument({
        "#if 5 > 2 { pagebreak() }",        // 0  - 25
        "#while 1 < 2 [",                   // 26 - 40
        "foo ]"                             // 41 - 46
    });

    CodeModel model(doc.get());
    QTextBlock res;

    res = model.findMatchingIndentBlock(10000);
    EXPECT_FALSE(res.isValid());

    res = model.findMatchingIndentBlock(-1);
    EXPECT_FALSE(res.isValid());

    res = model.findMatchingIndentBlock(0);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(12);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(24);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(26);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));

    res = model.findMatchingIndentBlock(39);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));

    res = model.findMatchingIndentBlock(41);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(2));

    res = model.findMatchingIndentBlock(45);
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));
}
