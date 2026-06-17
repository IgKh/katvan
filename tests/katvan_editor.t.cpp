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

#include "katvan_document.h"
#include "katvan_editor.h"
#include "katvan_text_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QApplication>

#include <memory>

using namespace katvan;

static QString keyToAscii(int key)
{
    switch (key) {
        case Qt::Key_ParenRight: return QStringLiteral(")");
        case Qt::Key_BraceLeft: return QStringLiteral("{");
        default: return QString();
    }
}

struct EditorHolder
{
    EditorHolder(const QString& text, const EditorSettings& settings = EditorSettings())
        : document(new Document())
        , editor(new Editor(document.get(), nullptr))
    {
        editor->applySettings(settings);
        document->setDocumentText(text);
    }

    void setText(const QString& text)
    {
        document->setDocumentText(text);
    }

    QString text()
    {
        return document->toPlainText();
    }

    int cursorPosition()
    {
        return editor->textCursor().position();
    }

    std::tuple<int, int> selectionRange()
    {
        QTextCursor cursor = editor->textCursor();
        return std::make_tuple(cursor.selectionStart(), cursor.selectionEnd());
    }

    void selectRange(int from, int to)
    {
        QTextCursor cursor { document.get() };
        cursor.setPosition(from);
        cursor.setPosition(to, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
    }

    void sendKeyPress(int pos, int key, Qt::KeyboardModifiers modifiers)
    {
        if (pos >= 0) {
            QTextCursor cursor { document.get() };
            cursor.setPosition(pos);
            editor->setTextCursor(cursor);
        }

        QString text = keyToAscii(key);
        QKeyEvent e(QEvent::KeyPress, key, modifiers, text);
        QApplication::sendEvent(editor.get(), &e);
    }

    void sendKeyPress(int pos, const QKeySequence& sequence) {
        if (sequence.count() != 1) {
            return;
        }
        sendKeyPress(pos, sequence[0].key(), sequence[0].keyboardModifiers());
    }

    std::unique_ptr<Document> document;
    std::unique_ptr<Editor> editor;
};

TEST(EditorTests, BackspaceToBidiMark) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(4);

    QString text = utils::RLM_MARK + QStringLiteral("   foo");
    EditorHolder holder(text, settings);

    holder.sendKeyPress(1, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("   foo")));

    holder.setText(text);
    holder.sendKeyPress(4, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(utils::RLM_MARK + QStringLiteral("foo")));
}

TEST(EditorTests, InsertClosingBracketSkip) {
    EditorSettings settings;
    settings.setAutoBrackets(true);

    QString text = QStringLiteral("#text() ");
    EditorHolder holder(text, settings);

    holder.sendKeyPress(6, Qt::Key_ParenRight, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(text));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(7));
}

TEST(EditorTests, InsertClosingBracketInsertAnyway) {
    EditorSettings settings;
    settings.setAutoBrackets(false);

    QString text = QStringLiteral("#text()");
    EditorHolder holder(text, settings);

    holder.sendKeyPress(6, Qt::Key_ParenRight, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("#text())")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(7));
}

TEST(EditorTests, InsertClosingBracketMultiline) {
    EditorSettings settings;
    settings.setAutoBrackets(true);

    QString text = QStringLiteral("#\n");
    EditorHolder holder(text, settings);

    holder.sendKeyPress(1, Qt::Key_BraceLeft, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("#{}\n")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(2));
}

