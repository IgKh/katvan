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

#include "typstdriver_export.h"

#include <QList>
#include <QString>

#include <optional>
#include <span>

namespace katvan::typstdriver {

class OutlineEntry;

class TYPSTDRIVER_EXPORT OutlineNode
{
public:
    OutlineNode();
    OutlineNode(std::span<OutlineEntry> entries);
    ~OutlineNode();

    OutlineNode(const OutlineNode&) = delete;
    OutlineNode& operator=(const OutlineNode&) = delete;

    int level() const { return d_level; }
    QString title() const { return d_title; }
    std::optional<int> line() const { return d_line; }
    std::optional<int> column() const { return d_column; }
    OutlineNode* parent() const { return d_parent; }
    QList<OutlineNode*> children() const { return d_children; }

private:
    int d_level;
    QString d_title;
    std::optional<int> d_line;
    std::optional<int> d_column;

    OutlineNode* d_parent;
    QList<OutlineNode*> d_children;
};

}
