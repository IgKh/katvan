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

/*
 * This file is heavily adapted from Qt's qtwebengine:src/pdfwidgets/qpdfview.cpp
 *
 * Original copyrights:
 * Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias König <tobias.koenig@kdab.com>
 * Copyright (C) 2022 The Qt Company Ltd.
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 */

#include "katvan_coreutils.h"
#include "katvan_previewerview.h"
#include "katvan_typstdriverwrapper.h"

#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QScreen>
#include <QScrollBar>
#include <QScroller>
#include <QTimer>

namespace katvan {

static constexpr int PAGE_SPACING { 3 };
static constexpr QMargins DOCUMENT_MARGINS { 6, 6, 6, 6 };

PreviewerView::PreviewerView(TypstDriverWrapper* driver, QWidget* parent)
    : QAbstractScrollArea(parent)
    , d_zoomMode(ZoomMode::Custom)
    , d_zoomFactor(1.0)
    , d_currentPage(0)
    , d_invertColors(false)
    , d_driver(driver)
    , d_scrollerGestureUngrabbed(false)
{
    viewport()->setMouseTracking(true);

    connect(driver, &TypstDriverWrapper::pageRendered, this, &PreviewerView::pageRendered);
    connect(screen(), &QScreen::logicalDotsPerInchChanged, this, &PreviewerView::dpiChanged);
    connect(screen(), &QScreen::physicalDotsPerInchChanged, this, &PreviewerView::invalidateAllRenderCache);

    verticalScrollBar()->setSingleStep(20);
    horizontalScrollBar()->setSingleStep(20);
    dpiChanged();

    d_invalidationTimer = new QTimer(this);
    d_invalidationTimer->setSingleShot(true);
    d_invalidationTimer->setInterval(100);
    d_invalidationTimer->callOnTimeout(this, &PreviewerView::invalidateAllRenderCache);

    d_wheelTracker = new utils::WheelTracker(this);
    connect(d_wheelTracker, &utils::WheelTracker::scrolled, this, &PreviewerView::zoomedByScrolling);

    QScroller::grabGesture(viewport(), QScroller::LeftMouseButtonGesture);

    QScroller* scroller = QScroller::scroller(viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(props);

    connect(scroller, &QScroller::stateChanged, this, &PreviewerView::scrollerStateChanged);
    scrollerStateChanged();
}

QString PreviewerView::pageLabel(int page) const
{
    if (d_pages.empty()) {
        return QString();
    }

    Q_ASSERT(page >= 0 && page < d_pages.size());
    return QString::number(d_pages[page].pageNumber);
}

qreal PreviewerView::effectiveZoom(int page) const
{
    if (d_zoomMode == ZoomMode::Custom) {
        return d_zoomFactor;
    }

    Q_ASSERT(page >= 0 && page < d_pages.size());
    QSizeF pageSize = d_pages[page].sizeInPoints;

    int displayWidth = viewport()->width()
        - DOCUMENT_MARGINS.left()
        - DOCUMENT_MARGINS.right();

    if (d_zoomMode == ZoomMode::FitToWidth) {
        return displayWidth / (pageSize.width() * d_pointSize);
    }
    else if (d_zoomMode == ZoomMode::FitToPage) {
        QSize displaySize { displayWidth, viewport()->height() - PAGE_SPACING };
        QSizeF scaled = (pageSize * d_pointSize).scaled(displaySize, Qt::KeepAspectRatio);

        return scaled.width() / (pageSize.width() * d_pointSize);
    }
    return 1.0;
}

void PreviewerView::setPages(QList<typstdriver::PreviewPageData> pages)
{
    bool hadPages = !d_pages.isEmpty();
    d_pages = pages;

    resetAllCalculations(false);

    if (pages.isEmpty()) {
        // Reset before loading a new document - discard the entire cache.
        d_renderCache.clear();
    }
    else if (hadPages) {
        // In case of new content in an already open preview - invalidate
        // only pages that have actually changed
        const QList<int> keys = d_renderCache.keys();
        for (int key : keys) {
            if (key >= pages.size()) {
                d_renderCache.remove(key);
            }
            else {
                CachedPage* page = d_renderCache.object(key);
                if (page->fingerprint != pages[key].fingerprint) {
                    page->invalidated = true;
                }
            }
        }
    }

    viewport()->update();
}

void PreviewerView::setZoomMode(ZoomMode mode)
{
    if (mode == d_zoomMode) {
        return;
    }

    d_zoomMode = mode;
    d_zoomFactor = 1.0;

    resetAllCalculations();
}

void PreviewerView::setCustomZoomFactor(qreal factor)
{
    if (factor <= 0.0) {
        return;
    }

    if (d_zoomMode != ZoomMode::Custom || factor != d_zoomFactor) {
        d_zoomMode = ZoomMode::Custom;
        d_zoomFactor = factor;

        resetAllCalculations();
    }
}

void PreviewerView::setInvertColors(bool value)
{
    if (d_invertColors == value) {
        return;
    }
    d_invertColors = value;

    invalidateAllRenderCache();
}

void PreviewerView::jumpTo(int page, QPointF pos)
{
    if (page < 0 || page >= d_pageGeometries.size()) {
        return;
    }

    qreal zoom = effectiveZoom(page);
    QPointF target = d_pageGeometries[page].topLeft() + d_pointSize * zoom * pos;

    // Try to aim for placing the jump point just above the middle of the
    // viewport. Calculate the desired visibility rectangle ourselves, since
    // QScroller doesn't know the total preview size and does stupid things
    // because of it.
    qreal xMargin = 0.45 * viewport()->width();
    qreal yMargin = 0.45 * viewport()->height();

    QRect marginedRect {
        QPoint {
            qMax(qRound(target.x() - xMargin), DOCUMENT_MARGINS.left()),
            qMax(qRound(target.y() - yMargin), DOCUMENT_MARGINS.top())
        },
        QPoint{
            qMin(qRound(target.x() + xMargin), d_documentSize.width() - DOCUMENT_MARGINS.right()),
            qMin(qRound(target.y() + yMargin), d_documentSize.height() - DOCUMENT_MARGINS.bottom())
        },
    };

    QScroller::scroller(viewport())->ensureVisible(marginedRect, 0, 0);

#ifdef DEBUG_JUMP_POINT
    d_lastJumpPoint = target;
    viewport()->update();
#endif
}

void PreviewerView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);

