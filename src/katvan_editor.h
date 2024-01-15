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

#include <QTextEdit>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class Editor : public QTextEdit
{
    Q_OBJECT

public:
    Editor(QWidget* parent = nullptr);

public slots:
    void insertInlineMath();

signals:
    void contentModified(const QString& text);

private:
    QTimer* d_debounceTimer;
};

}
