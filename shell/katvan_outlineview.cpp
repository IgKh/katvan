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
#include "katvan_outlineview.h"

#include "katvan_outlinemodel.h"

namespace katvan {

OutlineView::OutlineView(QWidget* parent)
    : QTreeView(parent)
{
    d_model = new OutlineModel(this);
    setModel(d_model);

    setHeaderHidden(true);
    setUniformRowHeights(true);
    setExpandsOnDoubleClick(false);
    //setRootIsDecorated(false);

    connect(this, &QTreeView::activated, this, &OutlineView::itemActivated);
}

void OutlineView::outlineUpdated(typstdriver::OutlineNode* outline)
{
    d_model->setOutline(outline);

    expandToDepth(3);
    setLayoutDirection(d_model->isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);

    // TODOS - refresh correctly, reverse location selections
}

void OutlineView::itemActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    QVariant line = index.data(OutlineModel::POSITION_LINE_ROLE);
    QVariant column = index.data(OutlineModel::POSITION_COLUMN_ROLE);

    if (line.isNull() || column.isNull()) {
        return;
    }

    Q_EMIT goToPosition(line.toInt(), column.toInt());
}

}
