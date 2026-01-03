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

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class CodeModel;

class EditorLayout : public QAbstractTextDocumentLayout
{
    Q_OBJECT

    // Undocumented property that QTextEdit (actually QWidgetTextControlPrivate)
    // wants to exist on document layouts
    Q_PROPERTY(int cursorWidth MEMBER d_cursorWidth)

public:
    EditorLayout(QTextDocument* document, CodeModel* codeModel);

    QSizeF documentSize() const override;
    int pageCount() const override;
    QRectF frameBoundingRect(QTextFrame* frame) const override;
    QRectF blockBoundingRect(const QTextBlock& block) const override;
    int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
    void draw(QPainter* painter, const QAbstractTextDocumentLayout::PaintContext& context) override;

    QTextBlock findContainingBlock(qreal y) const;
    QPointF cursorPositionPoint(int pos) const;
    int getLineEdgePosition(int pos, QTextLine::Edge edge) const;

signals:
    void fullRelayoutDone();

protected:
    void documentChanged(int position, int charsRemoved, int charsAdded) override;

private:
    void doDocumentLayout(const QTextBlock& startBlock, const QTextBlock& endBlock);
    void layoutBlock(QTextBlock& block, qreal topY);
    void doBlockLayout(QTextLayout* layout, const QTextOption& option, qreal wrappingIndentWidth, qreal topY);
    Qt::LayoutDirection getBlockDirection(const QTextBlock& block, const QString& blockText);
    qreal calculateIndentWidth(const QString& text, const QTextOption& option, bool inContent);
    void recalculateDocumentSize();

    CodeModel* d_codeModel;
    QTimer* d_fullLayoutDebounceTimer;

    int d_cursorWidth;
    QSizeF d_documentSize;
};

}
