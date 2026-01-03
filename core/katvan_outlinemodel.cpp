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

#include "katvan_outlinemodel.h"

namespace katvan {

OutlineModel::OutlineModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

void OutlineModel::setOutline(typstdriver::OutlineNode* outline)
{
    beginResetModel();
    d_root.reset(outline);
    endResetModel();
}

typstdriver::OutlineNode* OutlineModel::indexToNode(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return d_root.get();
    }
    return reinterpret_cast<typstdriver::OutlineNode*>(index.internalPointer());
}

QModelIndex OutlineModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_ASSERT(checkIndex(parent));

    if (column != 0) {
        return QModelIndex();
    }

    auto* node = indexToNode(parent);
    if (row < 0 || row >= node->children().size()) {
        return QModelIndex();
    }

    return createIndex(row, column, node->children().at(row));
}

QModelIndex OutlineModel::parent(const QModelIndex& child) const
{
    Q_ASSERT(checkIndex(child, QAbstractItemModel::CheckIndexOption::DoNotUseParent));

    auto* node = indexToNode(child);
    auto* parent = node->parent();
    if (node->parent() == nullptr) {
        // child is the root
        return QModelIndex();
    }

    auto* grandparent = parent->parent();
    if (grandparent == nullptr) {
        // parent is the root
        return createIndex(0, 0, parent);
    }

    // Find row of the parent within its' own parent
    int row = grandparent->children().indexOf(parent);
    if (row < 0) {
        return QModelIndex();
    }

    return createIndex(row, 0, parent);
}

int OutlineModel::rowCount(const QModelIndex& parent) const
{
    Q_ASSERT(checkIndex(parent));

    auto* node = indexToNode(parent);
    if (node == nullptr) {
        return 0;
    }
    return node->children().count();
}

int OutlineModel::columnCount(const QModelIndex& parent) const
{
    Q_ASSERT(checkIndex(parent));

    return 1;
}

QVariant OutlineModel::data(const QModelIndex& index, int role) const
{
    Q_ASSERT(checkIndex(index));

    if (index.column() != 0) {
        return QVariant();
    }

    auto* node = indexToNode(index);

    if (role == Qt::DisplayRole) {
        return node->title();
    }
    else if (role == POSITION_LINE_ROLE) {
        if (node->line()) {
            return node->line().value();
        }
    }
    else if (role == POSITION_COLUMN_ROLE) {
        if (node->column()) {
            return node->column().value();
        }
    }
    return QVariant();
}

static const typstdriver::OutlineNode* getOutlineNodeForDocumentLine(const typstdriver::OutlineNode* parent, int line)
{
    if (!parent) {
        return nullptr;
    }
    const typstdriver::OutlineNode* result = nullptr;

    const QList<typstdriver::OutlineNode*> children = parent->children();
    for (const auto* node : children) {
        if (node->line() > line) {
            break;
        }
        result = node;
    }

    if (result) {
        const typstdriver::OutlineNode* subtreeResult = getOutlineNodeForDocumentLine(result, line);
        if (subtreeResult) {
            result = subtreeResult;
        }
    }
    return result;
}

QModelIndex OutlineModel::indexForDocumentLine(int line) const
{
    const auto* node = getOutlineNodeForDocumentLine(d_root.get(), line);
    if (!node) {
        return QModelIndex();
    }

    int row = node->parent()->children().indexOf(node);
    if (row < 0) {
        return QModelIndex();
    }

    return createIndex(row, 0, node);
}

bool OutlineModel::isRightToLeft() const
{
    // Determine this based on plurality of top level heading titles
    if (!d_root) {
        return false;
    }

    int rtlCount = 0, ltrCount = 0;

    const QList<typstdriver::OutlineNode*> children = d_root->children();
    for (const auto* child : children) {
        if (child->title().isRightToLeft()) {
            rtlCount++;
        }
        else {
            ltrCount++;
        }
    }

    return (rtlCount > 0 && rtlCount >= ltrCount);
}

}

#include "moc_katvan_outlinemodel.cpp"
