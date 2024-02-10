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
#include "katvan_highlighter.h"
#include "katvan_spellchecker.h"

#include <QAbstractTextDocumentLayout>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QTextBlock>
#include <QTimer>

namespace katvan {

static constexpr QChar LRM_MARK = (ushort)0x200e;
static constexpr QChar LRI_MARK = (ushort)0x2066;
static constexpr QChar PDI_MARK = (ushort)0x2069;

static QKeySequence TEXT_DIRECTION_TOGGLE(Qt::CTRL | Qt::SHIFT | Qt::Key_X);

class LineNumberGutter : public QWidget
{
public:
    LineNumberGutter(Editor *editor) : QWidget(editor), d_editor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(d_editor->lineNumberGutterWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        d_editor->lineNumberGutterPaintEvent(this, event);
    }

private:
    Editor *d_editor;
};

Editor::Editor(QWidget* parent)
    : QTextEdit(parent)
{
    setAcceptRichText(false);

    d_spellChecker = new SpellChecker();
    d_highlighter = new Highlighter(document(), d_spellChecker);

    d_leftLineNumberGutter = new LineNumberGutter(this);
    d_rightLineNumberGutter = new LineNumberGutter(this);

    connect(document(), &QTextDocument::blockCountChanged, this, &Editor::updateLineNumberGutterWidth);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &Editor::updateLineNumberGutters);
    connect(this, &QTextEdit::textChanged, this, &Editor::updateLineNumberGutters);
    connect(this, &QTextEdit::cursorPositionChanged, this, &Editor::updateLineNumberGutters);

    updateLineNumberGutters();

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

void Editor::insertLRM()
{
    textCursor().insertText(LRM_MARK);
}

void Editor::insertInlineMath()
{
    QTextCursor cursor = textCursor();
    cursor.insertText(LRI_MARK + QStringLiteral("$") + cursor.selectedText() + QStringLiteral("$") + PDI_MARK);
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

void Editor::goToBlock(int blockNum)
{
    QTextBlock block = document()->findBlockByNumber(blockNum);
    if (block.isValid()) {
        setTextCursor(QTextCursor(block));
    }
}

void Editor::forceRehighlighting()
{
    d_highlighter->rehighlight();
}

void Editor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu(event->pos());
    menu->addSeparator();
    menu->addAction(tr("Toggle Text Direction"), TEXT_DIRECTION_TOGGLE, this, &Editor::toggleTextBlockDirection);
    menu->exec(event->globalPos());
    delete menu;
}

void Editor::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::ShiftModifier && event->key() == Qt::Key_Return) {
        // For displayed line numbers to make sense, each QTextBlock must correspond
        // to one plain text line - meaning no newlines allowed in the middle of a
        // block. Since we only ever import and export plain text to the editor, the
        // only way to create such a newline is by typing it with Shift+Return; disable
        // this by sending the base implementation an event without the Shift modifier.
        QKeyEvent overrideEvent(
            QEvent::KeyPress,
            event->key(),
            Qt::NoModifier,
            QLatin1String("\n"),
            event->isAutoRepeat());

        QTextEdit::keyPressEvent(&overrideEvent);
        return;
    }
    QTextEdit::keyPressEvent(event);
}

void Editor::resizeEvent(QResizeEvent* event)
{
    QTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    int gutterWidth = lineNumberGutterWidth();
    int verticalScrollBarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;

    if (layoutDirection() == Qt::LeftToRight) {
        d_leftLineNumberGutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth, cr.height()));
        d_rightLineNumberGutter->setGeometry(QRect(cr.right() - gutterWidth - verticalScrollBarWidth, cr.top(), gutterWidth, cr.height()));
    }
    else {
        d_rightLineNumberGutter->setGeometry(QRect(cr.left() + verticalScrollBarWidth, cr.top(), gutterWidth, cr.height()));
        d_leftLineNumberGutter->setGeometry(QRect(cr.right() - gutterWidth, cr.top(), gutterWidth, cr.height()));
    }
}

int Editor::lineNumberGutterWidth()
{
    int digits = 1;
    int max = qMax(1, document()->blockCount());
    while (max >= 10) {
        max /= 10;
        digits++;
    }

    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void Editor::updateLineNumberGutterWidth()
{
    int gutterWidth = lineNumberGutterWidth();
    setViewportMargins(gutterWidth, 0, gutterWidth, 0);
}

void Editor::updateLineNumberGutters()
{
    QRect cr = contentsRect();
    d_leftLineNumberGutter->update(0, cr.y(), d_leftLineNumberGutter->width(), cr.height());
    d_rightLineNumberGutter->update(0, cr.y(), d_rightLineNumberGutter->width(), cr.height());

    updateLineNumberGutterWidth();

    int dy = verticalScrollBar()->sliderPosition();
    if (dy >= 0) {
        d_leftLineNumberGutter->scroll(0, dy);
        d_rightLineNumberGutter->scroll(0, dy);
    }
}

QTextBlock Editor::getFirstVisibleBlock()
{
    QTextDocument* doc = document();
    QRect viewportGeometry = viewport()->geometry();

    for (QTextBlock it = doc->firstBlock(); it.isValid(); it = it.next()) {
        QRectF blockRect = doc->documentLayout()->blockBoundingRect(it);

        // blockRect is in document coordinates, translate it to be relative to
        // the viewport. Then we want the first block that starts after the current
        // scrollbar position.
        blockRect.translate(viewportGeometry.topLeft());
        if (blockRect.y() > verticalScrollBar()->sliderPosition()) {
            return it;
        }
    }
    return QTextBlock();
}

void Editor::lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event)
{
    QColor bgColor(Qt::lightGray);
    QColor fgColor(120, 120, 120);

    QPainter painter(gutter);
    painter.fillRect(event->rect(), bgColor);

    QTextBlock block = getFirstVisibleBlock();
    int blockNumberUnderCursor = textCursor().blockNumber();

    QTextDocument* doc = document();
    QRect viewportGeometry = viewport()->geometry();

    qreal additionalMargin;
    if (block.blockNumber() == 0) {
        additionalMargin = doc->documentMargin() - 1 - verticalScrollBar()->sliderPosition();
    }
    else {
        // Getting the height of the visible part of the previous "non entirely visible" block
        QTextBlock prevBlock = block.previous();
        QRectF prevBlockRect = doc->documentLayout()->blockBoundingRect(prevBlock);
        prevBlockRect.translate(0, -verticalScrollBar()->sliderPosition());

        additionalMargin = prevBlockRect.intersected(viewportGeometry).height();
    }

    qreal top = viewportGeometry.top() + additionalMargin;
    qreal bottom = top + doc->documentLayout()->blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(block.blockNumber() + 1);

            painter.setPen(fgColor);

            QFont f = gutter->font();
            if (block.blockNumber() == blockNumberUnderCursor) {
                f.setWeight(QFont::ExtraBold);
            }
            painter.setFont(f);

            int textFlags;
            int textOffset;
            if (gutter == d_leftLineNumberGutter) {
                textFlags = Qt::AlignRight;
                textOffset = -5;
            }
            else {
                textFlags = Qt::AlignLeft;
                textOffset = 5;
            }
            if (layoutDirection() == Qt::RightToLeft) {
                textOffset *= -1;
            }

            QRectF r(textOffset, top, gutter->width(), painter.fontMetrics().height());
            painter.drawText(r, textFlags, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + doc->documentLayout()->blockBoundingRect(block).height();
    }
}

}
