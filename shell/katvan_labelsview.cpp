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
#include "katvan_labelsview.h"
#include "katvan_utils.h"

#include "katvan_constants.h"

#include <QAbstractListModel>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

namespace katvan {

enum LabelsModelRoles {
    POSITION_LINE_ROLE = Qt::UserRole + 1,
    POSITION_COLUMN_ROLE,
};

LabelsModel::LabelsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void LabelsModel::setLabels(QList<katvan::typstdriver::DocumentLabel> labels)
{
    beginResetModel();
    d_labels = labels;
    endResetModel();
}

int LabelsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return d_labels.size();
}

Qt::ItemFlags LabelsModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

    if (index.isValid()) {
        defaultFlags |= Qt::ItemIsDragEnabled;
    }
    return defaultFlags;
}

QVariant LabelsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= d_labels.size()) {
        return QVariant();
    }

    const auto& [name, line, column] = d_labels[index.row()];

    switch (role) {
        case Qt::DisplayRole: return name;
        case Qt::DecorationRole:
            if (line >= 0 && column >= 0) {
                return utils::themeIcon("open-link");
            }
            else {
                return utils::themeIcon("tag");
            }
        case POSITION_LINE_ROLE: return line;
        case POSITION_COLUMN_ROLE: return column;
    }
    return QVariant();
}

Qt::DropActions LabelsModel::supportedDragActions() const
{
    return Qt::LinkAction;
}

QStringList LabelsModel::mimeTypes() const
{
    QStringList types;
    types << LABEL_REF_MIME_TYPE;
    return types;
}

QMimeData* LabelsModel::mimeData(const QModelIndexList& indexes) const
{
    QModelIndex index = indexes.value(0, QModelIndex());
    if (!index.isValid()) {
        return nullptr;
    }

    QString name = index.data().toString();

    QMimeData* data = new QMimeData();
    data->setData(LABEL_REF_MIME_TYPE, name.toUtf8());
    return data;
}

LabelsView::LabelsView(QWidget* parent)
    : QWidget(parent)
{
    d_model = new LabelsModel();

    d_proxyModel = new QSortFilterProxyModel();
    d_proxyModel->setSourceModel(d_model);

    d_listView = new QListView();
    d_listView->setModel(d_proxyModel);
    d_listView->setDragEnabled(true);

    connect(d_listView, &QListView::activated, this, &LabelsView::itemActivated);

    d_filterEdit = new QLineEdit();
    d_filterEdit->setPlaceholderText(tr("Filter"));

    connect(d_filterEdit, &QLineEdit::textEdited, d_proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(d_filterEdit);
    layout->addWidget(d_listView, 1);
}

void LabelsView::resetView()
{
    d_model->setLabels(QList<katvan::typstdriver::DocumentLabel>());
    d_proxyModel->setFilterFixedString(QString());
    d_filterEdit->clear();
}

void LabelsView::labelsUpdated(QList<katvan::typstdriver::DocumentLabel> labels)
{
    d_model->setLabels(labels);
    d_proxyModel->sort(0);
}

void LabelsView::itemActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    int line = index.data(POSITION_LINE_ROLE).toInt();
    int column = index.data(POSITION_COLUMN_ROLE).toInt();
    if (line >= 0 && column >= 0) {
        Q_EMIT goToPosition(line, column);
    }
}

}

#include "moc_katvan_labelsview.cpp"
