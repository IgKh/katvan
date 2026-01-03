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
#include "katvan_testutils.h"

#include "katvan_codemodel.h"
#include "katvan_editortheme.h"
#include "katvan_highlighter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QGlobalStatic>

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

    Highlighter highlighter(doc.get(), nullptr, EditorTheme{});
    highlighter.rehighlight();

    return doc;
}

static int globalPos(const QTextDocument& doc, int blockNum, int posInBlock)
{
    return doc.findBlockByNumber(blockNum).position() + posInBlock;
}

TEST(CodeModelTests, FindMatchingBracket_Simple) {
    auto doc = buildDocument({
        /* 0 */ QStringLiteral("#align(center, canvas({"),
        /* 1 */ QStringLiteral("    plot.plot( "),
        /* 2 */ QStringLiteral("        size: (10, 5),"),
        /* 3 */ QStringLiteral("        x-label: $C + sqrt(i)$,"),
        /* 4 */ QStringLiteral("        y-grid: \"both()\","),
        /* 5 */ QStringLiteral("        ["),
        /* 6 */ QStringLiteral("            _foo_ (bar)"),
        /* 7 */ QStringLiteral("        ]"),
        /* 8 */ QStringLiteral("    )"),
        /* 9 */ QStringLiteral("}))")
    });

    CodeModel model(doc.get());
    std::optional<int> res;

    res = model.findMatchingBracket(5000);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(-1);
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(globalPos(*doc, 0, 0));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    // Code round brackets, multiline
    res = model.findMatchingBracket(globalPos(*doc, 0, 6));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 9, 2)));

    res = model.findMatchingBracket(globalPos(*doc, 9, 2));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 0, 6)));

    res = model.findMatchingBracket(globalPos(*doc, 0, 21));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 9, 1)));

    res = model.findMatchingBracket(globalPos(*doc, 9, 1));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 0, 21)));

    // Code block, multiline
    res = model.findMatchingBracket(globalPos(*doc, 0, 22));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 9, 0)));

    res = model.findMatchingBracket(globalPos(*doc, 9, 0));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 0, 22)));

    // Code round brackets, same line
    res = model.findMatchingBracket(globalPos(*doc, 2, 14));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 2, 20)));

    res = model.findMatchingBracket(globalPos(*doc, 2, 20));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 2, 14)));

    // Math delimiters
    res = model.findMatchingBracket(globalPos(*doc, 3, 17));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 3, 29)));

    res = model.findMatchingBracket(globalPos(*doc, 3, 29));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 3, 17)));

    // Function params in math mode
    res = model.findMatchingBracket(globalPos(*doc, 3, 26));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 3, 28)));

    res = model.findMatchingBracket(globalPos(*doc, 3, 28));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 3, 26)));

    // Round brackets inside a string literal
    res = model.findMatchingBracket(globalPos(*doc, 4, 22));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(globalPos(*doc, 4, 23));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    // Content block delimiters
    res = model.findMatchingBracket(globalPos(*doc, 5, 8));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 7, 8)));

    res = model.findMatchingBracket(globalPos(*doc, 7, 8));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 5, 8)));

    // Round brackets in content mode
    res = model.findMatchingBracket(globalPos(*doc, 6, 18));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(globalPos(*doc, 6, 22   ));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));
}

TEST(CodeModelTests, FindMatchingBracket_CodeExpression) {
    auto doc = buildDocument({
        /* 0 */ QStringLiteral("#{"),
        /* 1 */ QStringLiteral("    let x = 2"),
        /* 2 */ QStringLiteral("}")
    });

    CodeModel model(doc.get());
    std::optional<int> res;

    res = model.findMatchingBracket(globalPos(*doc, 0, 0));
    EXPECT_THAT(res, ::testing::Eq(std::nullopt));

    res = model.findMatchingBracket(globalPos(*doc, 0, 1));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 2, 0)));

    res = model.findMatchingBracket(globalPos(*doc, 2, 0));
    EXPECT_THAT(res, ::testing::Eq(globalPos(*doc, 0, 1)));
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
        /* 0 */ "#if 5 > 2 { pagebreak()",
        /* 1 */ "table(",
        /* 2 */ "..nums.map(n => $ln(n)$) + 1",
        /* 3 */ "[Final]) }"
    });

    CodeModel model(doc.get());

    EXPECT_FALSE(model.shouldIncreaseIndent(5000));
    EXPECT_FALSE(model.shouldIncreaseIndent(-1));

    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 0, 0)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 0, 9)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 0, 10)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 0, 11)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 0, 22)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 0, 23)));

    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 1, 0)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 1, 5)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 1, 6)));

    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 2, 0)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 2, 10)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 2, 11)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 2, 16)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 2, 20)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 2, 22)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 2, 23)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 2, 24)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 2, 28)));

    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 3, 0)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 3, 1)));
    EXPECT_TRUE (model.shouldIncreaseIndent(globalPos(*doc, 3, 6)));
    EXPECT_FALSE(model.shouldIncreaseIndent(globalPos(*doc, 3, 7)));
}

