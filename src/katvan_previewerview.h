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

#include "typstdriver_engine.h"

#include <QAbstractScrollArea>
#include <QCache>
#include <QImage>
#include <QList>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
QT_END_NAMESPACE

namespace katvan {

class TypstDriverWrapper;

namespace typstdriver {
struct PreviewPageData;
}

class PreviewerView : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class ZoomMode {
        Custom,
        FitToWidth,
        FitToPage,
    };
    Q_ENUM(ZoomMode);

    PreviewerView(TypstDriverWrapper* driver, QWidget* parent = nullptr);

    int currentPage() const { return d_currentPage; }
    int pageCount() const { return d_pages.size(); }

    ZoomMode zoomMode() const { return d_zoomMode; }
    qreal zoomFactor() const { return d_zoomFactor; }

    QString pageLabel(int page) const;

    qreal effectiveZoom(int page) const;

signals:
    void currentPageChanged(int page);

public slots:
    void setPages(QList<typstdriver::PreviewPageData> pages);
    void setZoomMode(ZoomMode mode);
    void setZoomFactor(qreal zoom);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private slots:
    void pageRendered(int page, QImage image);
    void dpiChanged();
    void scrollerStateChanged();

private:
    void resetAllCalculations();
    void updatePageGeometries();
    void updateCurrentPage();
    void updateScrollbars();

    ZoomMode d_zoomMode;
    qreal d_zoomFactor;
    qreal d_pointSize;
    int d_currentPage;

    TypstDriverWrapper* d_driver;

    QList<typstdriver::PreviewPageData> d_pages;
    QList<QRect> d_pageGeometries;
    QCache<int, QImage> d_renderCache;
    QSize d_documentSize;
};

}
