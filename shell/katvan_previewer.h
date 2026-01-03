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

#include "katvan_typstdriverwrapper.h"

#include <QList>
#include <QSettings>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QAction;
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

    int currentPage() const;
    bool shouldFollowEditorCursor() const;

    void restoreSettings(const QSettings& settings);
    void saveSettings(QSettings& settings);

signals:
    void followCursorEnabled();

public slots:
    void reset();
    void jumpToPreview(int page, QPointF pos);

private slots:
    void updatePreview(QList<katvan::typstdriver::PreviewPageData> pages);
    void zoomIn();
    void zoomOut();
    void zoomOptionSelected(int index);
    void manualZoomEntered();
    void zoomedByScrolling(int units);
    void currentPageChanged(int page);
    void followEditorCursorChanged(bool checked);

private:
    void setZoom(QVariant value);
    void setCustomZoom(qreal factor);

private:
    PreviewerView* d_view;

    QComboBox* d_zoomComboBox;
    QLabel* d_currentPageLabel;
    QAction* d_invertColorsAction;
    QAction* d_followEditorCursorAction;
};

}
