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
#include "katvan_editorlayout.h"
#include "katvan_utils.h"

#include <QGuiApplication>
#include <QPainter>
#include <QStyle>
#include <QTextDocument>

namespace katvan {

/**
 * This is the custom document layout engine for Katvan's editor component. It
 * unfortunately has to exist in order to safely implement our line directionality
 * heuristic.
 *
 * It is similar in nature (but not in code!) to Qt's own QPlainTextDocumentLayout
 * class. Like it, we work by laying out plain text blocks vertically one after
 * the other. Unlike it, we DO concern ourselves with vertical pixels, and allow
 * each line to have a different base text direction.
 *
 * While in theory this layout class can be used with any QTextEdit, by making
 * assumptions specific to Katvan we can cut all sorts of corners. These assumptions
 * are:
 *
 * - The document contains only simple text blocks, and all of them are directly
 *   part of the root frame. Each block represents a single line in the edited
 *   file, although word wrapping may cause one block to cover multiple lines
 *   in the editor widget.
 *
 * - The content of the document is never printed, or painted unto anything which
 *   isn't the viewport of the editor widget.
 *
 * - Blocks are never hidden (QTextBlock::isVisible is always true), and no
 *   special QTextBlockFormat properties are in use. All formatting is via
 *   QTextCharFormat objects set by the syntax highlighter.
 *
 * - Document margins are kept at their default and never change.
 *
 * - Word wrapping is always used; a horizontal scrollbar is never visible.
 *
 * - Override mode isn't used (it doesn't actually impact layout, but the document
 *   layout engine has to draw the cursor differently and slightly change the hit
 *   test logic).
 */

constexpr int CURSOR_WIDTH = 1;

EditorLayout::EditorLayout(QTextDocument* document)
    : QAbstractTextDocumentLayout(document)
    , d_documentSize(0, 0)
{
}

QSizeF EditorLayout::documentSize() const
{
    // Margin never changes for us...
    qreal margin = document()->documentMargin();
    return d_documentSize.grownBy(QMarginsF(margin, margin, margin, margin));
}

int EditorLayout::pageCount() const
{
    // Breaking source code to pages in meaningless
    return 1;
}

QRectF EditorLayout::frameBoundingRect(QTextFrame* frame) const
{
    if (frame != document()->rootFrame()) {
        return QRectF();
    }
    return QRectF(QPointF(0, 0), documentSize());
}

QRectF EditorLayout::blockBoundingRect(const QTextBlock& block) const
{
    if (!block.isValid()) {
        return QRectF();
    }

    QTextLayout* layout = block.layout();
    if (layout->lineCount() == 0) {
        // Not layed out yet
        return QRectF();
    }

    QRectF rect = layout->boundingRect();
    rect.moveTopLeft(layout->position());
    return rect;
}

int EditorLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const
{
    Q_UNUSED(accuracy)

    QTextBlock block = findContainingBlock(point.y());
    if (!block.isValid()) {
        return -1;
    }

    QTextLayout* layout = block.layout( );
    QPointF blockPoint = point - layout->position();

    for (int i = 0; i < layout->lineCount(); i++) {
        QTextLine line = layout->lineAt(i);
        if (!line.rect().contains(blockPoint)) {
            continue;
        }

        int pos = line.xToCursor(point.x());
        if (pos >= 0) {
            return pos + block.position();
        }
    }
    return -1;
}

void EditorLayout::draw(QPainter* painter, const QAbstractTextDocumentLayout::PaintContext& context)
{
    QRectF clip = context.clip;
    if (!clip.isValid()) {
        clip = QRectF(QPointF(0, 0), documentSize());
    }

    // Make sure that the right margin is NOT part of the clip rectangle, so
    // that it won't be painted over by a FullWidthSelection
    qreal maxX = document()->textWidth() - document()->documentMargin() + CURSOR_WIDTH;
    clip.setRight(qMin(clip.right(), maxX));

    QTextBlock block = findContainingBlock(clip.top());

    while (block.isValid()) {
        QTextLayout* layout = block.layout();
        if (layout->position().y() > clip.bottom()) {
            break;
        }

        int blockStart = block.position();
        int blockEnd = block.position() + block.length();

        QList<QTextLayout::FormatRange> formats;
        for (const QAbstractTextDocumentLayout::Selection& sel : context.selections) {
            bool selectionIntersectsBlock = sel.cursor.selectionStart() < blockEnd && sel.cursor.selectionEnd() >= blockStart;

            if (sel.cursor.hasSelection() && selectionIntersectsBlock) {
                int start = qMax(sel.cursor.selectionStart() - blockStart, 0);
                int end = qMin(sel.cursor.selectionEnd() - blockStart, blockEnd);

                QTextLayout::FormatRange fmt;
                fmt.start = start;
                fmt.length = end - start;
                fmt.format = sel.format;
                formats.append(fmt);
            }
            else if (!sel.cursor.hasSelection()
                        && sel.format.boolProperty(QTextFormat::FullWidthSelection)
                        && block.contains(sel.cursor.position())) {
                QTextLine line = layout->lineForTextPosition(sel.cursor.position() - blockStart);
                Q_ASSERT(line.isValid());

                QTextLayout::FormatRange fmt;
                fmt.start = line.textStart();
                fmt.length = line.textLength();
                fmt.format = sel.format;

                if (fmt.start + fmt.length == block.length() - 1) {
                    // For the last line in a block, we must ensure that the
                    // selection ends after the block
                    fmt.length++;
                }
                formats.append(fmt);
            }
        }

        layout->draw(painter, QPointF(), formats, clip);

        if (block.contains(context.cursorPosition)) {
            painter->setPen(context.palette.color(QPalette::Text));
            layout->drawCursor(painter, QPointF(), context.cursorPosition - block.position(), CURSOR_WIDTH);
        }

        block = block.next();
    }
}

void EditorLayout::documentChanged(int position, int charsRemoved, int charsAdded)
{
    QTextBlock startBlock = document()->findBlock(position);
    QTextBlock endBlock = document()->findBlock(qMax(0, position + charsAdded + charsRemoved));
    if (!endBlock.isValid()) {
        endBlock = document()->lastBlock();
    }

    qreal y;
    if (startBlock.previous().isValid()) {
        QTextLayout* prevLayout = startBlock.previous().layout();
        y = prevLayout->position().y() + prevLayout->boundingRect().height();
    }
    else {
        y = document()->documentMargin();
    }

    bool updated = false;
    for (QTextBlock block = startBlock; block.isValid(); block = block.next()) {
        if (endBlock < block) {
            break;
        }

        updated = true;
        layoutBlock(block, y);

        QRectF blockRect = blockBoundingRect(block);
        y += blockRect.height();
    }

    if (!updated) {
        return;
    }

    // Adding or removing block in the middle of the document can cause all
    // following blocks to shift up or down. Iterate on the remaining blocks,
    // just to update their y position if needed (size will remain the same).
    for (QTextBlock block = endBlock.next(); block.isValid(); block = block.next()) {
        QTextLayout* layout = block.layout();
        QPointF pos = layout->position();
        if (qFuzzyCompare(pos.y(), y)) {
            break;
        }

        QPointF newPos(pos.x(), y);
        layout->setPosition(newPos);

        y += layout->boundingRect().height();
    }

    recalculateDocumentSize();
    Q_EMIT update();
}

static Qt::LayoutDirection getBlockDirection(const QTextBlock& block)
{
    Qt::LayoutDirection dir = utils::naturalTextDirection(block.text());
    if (dir == Qt::LayoutDirectionAuto) {
        QTextBlock prevBlock = block.previous();
        if (prevBlock.isValid()) {
            dir = prevBlock.layout()->textOption().textDirection();
        }
    }
    if (dir == Qt::LayoutDirectionAuto) {
        dir = qGuiApp->inputMethod()->inputDirection();
    }
    if (dir == Qt::LayoutDirectionAuto) {
        dir = qGuiApp->layoutDirection();
    }
    return dir;
}

void EditorLayout::layoutBlock(QTextBlock& block, qreal topY)
{
    QTextLayout* layout = block.layout();

    qreal margin = document()->documentMargin();
    qreal availableWidth = document()->textWidth() - 2 * margin;

    Qt::LayoutDirection dir = getBlockDirection(block);

    QTextOption option = document()->defaultTextOption();
    option.setTextDirection(dir);
    option.setAlignment(QStyle::visualAlignment(dir, option.alignment()));
    layout->setTextOption(option);

    layout->beginLayout();

    qreal lineHeight = 0;

    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLeadingIncluded(true);
        line.setLineWidth(availableWidth);
        line.setPosition(QPointF(0, lineHeight));

        lineHeight += line.height();
    }

