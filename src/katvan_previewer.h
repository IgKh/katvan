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

#include "katvan_typstdriverwrapper.h"

#include <QList>
#include <QSettings>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
QT_END_NAMESPACE

namespace katvan {

class PreviewerView;

class Previewer : public QWidget
{
    Q_OBJECT

public:
    Previewer(TypstDriverWrapper* driver, QWidget* parent = nullptr);
    ~Previewer();

    void restoreSettings(const QSettings& settings);
    void saveSettings(QSettings& settings);

public slots:
    void reset();

private slots:
    void updatePreview(QList<typstdriver::PreviewPageData> pages);
    void zoomIn();
    void zoomOut();
    void zoomOptionSelected(int index);
    void manualZoomEntered();
    void currentPageChanged(int page);

private:
    void setZoom(QVariant value);
    void setCustomZoom(qreal factor);

private:
    PreviewerView* d_view;

    QComboBox* d_zoomComboBox;
    QLabel* d_currentPageLabel;
};

}
