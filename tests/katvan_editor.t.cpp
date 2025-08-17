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
        default: return QString();
    }
}

struct EditorHolder
{
    EditorHolder(const QString& text, const EditorSettings& settings = EditorSettings())
        : document(new Document())
        , editor(new Editor(document.get()))
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