    layout->setPosition(QPointF(margin, topY));
    layout->endLayout();

    block.setLineCount(layout->lineCount());
}

QTextBlock EditorLayout::findContainingBlock(qreal y) const
{
    // Skip top margin
    y = qMax(y, document()->documentMargin());

    // Binary search for the block whose bounding rect contains the given y
    int from = 0;
    int to = document()->blockCount();

    while (from < to) {
        int mid = from + (to - from) / 2;
        QTextBlock block = document()->findBlockByNumber(mid);
        QRectF blockRect = blockBoundingRect(block);

        if (y >= blockRect.top() && y <= blockRect.bottom()) {
            return block;
        }
        else if (y < blockRect.top()) {
            to = mid;
        }
        else {
            Q_ASSERT(y > blockRect.bottom());
            from = mid + 1;
        }
    }
    return QTextBlock();
}

void EditorLayout::recalculateDocumentSize()
{
    // TODO: This way (recalculating from scratch after every change) ensures
    // correctness, but is somewhat inefficient. Try to find a way to maintain
    // the document's size incrementally. This is tricky because sometimes a
    // block's layout is already invalidated before documentChanged is called.
    qreal height = 0;
    qreal width = 0;

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        QTextLayout* layout = block.layout();
        for (int i = 0; i < layout->lineCount(); i++) {
            QTextLine line = layout->lineAt(i);
            height += line.height();
            width = qMax(width, line.naturalTextWidth());
        }
    }

    QSizeF newDocumentSize(width, height);
    if (newDocumentSize != d_documentSize) {
        d_documentSize = newDocumentSize;
        Q_EMIT documentSizeChanged(documentSize());
    }
}

}
