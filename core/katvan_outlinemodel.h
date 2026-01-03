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

#include "typstdriver_outlinenode.h"

#include <QAbstractItemModel>

#include <memory>

namespace katvan {

class OutlineModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static constexpr int POSITION_LINE_ROLE = Qt::UserRole + 1;
    static constexpr int POSITION_COLUMN_ROLE = Qt::UserRole + 2;

    OutlineModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    bool isRightToLeft() const;
    QModelIndex indexForDocumentLine(int line) const;

public slots:
    void setOutline(katvan::typstdriver::OutlineNode* outline);

private:
    typstdriver::OutlineNode* indexToNode(const QModelIndex& index) const;

    std::unique_ptr<typstdriver::OutlineNode> d_root;
};

}
