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

#include <QWidget>

QT_BEGIN_NAMESPACE
class QAbstractButton;
class QHBoxLayout;
class QLabel;
QT_END_NAMESPACE

namespace katvan {

class InfoBar : public QWidget
{
    Q_OBJECT

public:
    InfoBar(QWidget* parent = nullptr);

public slots:
    void clear();
    void addButton(QAbstractButton* button);
    void showMessage(const QString& message);

private:
    QLabel* d_icon;
    QLabel* d_message;
    QHBoxLayout* d_buttonLayout;
};

}
