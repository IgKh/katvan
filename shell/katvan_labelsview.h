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
#pragma once

#include "typstdriver_engine.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QListView;
class QSortFilterProxyModel;
QT_END_NAMESPACE

namespace katvan {

class LabelsModel;

class LabelsView : public QWidget
{
    Q_OBJECT

public:
    LabelsView(QWidget* parent = nullptr);

public slots:
    void resetView();
    void labelsUpdated(QList<katvan::typstdriver::DocumentLabel> labels);

private slots:
    void itemActivated(const QModelIndex& index);

signals:
    void goToPosition(int blockNum, int charOffset);

private:
    LabelsModel* d_model;
    QSortFilterProxyModel* d_proxyModel;

    QLineEdit* d_filterEdit;
    QListView* d_listView;
};

}
