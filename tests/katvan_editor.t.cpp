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

    void sendKeyPress(int pos, int key, Qt::KeyboardModifiers modifiers)
    {
        QTextCursor cursor { document.get() };
        cursor.setPosition(pos);
        editor->setTextCursor(cursor);

        QKeyEvent e(QEvent::KeyPress, key, modifiers);
        QApplication::sendEvent(editor.get(), &e);
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
