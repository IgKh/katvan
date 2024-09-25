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
#pragma once

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>

namespace katvan {

class EditorLayout : public QAbstractTextDocumentLayout
{
    Q_OBJECT

public:
    EditorLayout(QTextDocument* document);

    QSizeF documentSize() const override;
    int pageCount() const override;
    QRectF frameBoundingRect(QTextFrame* frame) const override;
    QRectF blockBoundingRect(const QTextBlock& block) const override;
    int hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const override;
    void draw(QPainter* painter, const QAbstractTextDocumentLayout::PaintContext& context) override;

protected:
    void documentChanged(int position, int charsRemoved, int charsAdded) override;

private:
    void layoutBlock(QTextBlock& block, qreal topY);

    QTextBlock findContainingBlock(qreal y) const;
    void recalculateDocumentSize();

    QSizeF d_documentSize;
};

}
