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

#include <QSettings>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPdfDocument;
class QPdfView;
QT_END_NAMESPACE

namespace katvan {

class Previewer : public QWidget
{
    Q_OBJECT

public:
    Previewer(QWidget* parent = nullptr);
    ~Previewer();

    void restoreSettings(const QSettings& settings);
    void saveSettings(QSettings& settings);

public slots:
    void reset();
    bool updatePreview(const QString& pdfFile);

private slots:
    void zoomIn();
    void zoomOut();
    void zoomOptionSelected(int index);
    void manualZoomEntered();
    void currentPageChanged(int page);

private:
    qreal effectiveZoom();
    void setZoom(QVariant value);
    void setCustomZoom(qreal factor);

private:
    QPdfView* d_pdfView;
    QPdfDocument* d_previewDocument;

    QComboBox* d_zoomComboBox;
    QLabel* d_currentPageLabel;
};

}