TEST(CodeModelTests, FindMatchingIndentBlockByPosition)
{
    auto doc = buildDocument({
        /* 0 */ "#if 5 > 2 { pagebreak() }",
        /* 1 */ "#while 1 < 2 [",
        /* 2 */ "bar",
        /* 3 */ "foo ]"
    });

    CodeModel model(doc.get());
    QTextBlock res;

    res = model.findMatchingIndentBlock(10000);
    EXPECT_FALSE(res.isValid());

    res = model.findMatchingIndentBlock(-1);
    EXPECT_FALSE(res.isValid());

    res = model.findMatchingIndentBlock(globalPos(*doc, 0, 0));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(globalPos(*doc, 0, 12));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(globalPos(*doc, 0, 24));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(globalPos(*doc, 1, 0));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));

    res = model.findMatchingIndentBlock(globalPos(*doc, 1, 13));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));

    res = model.findMatchingIndentBlock(globalPos(*doc, 2, 1));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(2));

    res = model.findMatchingIndentBlock(globalPos(*doc, 3, 0));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(3));

    res = model.findMatchingIndentBlock(globalPos(*doc, 3, 4));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));
}

TEST(CodeModelTests, FindMatchingIndentBlockByBlock)
{
    auto doc = buildDocument({
        /* 0 */ "#if 5 > 2 { pagebreak() }",
        /* 1 */ "#while 1 < 2 [",
        /* 2 */ "bar",
        /* 3 */ "foo ]"
    });

    CodeModel model(doc.get());
    QTextBlock res;

    res = model.findMatchingIndentBlock(doc->end());
    EXPECT_FALSE(res.isValid());

    res = model.findMatchingIndentBlock(doc->findBlockByNumber(0));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(0));

    res = model.findMatchingIndentBlock(doc->findBlockByNumber(1));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));

    res = model.findMatchingIndentBlock(doc->findBlockByNumber(2));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(2));

    res = model.findMatchingIndentBlock(doc->findBlockByNumber(3));
    EXPECT_THAT(res.blockNumber(), ::testing::Eq(1));
}

TEST(CodeModelTests, GetSymbolExpression)
{
    auto doc = buildDocument({
        /* 0 */ "",
        /* 1 */ "$ sqrt() $",
        /* 2 */ "#{  }",
        /* 3 */ "``"
    });

    CodeModel model(doc.get());
    QString res;

    res = model.getSymbolExpression("sym.RR", globalPos(*doc, 0, 0));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#sym.RR")));

    res = model.getSymbolExpression("sym.RR", globalPos(*doc, 1, 7));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("RR")));

    res = model.getSymbolExpression("sym.RR", globalPos(*doc, 2, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("sym.RR")));

    res = model.getSymbolExpression("sym.RR", globalPos(*doc, 3, 1));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("sym.RR")));

    res = model.getSymbolExpression("emoji.man.levitate", globalPos(*doc, 0, 0));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#emoji.man.levitate")));

    res = model.getSymbolExpression("emoji.man.levitate", globalPos(*doc, 1, 7));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#emoji.man.levitate")));

    res = model.getSymbolExpression("emoji.man.levitate", globalPos(*doc, 2, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("emoji.man.levitate")));

    res = model.getSymbolExpression("emoji.man.levitate", globalPos(*doc, 3, 1));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("emoji.man.levitate")));
}