    if (d_zoomMode != ZoomMode::Custom) {
        resetAllCalculations();
    }
    else {
        updatePageGeometries();
        viewport()->update();
    }
}

void PreviewerView::paintEvent(QPaintEvent* event)
{
    QPainter painter { viewport() };

    QRect viewportRect {
        horizontalScrollBar()->value(),
        verticalScrollBar()->value(),
        viewport()->width(),
        viewport()->height()
    };

    painter.fillRect(event->rect(), palette().brush(QPalette::Dark));
    painter.translate(-viewportRect.x(), -viewportRect.y());

    bool paintedSomething = false;

    for (qsizetype i = 0; i < d_pageGeometries.size(); i++) {
        const QRect& pageGeometry = d_pageGeometries[i];

        if (!pageGeometry.intersects(viewportRect)) {
            if (!paintedSomething) {
                continue;
            }
            else {
                break;
            }
        }
        paintedSomething = true;

        painter.fillRect(pageGeometry, d_invertColors ? Qt::black : Qt::white);

        CachedPage* renderedPage = d_renderCache.object(i);
        if (renderedPage != nullptr) {
            painter.drawImage(pageGeometry, renderedPage->image);
        }
        if (renderedPage == nullptr || renderedPage->invalidated) {
            d_driver->renderPage(i, d_pointSize * effectiveZoom(i) * devicePixelRatio());
        }
    }

#ifdef DEBUG_JUMP_POINT
    if (!d_lastJumpPoint.isNull()) {
        painter.fillRect(QRectF(d_lastJumpPoint.x() - 5, d_lastJumpPoint.y() - 5, 10, 10), Qt::blue);
    }
#endif
}

