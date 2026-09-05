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

#include <QApplication>
#include <QString>

#include <iostream>

QT_BEGIN_NAMESPACE

void PrintTo(QChar ch, std::ostream* os)
{
    *os << '\'' << ch.toLatin1() << '\'';
}

void PrintTo(const QString& str, std::ostream* os)
{
    // Escape tabs and newlines
    std::string res;
    for (QChar c : str) {
        if (c == QLatin1Char('\n')) {
            res.append("\\n");
        }
        else if (c == QLatin1Char('\t')) {
            res.append("\\t");
        }
        else {
            res.append(QString(c).toStdString());
        }
    }

    *os << '\"' << res << '\"';
}

QT_END_NAMESPACE

static QString keyToAscii(int key)
{
    switch (key) {
        case Qt::Key_ParenRight: return QStringLiteral(")");
        case Qt::Key_BraceLeft: return QStringLiteral("{");
        default: return QString();
    }
}

EditorHolder::EditorHolder(const QString& text)
    : EditorHolder(text, katvan::EditorSettings())
{
}

EditorHolder::EditorHolder(const QString& text, const katvan::EditorSettings& settings)
    : document(new katvan::Document())
    , editor(new katvan::Editor(document.get(), nullptr))
{
    editor->applySettings(settings);
    document->setDocumentText(text);
}

EditorHolder::~EditorHolder()
{
}

void EditorHolder::setText(const QString& text)
{
    document->setDocumentText(text);
}

QString EditorHolder::text()
{
    return document->toPlainText();
}

int EditorHolder::cursorPosition()
{
    return editor->textCursor().position();
}

std::tuple<int, int> EditorHolder::selectionRange()
{
    QTextCursor cursor = editor->textCursor();
    return std::make_tuple(cursor.selectionStart(), cursor.selectionEnd());
}

void EditorHolder::selectRange(int from, int to)
{
    QTextCursor cursor { document.get() };
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}

void EditorHolder::sendKeyPress(int pos, int key, Qt::KeyboardModifiers modifiers)
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

void EditorHolder::sendKeyPress(int pos, const QKeySequence& sequence)
{
    if (sequence.count() != 1) {
        return;
    }
    sendKeyPress(pos, sequence[0].key(), sequence[0].keyboardModifiers());
}
