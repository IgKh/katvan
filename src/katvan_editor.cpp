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
#include "katvan_editor.h"

#include <QMenu>
#include <QShortcut>
#include <QTextBlock>
#include <QTimer>

namespace katvan {

static constexpr QChar LRI_MARK = (ushort)0x2066;
static constexpr QChar PDI_MARK = (ushort)0x2069;

static QKeySequence TEXT_DIRECTION_TOGGLE(Qt::CTRL | Qt::SHIFT | Qt::Key_X);

Editor::Editor(QWidget* parent)
    : QTextEdit(parent)
{
    setAcceptRichText(false);

    QShortcut* toggleDirection = new QShortcut(this);
    toggleDirection->setKey(TEXT_DIRECTION_TOGGLE);
    toggleDirection->setContext(Qt::WidgetShortcut);
    connect(toggleDirection, &QShortcut::activated, this, &Editor::toggleTextBlockDirection);

    d_debounceTimer = new QTimer(this);
    d_debounceTimer->setSingleShot(true);
    d_debounceTimer->setInterval(500);
    d_debounceTimer->callOnTimeout(this, [this]() {
        emit contentModified(toPlainText());
    });

    connect(this, &QTextEdit::textChanged, [this]() {
        d_debounceTimer->start();
    });
}

void Editor::insertInlineMath()
{
    QTextCursor cursor = textCursor();
    cursor.insertText(LRI_MARK + QStringLiteral("$$") + PDI_MARK);
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, 2);
    setTextCursor(cursor);
}

void Editor::toggleTextBlockDirection()
{
    QTextCursor cursor = textCursor();
    Qt::LayoutDirection currentDirection = cursor.block().textDirection();

    QTextBlockFormat fmt;
    if (currentDirection == Qt::LeftToRight) {
        fmt.setLayoutDirection(Qt::RightToLeft);
    }
    else {
        fmt.setLayoutDirection(Qt::LeftToRight);
    }
    cursor.mergeBlockFormat(fmt);
}

void Editor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());
    menu->addSeparator();
    menu->addAction(tr("Toggle Text Direction"), TEXT_DIRECTION_TOGGLE, this, &Editor::toggleTextBlockDirection);
    menu->exec(event->globalPos());
    delete menu;
}

}
