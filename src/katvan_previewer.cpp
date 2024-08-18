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
#include "katvan_previewerview.h"
#include "katvan_typstdriverwrapper.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

static constexpr QLatin1StringView FIT_TO_PAGE("fit-page");
static constexpr QLatin1StringView FIT_TO_WIDTH("fit-width");

static constexpr QLatin1StringView SETTING_PREVIEW_ZOOM("preview/zoom");

namespace katvan {

Previewer::Previewer(TypstDriverWrapper* driver, QWidget* parent)
    : QWidget(parent)
{
    d_view = new PreviewerView(driver, this);

    connect(driver, &TypstDriverWrapper::previewReady, this, &Previewer::updatePreview);
    connect(d_view, &PreviewerView::currentPageChanged, this, &Previewer::currentPageChanged);

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
    layout->addWidget(d_view, 1);
}

Previewer::~Previewer()
{
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
    switch (d_view->zoomMode()) {
        case PreviewerView::ZoomMode::Custom:
            zoomValue = d_view->zoomFactor();
            break;
        case PreviewerView::ZoomMode::FitToPage:
            zoomValue = FIT_TO_PAGE;
            break;
        case PreviewerView::ZoomMode::FitToWidth:
            zoomValue = FIT_TO_WIDTH;
            break;
    }

    settings.setValue(SETTING_PREVIEW_ZOOM, zoomValue);
}

void Previewer::reset()
{
    d_view->setPages({});
    d_currentPageLabel->setText(QString());
}

void Previewer::updatePreview(QList<typstdriver::PreviewPageData> pages)
{
    int origY = d_view->verticalScrollBar()->value();
    int origX = d_view->horizontalScrollBar()->value();

    d_view->setPages(pages);

    d_view->verticalScrollBar()->setValue(origY);
    d_view->horizontalScrollBar()->setValue(origX);

    currentPageChanged(d_view->currentPage());
}

static qreal roundFactor(qreal factor)
{
    // Round to nearest multiple of 5%
    return qRound(factor * 20) / 20.0;
}

void Previewer::zoomIn()
{
    qreal zoom = roundFactor(d_view->effectiveZoom(d_view->currentPage()));
    setCustomZoom(zoom + 0.05);
}

void Previewer::zoomOut()
{
    qreal zoom = roundFactor(d_view->effectiveZoom(d_view->currentPage()));
    setCustomZoom(zoom - 0.05);
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
    QString pageLabel = d_view->pageLabel(page);
    QString pageCount = QString::number(d_view->pageCount());

    d_currentPageLabel->setText(tr("Page %1 of %2").arg(pageLabel, pageCount));
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
            d_view->setZoomMode(PreviewerView::ZoomMode::FitToPage);
        }
        else if (val == FIT_TO_WIDTH) {
            d_view->setZoomMode(PreviewerView::ZoomMode::FitToWidth);
        }
    }
}

void Previewer::setCustomZoom(qreal factor)
{
    d_view->setCustomZoomFactor(factor);
    d_zoomComboBox->setEditText(QString("%1%").arg(qRound(factor * 100)));
}

}