TEST(CodeModelTests, getColorExpression)
{
    auto doc = buildDocument({
        /* 0 */ "== Heading",
        /* 1 */ "#{  }",
    });

    CodeModel model(doc.get());
    QString res;

    QColor c1 = Qt::red;

    res = model.getColorExpression(c1, globalPos(*doc, 0, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#rgb(\"#ff0000\")")));

    res = model.getColorExpression(c1, globalPos(*doc, 1, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("rgb(\"#ff0000\")")));

    QColor c2(10, 5, 12, 8);

    res = model.getColorExpression(c2, globalPos(*doc, 0, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#rgb(\"#0a050c08\")")));

    res = model.getColorExpression(c2, globalPos(*doc, 1, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("rgb(\"#0a050c08\")")));

    QColor c3 = QColor::fromHsv(20, 20, 20);

    res = model.getColorExpression(c3, globalPos(*doc, 0, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("#rgb(\"#141312\")")));

    res = model.getColorExpression(c3, globalPos(*doc, 1, 3));
    EXPECT_THAT(res, ::testing::Eq(QStringLiteral("rgb(\"#141312\")")));

    QColor c4;

    res = model.getColorExpression(c4, globalPos(*doc, 0, 3));
    EXPECT_TRUE(res.isEmpty());

    res = model.getColorExpression(c4, globalPos(*doc, 1, 3));
    EXPECT_TRUE(res.isEmpty());
}

Q_GLOBAL_STATIC(QStringList, GET_MATCHING_CLOSE_BRACKET_TEST_DOC, {
    /* 0 */ "== English \\content",
    /* 1 */ "תוכן *מודגש* _כזה_ בעברית",
    /* 2 */ "#while 1 > 2",
    /* 3 */ "$\"AB\" = ln(1 + x)$",
    /* 4 */ "// a comment",
    /* 5 */ "`raw content`",
    /* 6 */ "#par $ x = #rect $",
    /* 7 */ "#text()"
})

class CodeModel_GetMatchingCloseBracketTests : public ::testing::Test {
protected:
    CodeModel_GetMatchingCloseBracketTests()
        : doc(buildDocument(*GET_MATCHING_CLOSE_BRACKET_TEST_DOC))
        , model(doc.get())
    {
    }

    QTextCursor cursorAt(int block, int positionInBlock, int selectionLength = 0)
    {
        QTextCursor cursor { doc->findBlockByNumber(block) };

        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, positionInBlock);
        if (selectionLength > 0) {
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectionLength);
        }
        return cursor;
    }

    std::unique_ptr<QTextDocument> doc;
    CodeModel model;
};

TEST_F(CodeModel_GetMatchingCloseBracketTests, Parentheses)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('(')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('(')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('(')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('(')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('(')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('(')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 4),  QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 16), QLatin1Char('(')), ::testing::Eq(QLatin1Char(')')));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, CurlyBrackets)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('{')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('{')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('{')), ::testing::Eq(QLatin1Char('}')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('{')), ::testing::Eq(QLatin1Char('}')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('{')), ::testing::Eq(QLatin1Char('}')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('{')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('{')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('{')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 4),  QLatin1Char('{')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 16), QLatin1Char('{')), ::testing::Eq(std::nullopt));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, SquareBrackets)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('[')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('[')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('[')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('[')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('[')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('[')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 4),  QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(6, 16), QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(7, 7),  QLatin1Char('[')), ::testing::Eq(QLatin1Char(']')));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, MathDelimiters)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 12), QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('$')), ::testing::Eq(QLatin1Char('$')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('$')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('$')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('$')), ::testing::Eq(std::nullopt));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, DoubleQuotes)
{
    // Content or raw, but not after hebrew
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 12), QLatin1Char('"')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),    QLatin1Char('"')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3, 1), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8, 1), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26),   QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('"')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('"')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('"')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('"')), ::testing::Eq(QLatin1Char('"')));

}

TEST_F(CodeModel_GetMatchingCloseBracketTests, AngleBrackets)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('<')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('<')), ::testing::Eq(QLatin1Char('>')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('<')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('<')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('<')), ::testing::Eq(std::nullopt));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, StrongEmphasis)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 15), QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('*')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('*')), ::testing::Eq(QLatin1Char('*')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('*')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('*')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('*')), ::testing::Eq(std::nullopt));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, Emphasis)
{
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 10), QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(0, 19), QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 3),  QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 8),  QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 15), QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(1, 26), QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 0),  QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 1),  QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 7),  QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(2, 12), QLatin1Char('_')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 0),  QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 1),  QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 2),  QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 12), QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 17), QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(3, 18), QLatin1Char('_')), ::testing::Eq(QLatin1Char('_')));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 10), QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(4, 12), QLatin1Char('_')), ::testing::Eq(std::nullopt));

    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 10), QLatin1Char('_')), ::testing::Eq(std::nullopt));
    EXPECT_THAT(model.getMatchingCloseBracket(cursorAt(5, 12), QLatin1Char('_')), ::testing::Eq(std::nullopt));
}

TEST_F(CodeModel_GetMatchingCloseBracketTests, UnsupportedChar)
{
    QTextCursor cursor { doc.get() };
    while (!cursor.atEnd()) {
        EXPECT_THAT(model.getMatchingCloseBracket(cursor, QLatin1Char('\'')), ::testing::Eq(std::nullopt));
        cursor.movePosition(QTextCursor::NextCharacter);
    }
}
