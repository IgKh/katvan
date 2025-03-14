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
#include "katvan_codemodel.h"
#include "katvan_document.h"
#include "katvan_editorlayout.h"
#include "katvan_highlighter.h"
#include "katvan_text_utils.h"

#include <QGuiApplication>
#include <QPainter>
#include <QStyle>
#include <QTextDocument>

#include <memory>

namespace katvan {

/**
 * This is the custom document layout engine for Katvan's editor component. It
 * exists in order to safely implement things like our custom line directionality
 * heuristic and auto-injection of BiDi isolates.
 *
 * It is similar in nature (but not in code!) to Qt's own QPlainTextDocumentLayout
 * class. Like it, we work by laying out plain text blocks vertically one after
 * the other. Unlike it, we DO concern ourselves with vertical pixels, and allow
 * each line to have a different base text direction.
 *
 * Another significant difference is that blocks can have an additional QTextLayout
 * used for rendering only, so there can be some distinction between "edit" text and
 * "display" text. Text and formats for the latter are derived from the former with
 * custom logic.
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

class LayoutBlockData : public QTextBlockUserData
{
public:
    static constexpr BlockDataKind DATA_KIND = BlockDataKind::LAYOUT;

    LayoutBlockData() : textHash(qHash(QStringView())) {}

    std::unique_ptr<QTextLayout> displayLayout;
    QList<ushort> displayOffsets;
    size_t textHash;
};

static int adjustPosToDisplay(const QList<ushort>& displayOffsets, int pos)
{
    if (displayOffsets.isEmpty()) {
        return pos;
    }
    return pos + displayOffsets[qMin(pos, displayOffsets.size() - 1)];
}

static int adjustPosFromDisplay(const QList<ushort>& displayOffsets, int pos)
{
    if (displayOffsets.isEmpty()) {
        return pos;
    }

    // We want to find the first `i` such that `i + displayOffsets[i] >= pos`.
    for (qsizetype i = 0; i < displayOffsets.size(); i++) {
        if (i + displayOffsets[i] >= pos) {
            return i;
        }
    }

    // If we are here, pos is after the the last character in block
    return displayOffsets.size();
}

EditorLayout::EditorLayout(QTextDocument* document, CodeModel* codeModel)
    : QAbstractTextDocumentLayout(document)
    , d_codeModel(codeModel)
    , d_cursorWidth(1)
    , d_documentSize(0, 0)
{
}

QSizeF EditorLayout::documentSize() const
{
    return d_documentSize;
}

int EditorLayout::pageCount() const
{
    // Breaking source code to pages is meaningless
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

    auto* layoutData = BlockData::get<LayoutBlockData>(block);
    if (layoutData == nullptr) {
        return QRectF();
    }

    QTextLayout* layout = layoutData->displayLayout
        ? layoutData->displayLayout.get()
        : block.layout();

    QRectF rect = layout->boundingRect();
    rect.moveTopLeft(layout->position());
    return rect;
}

int EditorLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const
{
    if (accuracy == Qt::ExactHit && point.y() <= document()->documentMargin()) {
        return -1;
    }

    QTextBlock block = findContainingBlock(point.y());
    if (!block.isValid()) {
        if (accuracy == Qt::ExactHit) {
            return -1;
        }
        block = document()->lastBlock();
    }

    LayoutBlockData* layoutData = BlockData::get<LayoutBlockData>(block);
    QTextLayout* layout = layoutData->displayLayout
        ? layoutData->displayLayout.get()
        : block.layout();

    QPointF blockPoint = point - layout->position();

    int lineCount = layout->lineCount();
    for (int i = 0; i < lineCount; i++) {
        QTextLine line = layout->lineAt(i);
        if (!line.rect().contains(blockPoint) && i < lineCount - 1) {
            continue;
        }

        if (accuracy == Qt::ExactHit && !line.naturalTextRect().contains(blockPoint)) {
            return -1;
        }

        int pos = line.xToCursor(point.x());
        if (pos >= 0) {
            return block.position() + adjustPosFromDisplay(layoutData->displayOffsets, pos);
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
    qreal maxX = document()->textWidth() - document()->documentMargin() + d_cursorWidth;
    clip.setRight(qMin(clip.right(), maxX));

    QTextBlock block = findContainingBlock(clip.top());

    while (block.isValid()) {
        LayoutBlockData* layoutData = BlockData::get<LayoutBlockData>(block);
        QTextLayout* layout = layoutData->displayLayout
            ? layoutData->displayLayout.get()
            : block.layout();

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
                fmt.start = adjustPosToDisplay(layoutData->displayOffsets, start);
                fmt.length = adjustPosToDisplay(layoutData->displayOffsets, end) - fmt.start;
                fmt.format = sel.format;
                formats.append(fmt);
            }
            else if (!sel.cursor.hasSelection()
                        && sel.format.boolProperty(QTextFormat::FullWidthSelection)
                        && block.contains(sel.cursor.position())) {
                int posInBlock = adjustPosToDisplay(layoutData->displayOffsets, sel.cursor.position() - blockStart);
                QTextLine line = layout->lineForTextPosition(posInBlock);
                Q_ASSERT(line.isValid());

                QTextLayout::FormatRange fmt;
                fmt.start = line.textStart();
                fmt.length = line.textLength();
                fmt.format = sel.format;

                if (line.lineNumber() == layout->lineCount() - 1) {
                    // For the last line in a block, we must ensure that the
                    // selection ends after the block
                    fmt.length++;
                }
                formats.append(fmt);
            }
        }

        layout->draw(painter, QPointF(), formats, clip);

        if (block.contains(context.cursorPosition)) {
            int cursorPosInBlock = context.cursorPosition - block.position();

            painter->setPen(context.palette.color(QPalette::Text));
            layout->drawCursor(painter,
                QPointF(),
                adjustPosToDisplay(layoutData->displayOffsets, cursorPosInBlock),
                d_cursorWidth);
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
        QRectF prevBoundingRect = blockBoundingRect(startBlock.previous());
        y = prevBoundingRect.y() + prevBoundingRect.height();
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

        LayoutBlockData* layoutData = BlockData::get<LayoutBlockData>(block);
        if (layoutData->displayLayout) {
            layoutData->displayLayout->setPosition(newPos);
            y += layoutData->displayLayout->boundingRect().height();
        }
        else {
            y += layout->boundingRect().height();
        }
    }

    recalculateDocumentSize();
    Q_EMIT update();
}

static void buildDisplayLayout(const QTextBlock& block, LayoutBlockData* blockData)
{
    QString text = block.text();
    size_t newHash = qHash(text);

    // Calculating a display layout is expensive, only do it if content actually changed
    if (newHash == blockData->textHash) {
        return;
    }
    blockData->textHash = newHash;

    IsolatesBlockData* isolateData = BlockData::get<IsolatesBlockData>(block);
    if (isolateData == nullptr || isolateData->isolates().isEmpty()) {
        blockData->displayOffsets.clear();
        blockData->displayLayout.reset();
        return;
    }

    QTextLayout* defaultLayout = block.layout();
    QList<QTextLayout::FormatRange> formats = defaultLayout->formats();
    parsing::IsolateRangeList isolates = isolateData->isolates();

    // Inject Unicode BiDi control characters the block's text for the needed
    // isolate ranges, creating a different text that is used for shaping but
    // not edit purposes. This moves around positions of visible characters
    // relative to the editable block content stored in the QTextDocument, so
    // we need to save an offset mapping.

    auto& offsets = blockData->displayOffsets;
    offsets.resize(text.size());
    offsets.fill(0);

    QMap<int, int> isolatesStartingAt;
    QMap<int, int> isolatesEndingAt;

    for (const auto& isolate : isolates) {
        QChar startChar;
        switch (isolate.dir) {
            case Qt::LeftToRight: startChar = utils::LRI_MARK; break;
            case Qt::RightToLeft: startChar = utils::RLI_MARK; break;
            case Qt::LayoutDirectionAuto: startChar = utils::FSI_MARK; break;
        }

        int start = static_cast<int>(isolate.startPos);
        int end = static_cast<int>(isolate.endPos);

        text.insert(start + offsets[start], startChar);
        text.insert(end + offsets[end] + 2, utils::PDI_MARK);

        for (int i = start; i <= end; i++) {
            offsets[i] += 1;
        }
        for (int i = end + 1; i < offsets.length(); i++) {
            offsets[i] += 2;
        }

        isolatesStartingAt[start]++;
        isolatesEndingAt[end]++;
    }

    // Patch format ranges to be be correct for adjusted text
    for (QTextLayout::FormatRange& r : formats) {
        auto startOffset = offsets[r.start];
        r.length += (offsets[r.start + r.length - 1] - startOffset);
        r.start += startOffset;
    }

    // The Qt method QWidgetTextControlPrivate::rectForPosition directly
    // uses the QTextLine objects from the default layout to determine cursor
    // and selection rectangles position and height. It is essential that
    // our display layout's lines have the same bounding rectangles - otherwise
    // there are various visual glitches related to mouse selection.
    //
    // In theory, we only add invisible control characters which shouldn't affect
    // text size, BUT - due to them being control chars most fonts will (rightfully)
    // claim to not cover them. We must prevent Qt from falling back to another font
    // that might claim coverage, and has a bigger ascent or descent height than
    // any font used for visible characters. To achieve that, add a format
    // for each control char we injected that prevents font merging.
    QTextLayout::FormatRange r;
    r.format.setFontStyleStrategy(QFont::NoFontMerging);

    for (const auto& [pos, count] : isolatesStartingAt.asKeyValueRange()) {
        r.start = pos + offsets[pos] - count;
        r.length = count;
        formats.append(r);
    }
    for (const auto& [pos, count] : isolatesEndingAt.asKeyValueRange()) {
        r.start = pos + offsets[pos] + 1;
        r.length = count;
        formats.append(r);
    }

    blockData->displayLayout = std::make_unique<QTextLayout>(text);
    blockData->displayLayout->setFormats(formats);
}

void EditorLayout::layoutBlock(QTextBlock& block, qreal topY)
{
    LayoutBlockData* blockData = BlockData::get<LayoutBlockData>(block);
    if (!blockData) {
        blockData = new LayoutBlockData();
        BlockData::set<LayoutBlockData>(block, blockData);
    }

    Qt::LayoutDirection dir = getBlockDirection(block);

    QTextOption option = document()->defaultTextOption();
    option.setTextDirection(dir);
    option.setAlignment(QStyle::visualAlignment(dir, option.alignment()));

    QTextLayout* defaultLayout = block.layout();
    doBlockLayout(defaultLayout, option, topY);
    block.setLineCount(defaultLayout->lineCount());

    buildDisplayLayout(block, blockData);

    QTextLayout* displayLayout = blockData->displayLayout.get();
    if (displayLayout != nullptr) {
        displayLayout->setFont(document()->defaultFont());
        displayLayout->setCursorMoveStyle(document()->defaultCursorMoveStyle());

        doBlockLayout(displayLayout, option, topY);

        if (displayLayout->boundingRect() != defaultLayout->boundingRect()) {
            qWarning() << "Block" << block.blockNumber() << "display bounding rect differs from default one!"
                << displayLayout->boundingRect() << "vs" << defaultLayout->boundingRect();
        }
    }
}

void EditorLayout::doBlockLayout(QTextLayout* layout, const QTextOption& option, qreal topY)
{
    qreal margin = document()->documentMargin();
    qreal availableWidth = document()->textWidth() - 2 * margin;

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
}

Qt::LayoutDirection EditorLayout::getBlockDirection(const QTextBlock& block)
{
    Qt::LayoutDirection dir = utils::naturalTextDirection(block.text());
    if (dir != Qt::LayoutDirectionAuto) {
        return dir;
    }

    QTextBlock matchingBlock = d_codeModel->findMatchingIndentBlock(block);
    if (matchingBlock != block) {
        return matchingBlock.layout()->textOption().textDirection();
    }

    if (d_codeModel->startsLeftLeaningSpan(block)) {
        return Qt::LeftToRight;
    }

    QTextBlock prevBlock = block.previous();
    if (prevBlock.isValid()) {
        return prevBlock.layout()->textOption().textDirection();
    }

    dir = qGuiApp->inputMethod()->inputDirection();
    if (dir != Qt::LayoutDirectionAuto) {
        return dir;
    }

    return qGuiApp->layoutDirection();
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
    qreal height = 2 * document()->documentMargin();

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        height += blockBoundingRect(block).height();
    }

    QSizeF newDocumentSize(document()->textWidth(), height);
    if (newDocumentSize != d_documentSize) {
        d_documentSize = newDocumentSize;
        Q_EMIT documentSizeChanged(newDocumentSize);
    }
}

}
