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
#include "katvan_compileroutput.h"
#include "katvan_diagnosticsmodel.h"

#include <QFontDatabase>
#include <QHeaderView>

namespace katvan {

CompilerOutput::CompilerOutput(QWidget* parent)
    : QTreeView(parent)
{
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    header()->setStretchLastSection(true);

    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLayoutDirection(Qt::LeftToRight);

    connect(this, &QTreeView::activated, this, &CompilerOutput::itemActivated);
}

void CompilerOutput::adjustColumnWidths()
{
    int severityWidth = 22;
    int locationWidth = fontMetrics().horizontalAdvance("0000:000");
    int availableWidthForFileName = viewport()->width() - severityWidth - locationWidth;
    int fileNameSizeHint = sizeHintForColumn(DiagnosticsModel::COLUMN_FILE);

    // If there is enough space for fully accomodate the file name, give it that
    // much width. Otherwise, give it 25% of the total width.
    int fileNameWidth = (fileNameSizeHint <= availableWidthForFileName)
        ? fileNameSizeHint
        : qRound(viewport()->width() / 4.0);

    setColumnWidth(DiagnosticsModel::COLUMN_SEVERITY, severityWidth);
    setColumnWidth(DiagnosticsModel::COLUMN_FILE, fileNameWidth);
    setColumnWidth(DiagnosticsModel::COLUMN_SOURCE_LOCATION, locationWidth);
}

void CompilerOutput::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);
    adjustColumnWidths();
}

void CompilerOutput::itemActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    DiagnosticsModel* dmodel = qobject_cast<DiagnosticsModel*>(model());

    auto result = dmodel->getSourceLocation(index);
    if (result) {
        const auto [line, column] = *result;
        Q_EMIT goToPosition(line - 1, column);
    }
}

}