TEST(EditorTests, MoveToLineEdges) {
    EditorSettings settings;

    QString text = QStringLiteral("  ") + QStringLiteral("long ").repeated(100);
    EditorHolder holder(text, settings);

    holder.editor->setFixedWidth(50);

    // Place the cursor in the middle of a word, somewhere in the middle
    constexpr int INITIAL_POSITION = 248;

    // Move to the end - should move to the of the text line
    holder.sendKeyPress(INITIAL_POSITION, QKeySequence::MoveToEndOfLine);

    int p1 = holder.cursorPosition();
    EXPECT_THAT(p1, ::testing::Gt(INITIAL_POSITION));
    EXPECT_THAT(p1, ::testing::Ne(text.length()));

    // Another move to the end - should move to the end of the block
    holder.sendKeyPress(p1, QKeySequence::MoveToEndOfLine);

    int p2 = holder.cursorPosition();
    EXPECT_THAT(p2, ::testing::Eq(text.length()));

    // Another move to the end - should stay put
    holder.sendKeyPress(p2, QKeySequence::MoveToEndOfLine);

    int p3 = holder.cursorPosition();
    EXPECT_THAT(p3, ::testing::Eq(p2));

    // Move to start - move to start of the text line
    holder.sendKeyPress(INITIAL_POSITION, QKeySequence::MoveToStartOfLine);

    int p4 = holder.cursorPosition();
    EXPECT_THAT(p4, ::testing::Lt(INITIAL_POSITION));
    EXPECT_THAT(p4, ::testing::Ne(0));
    EXPECT_THAT(p4, ::testing::Ne(2));

    // Another move to start - should go to first line after the initial whitespace
    holder.sendKeyPress(p4, QKeySequence::MoveToStartOfLine);

    int p5 = holder.cursorPosition();
    EXPECT_THAT(p5, ::testing::Eq(2));

    // Another move to start - should go to start of block
    holder.sendKeyPress(p5, QKeySequence::MoveToStartOfLine);

    int p6 = holder.cursorPosition();
    EXPECT_THAT(p6, ::testing::Eq(0));

    // Another move to start - should go back to p5
    holder.sendKeyPress(p6, QKeySequence::MoveToStartOfLine);

    int p7 = holder.cursorPosition();
    EXPECT_THAT(p7, ::testing::Eq(p5));
}

TEST(EditorTests, SelectToLineEdges) {
    EditorSettings settings;

    QString text = QStringLiteral("  ") + QStringLiteral("long ").repeated(100);
    EditorHolder holder(text, settings);

    holder.editor->setFixedWidth(50);

    // Place the cursor in the middle of a word, somewhere in the middle
    constexpr int INITIAL_POSITION = 248;

    // Move to the end - should move to the of the text line
    holder.sendKeyPress(INITIAL_POSITION, QKeySequence::SelectEndOfLine);

    auto [s1, e1] = holder.selectionRange();
    EXPECT_THAT(s1, ::testing::Eq(INITIAL_POSITION));
    EXPECT_THAT(e1, ::testing::Gt(INITIAL_POSITION));
    EXPECT_THAT(e1, ::testing::Ne(text.length()));

    // Another move to the end - should move to the end of the block
    holder.sendKeyPress(-1, QKeySequence::SelectEndOfLine);

    auto [s2, e2] = holder.selectionRange();
    EXPECT_THAT(s2, ::testing::Eq(INITIAL_POSITION));
    EXPECT_THAT(e2, ::testing::Eq(text.length()));

    // Move to start - move to start of the text line
    holder.sendKeyPress(INITIAL_POSITION, QKeySequence::SelectStartOfLine);

    auto [s3, e3] = holder.selectionRange();
    EXPECT_THAT(s3, ::testing::Lt(INITIAL_POSITION));
    EXPECT_THAT(s3, ::testing::Ne(0));
    EXPECT_THAT(s3, ::testing::Ne(2));
    EXPECT_THAT(e3, ::testing::Eq(INITIAL_POSITION));

    // Another move to start - should go to first line after the initial whitespace
    holder.sendKeyPress(-1, QKeySequence::SelectStartOfLine);

    auto [s4, e4] = holder.selectionRange();
    EXPECT_THAT(s4, ::testing::Eq(2));
    EXPECT_THAT(e4, ::testing::Eq(INITIAL_POSITION));

    // Another move to start - should go to start of block
    holder.sendKeyPress(-1, QKeySequence::SelectStartOfLine);

    auto [s5, e5] = holder.selectionRange();
    EXPECT_THAT(s5, ::testing::Eq(0));
    EXPECT_THAT(e5, ::testing::Eq(INITIAL_POSITION));
}