void PreviewerView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        d_scrollerGestureUngrabbed = true;
        QScroller::ungrabGesture(viewport());

        if (viewport()->cursor().shape() != Qt::ArrowCursor) {
            viewport()->setCursor(Qt::ArrowCursor);
        }
    }
    else if (d_scrollerGestureUngrabbed) {
        d_scrollerGestureUngrabbed = false;
        QScroller::grabGesture(viewport(), QScroller::LeftMouseButtonGesture);
        scrollerStateChanged();
    }
}

void PreviewerView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->modifiers() != Qt::ControlModifier || event->button() != Qt::LeftButton) {
        return;
    }

    QPoint click {
        horizontalScrollBar()->value() + event->pos().x(),
        verticalScrollBar()->value() + event->pos().y()
    };

    int page = -1;
    for (qsizetype i = 0; i < d_pageGeometries.size(); i++) {
        if (d_pageGeometries[i].contains(click)) {
            page = i;
            break;
        }
    }
    if (page < 0) {
        return;
    }

    double zoom = effectiveZoom(page);
    QPoint posInPage = click - d_pageGeometries[page].topLeft();
    QPointF posInPts = posInPage.toPointF() / (d_pointSize * zoom);
    d_driver->inverseSearch(page, posInPts);
}

void PreviewerView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        d_wheelTracker->processEvent(event);
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void PreviewerView::scrollContentsBy(int dx, int dy)
{
    QAbstractScrollArea::scrollContentsBy(dx, dy);
    updateCurrentPage();
}

static void invertPageColors(QImage& image)
{
    static QHash<QRgb, QRgb> cache;

    int height = image.height();
    int width = image.width();

    for (int i = 0; i < height; i++) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(i));
        for (int j = 0; j < width; j++) {
            QRgb* pixel = &line[j];
            QRgb result;

            auto it = cache.find(*pixel);
            if (it != cache.end()) {
                result = it.value();
            }
            else {
                QColor color = QColor::fromRgba(*pixel).toHsl();
                color.setHsl(
                    color.hue(),
                    color.saturation(),
                    255 - color.lightness(),
                    color.alpha());

                result = color.rgba();
                cache[*pixel] = result;
            }
            *pixel = result;
        }
    }
}

void PreviewerView::pageRendered(int page, QImage image)
{
    if (page >= d_pages.size()) {
        return;
    }

    if (d_invertColors) {
        invertPageColors(image);
    }

    // When asking to render a page we give typst-render a higher DPI that
    // compensates for the gap between the page geometry in logical pixels
    // and and the scaled display pixels. Tell Qt that so it won't re-scale
    // the page image and introduce artifacts.
    image.setDevicePixelRatio(devicePixelRatio());

    d_renderCache.insert(page, new CachedPage { d_pages[page].fingerprint, image });
    viewport()->update();
}

void PreviewerView::dpiChanged()
{
    // A point is 1/72th of an inch
    d_pointSize = screen()->logicalDotsPerInch() / 72.0;

    updatePageGeometries();
}

void PreviewerView::scrollerStateChanged()
{
    if (d_scrollerGestureUngrabbed) {
        return;
    }

    QScroller::State state = QScroller::scroller(viewport())->state();

    if (state == QScroller::Inactive || state == QScroller::Scrolling) {
        if (viewport()->cursor().shape() != Qt::OpenHandCursor) {
            viewport()->setCursor(Qt::OpenHandCursor);
        }
    }
    else {
        if (viewport()->cursor().shape() != Qt::ClosedHandCursor) {
            viewport()->setCursor(Qt::ClosedHandCursor);
        }
    }
}

void PreviewerView::invalidateAllRenderCache()
{
    const QList<int> pages = d_renderCache.keys();
    for (int page : pages) {
        d_renderCache.object(page)->invalidated = true;
    }
    viewport()->update();
}

