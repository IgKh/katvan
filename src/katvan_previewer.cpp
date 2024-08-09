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
#include "katvan_previewer.h"

#include <QBuffer>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QScreen>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

static constexpr QLatin1StringView FIT_TO_PAGE("fit-page");
static constexpr QLatin1StringView FIT_TO_WIDTH("fit-width");

static constexpr QLatin1StringView SETTING_PREVIEW_ZOOM("preview/zoom");

namespace katvan {

Previewer::Previewer(QWidget* parent)
    : QWidget(parent)
{
    d_previewDocument = new QPdfDocument(this);
    d_buffer = new QBuffer(this);

    d_pdfView = new QPdfView(this);
    d_pdfView->setDocument(d_previewDocument);
    d_pdfView->setPageMode(QPdfView::PageMode::MultiPage);

    connect(d_pdfView->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this, &Previewer::currentPageChanged);

    d_zoomComboBox = new QComboBox();
    d_zoomComboBox->setEditable(true);
    d_zoomComboBox->setValidator(new QIntValidator(1, 999));
    d_zoomComboBox->setInsertPolicy(QComboBox::NoInsert);

    connect(d_zoomComboBox, &QComboBox::activated, this, &Previewer::zoomOptionSelected);
    connect(d_zoomComboBox->lineEdit(), &QLineEdit::returnPressed, this, &Previewer::manualZoomEntered);

    d_zoomComboBox->addItem(tr("Fit Page"), FIT_TO_PAGE);
    d_zoomComboBox->addItem(tr("Fit Width"), FIT_TO_WIDTH);
    d_zoomComboBox->insertSeparator(d_zoomComboBox->count());
    d_zoomComboBox->addItem("50%", "0.5");
    d_zoomComboBox->addItem("75%", "0.75");
    d_zoomComboBox->addItem("100%", "1.0");
    d_zoomComboBox->addItem("125%", "1.25");
    d_zoomComboBox->addItem("150%", "1.5");
    d_zoomComboBox->addItem("200%", "2.0");

    QToolButton* zoomOutButton = new QToolButton();
    zoomOutButton->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/icons/zoom-out.svg")));
    zoomOutButton->setToolTip(tr("Zoom Out Preview"));
    connect(zoomOutButton, &QToolButton::clicked, this, &Previewer::zoomOut);

    QToolButton* zoomInButton = new QToolButton();
    zoomInButton->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/icons/zoom-in.svg")));
    zoomInButton->setToolTip(tr("Zoom In Preview"));
    connect(zoomInButton, &QToolButton::clicked, this, &Previewer::zoomIn);

    d_currentPageLabel = new QLabel();

    QHBoxLayout* toolLayout = new QHBoxLayout();
    toolLayout->setContentsMargins(
        style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
        style()->pixelMetric(QStyle::PM_LayoutTopMargin),
        style()->pixelMetric(QStyle::PM_LayoutRightMargin),
        0);

    toolLayout->addWidget(zoomOutButton);
    toolLayout->addWidget(d_zoomComboBox);
    toolLayout->addWidget(zoomInButton);
    toolLayout->addStretch(1);
    toolLayout->addWidget(d_currentPageLabel);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolLayout);
    layout->addWidget(d_pdfView, 1);
}

Previewer::~Previewer()
{
    reset();
}

void Previewer::restoreSettings(const QSettings& settings)
{
    QVariant zoomValue = settings.value(SETTING_PREVIEW_ZOOM, FIT_TO_WIDTH);
    setZoom(zoomValue);

    int index = d_zoomComboBox->findData(zoomValue);
    if (index >= 0) {
        d_zoomComboBox->setCurrentIndex(index);
    }
}

void Previewer::saveSettings(QSettings& settings)
{
    QVariant zoomValue;
    switch (d_pdfView->zoomMode()) {
        case QPdfView::ZoomMode::Custom:
            zoomValue = d_pdfView->zoomFactor();
            break;
        case QPdfView::ZoomMode::FitInView:
            zoomValue = FIT_TO_PAGE;
            break;
        case QPdfView::ZoomMode::FitToWidth:
            zoomValue = FIT_TO_WIDTH;
            break;
    }

    settings.setValue(SETTING_PREVIEW_ZOOM, zoomValue);
}