TEST(EditorTests, IndentSingleLineWithSpaces) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(3);

    QString text = QStringLiteral("AB\nCD\n EF");
    EditorHolder holder(text, settings);

    // In start of line
    holder.sendKeyPress(3, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("AB\n   CD\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(6));

    // In leading whitespace
    holder.sendKeyPress(10, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("AB\n   CD\n   EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(12));

    // In middle of a line - should only complete to the next multiple of the indent width
    holder.sendKeyPress(1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("A  B\n   CD\n   EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(3));

    // Selection inside a line - should override selection with indent to next multiple
    holder.selectRange(9, 10);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("A  B\n   C  \n   EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(11));
}

TEST(EditorTests, IndentMultipleLinesWithSpaces) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(3);

    QString text = QStringLiteral("AB\nCDE\n FG");
    EditorHolder holder(text, settings);

    // Select all of line 1 and part of line 2
    holder.selectRange(0, 4);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("   AB\n   CDE\n FG")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(3, 10)));

    // Select all of line 3
    holder.selectRange(13, 16);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("   AB\n   CDE\n   FG")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(13, 18)));
}

TEST(EditorTests, IndentSingleLineWithTabs) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::TABS);
    settings.setIndentWidth(5);
    settings.setTabWidth(2);

    QString text = QStringLiteral("AB\nCD\n EF\n\tGH");
    EditorHolder holder(text, settings);

    // In start of line
    holder.sendKeyPress(3, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("AB\n\tCD\n EF\n\tGH")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(4));

    // In leading whitespace
    holder.sendKeyPress(7, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("AB\n\tCD\n\tEF\n\tGH")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(8));

    // In middle of a line
    holder.sendKeyPress(1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("A\tB\n\tCD\n\tEF\n\tGH")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(2));

    // Selection inside a line - should override selection with tab
    holder.selectRange(10, 11);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("A\tB\n\tCD\n\tE\t\n\tGH")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(11));
}

TEST(EditorTests, IndentMultipleLinesWithTabs) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::TABS);
    settings.setIndentWidth(5);
    settings.setTabWidth(2);

    QString text = QStringLiteral("AB\n\tCDE\n   FG");
    EditorHolder holder(text, settings);

    // Select all of line 1 and part of line 2
    holder.selectRange(0, 6);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("\tAB\n\t\tCDE\n   FG")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(1, 8)));

    // Select all of line 3
    holder.selectRange(10, 15);
    holder.sendKeyPress(-1, Qt::Key_Tab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("\tAB\n\t\tCDE\n\t\tFG")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(12, 14)));
}

TEST(EditorTests, DedentSingleLineWithSpacesByBacktab) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(3);

    QString text = QStringLiteral("    AB\n   CD\n EF");
    EditorHolder holder(text, settings);

    // In leading whitespace
    holder.sendKeyPress(0, Qt::Key_Backtab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral(" AB\n   CD\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(0));

    // In middle of line
    holder.sendKeyPress(8, Qt::Key_Backtab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral(" AB\nCD\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(5));

    // Selection inside a line
    holder.selectRange(9, 10);
    holder.sendKeyPress(-1, Qt::Key_Backtab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral(" AB\nCD\nEF")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(8, 9)));
}

TEST(EditorTests, DedentSingleLineWithSpacesByBackspace) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(3);

    QString text = QStringLiteral("    AB\n   CD\n EF");
    EditorHolder holder(text, settings);

    // Start of leading whitespace
    holder.sendKeyPress(0, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("    AB\n   CD\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(0));

    // In leading whitespace
    holder.sendKeyPress(2, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("  AB\n   CD\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(0));

    // In middle of line
    holder.sendKeyPress(9, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("  AB\n   D\n EF")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(8));

    // Selection inside a line
    holder.selectRange(11, 13);
    holder.sendKeyPress(-1, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("  AB\n   D\n ")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(11, 11)));
}

TEST(EditorTests, DedentMultipleLinesWithSpaces) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(3);

    QString text = QStringLiteral("  AB\n     CD\n   FG");
    EditorHolder holder(text, settings);

    // Select all
    holder.editor->selectAll();
    holder.sendKeyPress(-1, Qt::Key_Backtab, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("AB\n  CD\nFG")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(0, 10)));

    // Backspace - will delete
    holder.sendKeyPress(-1, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QString()));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(0, 0)));
}

TEST(EditorTests, DedentMixedWhitespace) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::TABS);
    settings.setIndentWidth(2);
    settings.setTabWidth(3);

    QString text = QStringLiteral("    AB");
    EditorHolder holder(text, settings);

    holder.sendKeyPress(4, Qt::Key_Backspace, Qt::NoModifier);
    EXPECT_THAT(holder.text(), ::testing::Eq(QString("\tAB")));
    EXPECT_THAT(holder.cursorPosition(), ::testing::Eq(1));
}