void PreviewerView::resetAllCalculations(bool invalidateRenderCache)
{
    updatePageGeometries();
    updateCurrentPage();

    if (invalidateRenderCache) {
        // If there wasn't a content change, but just a geometry change,
        // keep the rendered pages but mark them for refresh. The idea
        // is that until the pages are re-rendered we'll scale the
        // existing bitmaps to the new size to avoid too much flicker,
        // and then the sharper images will replace them. Do it via a
        // debouncer to ensure that we don't re-render too much when e.g
        // resizing the previewer pane.
        d_invalidationTimer->start();
    }
}

void PreviewerView::calculatePageGeometries()
{
    qsizetype pageCount = d_pages.size();
    int totalWidth = DOCUMENT_MARGINS.left() + DOCUMENT_MARGINS.right();

    d_pageGeometries.resize(pageCount);
    for (int i = 0; i < pageCount; i++) {
        QSizeF pageSizeInPoints = d_pages[i].sizeInPoints;
        qreal zoom = effectiveZoom(i);

        qreal pageWidth = pageSizeInPoints.width() * zoom * d_pointSize;
        qreal pageHeight = pageSizeInPoints.height() * zoom * d_pointSize;

        d_pageGeometries[i] = QRect(
            QPoint(0, 0),
            QSizeF(pageWidth, pageHeight).toSize());

        totalWidth = qMax(totalWidth, qRound(DOCUMENT_MARGINS.left() + pageWidth + DOCUMENT_MARGINS.right()));
    }

    int topY = DOCUMENT_MARGINS.top();
    int effectiveWidth = qMax(totalWidth, viewport()->width());

    for (int i = 0; i < pageCount; i++) {
        QRect& pageGeometry = d_pageGeometries[i];

        // Center page in viewport
        int pageX = (effectiveWidth - pageGeometry.width()) / 2;

        pageGeometry.moveTopLeft(QPoint(pageX, topY));
        topY += pageGeometry.height() + PAGE_SPACING;
    }

    topY += DOCUMENT_MARGINS.bottom();
    d_documentSize = QSize(totalWidth, topY);
}

void PreviewerView::updateScrollbars(QSize documentSize, bool forceVerticalScrollBar)
{
    int verticalMax = documentSize.height() - viewport()->height();
    if (verticalMax <= 0 && forceVerticalScrollBar) {
        verticalMax = 1;
    }

    horizontalScrollBar()->setRange(0, documentSize.width() - viewport()->width());
    horizontalScrollBar()->setPageStep(viewport()->width());
    verticalScrollBar()->setRange(0, verticalMax);
    verticalScrollBar()->setPageStep(viewport()->height());
}

void PreviewerView::updatePageGeometries()
{
    calculatePageGeometries();
    bool forceVerticalScrollBar = false;

    if (d_zoomMode != ZoomMode::Custom) {
        if (verticalScrollBar()->isVisible() && d_documentSize.height() <= viewport()->height()) {
            // Vertical scroll bar was needed, but not anymore. If increasing
            // the viewport width by that of a scroll bar would cause the
            // document to lengthen such that a scroll bar would again be
            // needed, it would lead us to a resize loop, so just force the
            // scroll bar to keep being visible.

            qreal factor = (viewport()->width() + verticalScrollBar()->width()) / qreal(viewport()->width());
            if (qRound(d_documentSize.height() * factor) > viewport()->height()) {
                forceVerticalScrollBar = true;
            }
        }
    }
    updateScrollbars(d_documentSize, forceVerticalScrollBar);
}

void PreviewerView::updateCurrentPage()
{
    // As in the QPdfView implementation, use a 2px height scan line located at
    // 40% of viewport height. The first page that intersects that line, is the
    // current page.
    QRect scanLine {
        horizontalScrollBar()->value(),
        verticalScrollBar()->value() + qRound(0.4 * viewport()->height()),
        viewport()->width(),
        2
    };

    int currentPage = 0;
    for (qsizetype i = 0; i < d_pageGeometries.size(); i++) {
        if (d_pageGeometries[i].intersects(scanLine)) {
            currentPage = i;
            break;
        }
    }

    if (currentPage != d_currentPage) {
        d_currentPage = currentPage;
        Q_EMIT currentPageChanged(currentPage);
    }
}

}

#include "moc_katvan_previewerview.cpp"
