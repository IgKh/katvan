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
#include "typstdriver_outlinenode.h"

#include "typstdriver_ffi/bridge.h"

namespace katvan::typstdriver {

OutlineNode::OutlineNode()
    : d_level(0)
    , d_parent(nullptr)
{
}

OutlineNode::OutlineNode(std::span<OutlineEntry> entries)
    : d_level(0)
    , d_parent(nullptr)
{
    OutlineNode* parent = this;
    OutlineNode* previous = this;

    for (const OutlineEntry& entry : entries) {
        Q_ASSERT(entry.level > 0);

        QString title = QString::fromUtf8(entry.title.data(), entry.title.size());

        OutlineNode* node = new OutlineNode();
        node->d_level = entry.level;
        node->d_title = title;

        if (entry.has_position) {
            node->d_line = static_cast<int>(entry.position.line);
            node->d_column = static_cast<int>(entry.position.column);
        }

        if (entry.level > previous->level()) {
            parent = previous;
        }
        else if (entry.level <= parent->level()) {
            Q_ASSERT(previous->level() >= parent->level());
            while (entry.level <= parent->level()) {
                parent = parent->parent();
            }
        }
        parent->d_children.append(node);
        node->d_parent = parent;
        previous = node;
    }
}

OutlineNode::~OutlineNode()
{
    qDeleteAll(d_children);
}

}