void Previewer::reset()
{
    d_previewDocument->close();
    d_buffer->close();
    d_currentPageLabel->setText(QString());
}

bool Previewer::updatePreview(QByteArray pdfBuffer)
{
    int origY = d_pdfView->verticalScrollBar()->value();
    int origX = d_pdfView->horizontalScrollBar()->value();

    if (d_buffer->isOpen()) {
        d_buffer->close();
    }
    d_buffer->setData(pdfBuffer);
    d_buffer->open(QIODevice::ReadOnly);
    d_previewDocument->load(d_buffer);

    if (d_previewDocument->error() != QPdfDocument::Error::None) {
        QString err = QVariant::fromValue(d_previewDocument->error()).toString();
        QMessageBox::warning(
            window(),
            QCoreApplication::applicationName(),
            tr("Failed loading preview: %1").arg(err));

        return false;
    }

    d_pdfView->verticalScrollBar()->setValue(origY);
    d_pdfView->horizontalScrollBar()->setValue(origX);

    currentPageChanged(d_pdfView->pageNavigator()->currentPage());

    return true;
}

static qreal roundFactor(qreal factor)
{
    // Round to nearest multiple of 5%
    return qRound(factor * 20) / 20.0;
}

void Previewer::zoomIn()
{
    setCustomZoom(roundFactor(effectiveZoom()) + 0.05);
}

void Previewer::zoomOut()
{
    setCustomZoom(roundFactor(effectiveZoom()) - 0.05);
}

void Previewer::zoomOptionSelected(int index)
{
    if (index < 0) {
        return;
    }
    setZoom(d_zoomComboBox->itemData(index));
}

void Previewer::manualZoomEntered()
{
    QString value = d_zoomComboBox->lineEdit()->text();

    bool ok = false;
    int factor = value.toInt(&ok);
    if (!ok) {
        return;
    }

    d_zoomComboBox->setCurrentIndex(-1);
    setCustomZoom(factor / 100.0);
}

void Previewer::currentPageChanged(int page)
{
    QString pageLabel = d_previewDocument->pageLabel(page);
    QString pageCount = QString::number(d_previewDocument->pageCount());

    d_currentPageLabel->setText(tr("Page %1 of %2").arg(pageLabel, pageCount));
}

qreal Previewer::effectiveZoom()
{
    if (d_pdfView->zoomMode() == QPdfView::ZoomMode::Custom) {
        return d_pdfView->zoomFactor();
    }

    QSizeF currentPageSize = d_previewDocument->pagePointSize(d_pdfView->pageNavigator()->currentPage());

    int displayWidth = d_pdfView->viewport()->width()
        - d_pdfView->documentMargins().left()
        - d_pdfView->documentMargins().right();

    // A point is 1/72th of an inch
    qreal pointSize = screen()->logicalDotsPerInch() / 72.0;

    if (d_pdfView->zoomMode() == QPdfView::ZoomMode::FitToWidth) {
        return displayWidth / (currentPageSize.width() * pointSize);
    }
    else if (d_pdfView->zoomMode() == QPdfView::ZoomMode::FitInView) {
        QSize displaySize(displayWidth, d_pdfView->viewport()->height() - d_pdfView->pageSpacing());
        QSizeF scaled = (currentPageSize * pointSize).scaled(displaySize, Qt::KeepAspectRatio);

        return scaled.width() / (currentPageSize.width() * pointSize);
    }
    return 1.0;
}

void Previewer::setZoom(QVariant zoomValue)
{
    bool ok = false;
    double zoomFactor = zoomValue.toDouble(&ok);

    if (ok) {
        setCustomZoom(zoomFactor);
    }
    else {
        QString val = zoomValue.toString();
        if (val == FIT_TO_PAGE) {
            d_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
        }
        else if (val == FIT_TO_WIDTH) {
            d_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        }
    }
}

void Previewer::setCustomZoom(qreal factor)
{
    d_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    d_pdfView->setZoomFactor(factor);
    d_zoomComboBox->setEditText(QString("%1%").arg(qRound(factor * 100)));
}

}
