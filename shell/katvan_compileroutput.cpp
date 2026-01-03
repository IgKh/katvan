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
#include "katvan_compileroutput.h"

#include "katvan_diagnosticsmodel.h"

#include <QHeaderView>
#include <QResizeEvent>
#include <QStyledItemDelegate>

namespace katvan {

class DiagnosticsItemDelegate : public QStyledItemDelegate {
public:
    DiagnosticsItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);

        // Not interested in individual focus rects on columns
        option->state &= ~QStyle::State_HasFocus;
    }
};

CompilerOutput::CompilerOutput(QWidget* parent)
    : QTreeView(parent)
{
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setItemDelegate(new DiagnosticsItemDelegate(this));
    setLayoutDirection(Qt::LeftToRight);
    setMouseTracking(true);
    header()->setStretchLastSection(false);

    connect(this, &QTreeView::clicked, this, &CompilerOutput::indexClicked);
}

void CompilerOutput::setModel(QAbstractItemModel* model)
{
    Q_ASSERT(qobject_cast<DiagnosticsModel*>(model) != nullptr);

    QTreeView::setModel(model);

    header()->setSectionResizeMode(DiagnosticsModel::COLUMN_SEVERITY, QHeaderView::Fixed);
    header()->setSectionResizeMode(DiagnosticsModel::COLUMN_MESSAGE, QHeaderView::Stretch);
    header()->setSectionResizeMode(DiagnosticsModel::COLUMN_SOURCE_LOCATION, QHeaderView::Fixed);
}

void CompilerOutput::adjustColumnWidths()
{
    adjustColumnWidths(viewport()->size());
}

void CompilerOutput::adjustColumnWidths(QSize viewportSize)
{
    int severityWidth = 22;
    int messageSizeHint = sizeHintForColumn(DiagnosticsModel::COLUMN_MESSAGE);
    int locationSizeHint = sizeHintForColumn(DiagnosticsModel::COLUMN_SOURCE_LOCATION);
    int availableWidthForLocation = viewportSize.width() - severityWidth - messageSizeHint;

    // If there is enough space to fully accommodate the file name, give it that
    // much width. Otherwise, give it up to 25% of the total width.
    int locationWidth = (locationSizeHint <= availableWidthForLocation)
        ? locationSizeHint
        : qMin(qRound(viewportSize.width() / 4.0), locationSizeHint);

    setColumnWidth(DiagnosticsModel::COLUMN_SEVERITY, severityWidth);
    setColumnWidth(DiagnosticsModel::COLUMN_SOURCE_LOCATION, locationWidth);
}

void CompilerOutput::resizeEvent(QResizeEvent* event)
{
    if (!model()) {
        return;
    }

    adjustColumnWidths(event->size());
    QTreeView::resizeEvent(event);
}

void CompilerOutput::mouseMoveEvent(QMouseEvent* event)
{
    QTreeView::mouseMoveEvent(event);

    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        QVariant cursor = index.data(DiagnosticsModel::ROLE_MOUSE_CURSOR);
        if (cursor.isValid()) {
            setCursor(qvariant_cast<QCursor>(cursor));
            return;
        }
    }
    setCursor(Qt::ArrowCursor);
}

void CompilerOutput::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        clearSelection();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void CompilerOutput::indexClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    DiagnosticsModel* dmodel = qobject_cast<DiagnosticsModel*>(model());

    auto result = dmodel->getSourceLocation(index);
    if (result) {
        const auto [line, column] = *result;
        Q_EMIT goToPosition(line, column);
    }
}

}

#include "moc_katvan_compileroutput.cpp"
