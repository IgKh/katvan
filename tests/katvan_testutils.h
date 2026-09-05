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

#include <QChar>
#include <QtGlobal>

#include <iosfwd>
#include <memory>

namespace katvan {
    class Document;
    class Editor;
    class EditorSettings;
}

QT_BEGIN_NAMESPACE

class QKeySequence;
class QString;

void PrintTo(QChar ch, std::ostream* os);
void PrintTo(const QString& str, std::ostream* os);

QT_END_NAMESPACE

struct EditorHolder
{
    EditorHolder(const QString& text);
    EditorHolder(const QString& text, const katvan::EditorSettings& settings);
    ~EditorHolder();

    void setText(const QString& text);
    QString text();

    int cursorPosition();
    std::tuple<int, int> selectionRange();

    void selectRange(int from, int to);
    void sendKeyPress(int pos, int key, Qt::KeyboardModifiers modifiers);
    void sendKeyPress(int pos, const QKeySequence& sequence);

    std::unique_ptr<katvan::Document> document;
    std::unique_ptr<katvan::Editor> editor;
};
