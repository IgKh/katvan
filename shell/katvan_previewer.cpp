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
#include "katvan_previewer.h"
#include "katvan_utils.h"

#include "katvan_previewerview.h"
#include "katvan_typstdriverwrapper.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolBar>
#include <QVBoxLayout>

static constexpr QLatin1StringView FIT_TO_PAGE("fit-page");
static constexpr QLatin1StringView FIT_TO_WIDTH("fit-width");

static constexpr QLatin1StringView SETTING_PREVIEW_ZOOM("preview/zoom");
static constexpr QLatin1StringView SETTING_PREVIEW_INVERT_COLORS("preview/invert-colors");
static constexpr QLatin1StringView SETTING_PREVIEW_FOLLOW_CURSOR("preview/follow-cursor");

namespace katvan {

static QString stripTrailingPercent(const QString& value)
{
    QString result = value;
    if (value.endsWith(QLatin1Char('%'))) {
        result.chop(1);
    }
    return result;
}

class ZoomValidator : public QIntValidator
{
public:
    ZoomValidator(QObject* parent = nullptr) : QIntValidator(1, 1000, parent) {}

    QValidator::State validate(QString& input, int& pos) const override
    {
        QString value = stripTrailingPercent(input);
        int p = qMin(pos, value.size());
        return QIntValidator::validate(value, p);
    }
};

Previewer::Previewer(TypstDriverWrapper* driver, QWidget* parent)
    : QWidget(parent)
{
    d_view = new PreviewerView(driver, this);

    connect(driver, &TypstDriverWrapper::previewReady, this, &Previewer::updatePreview);
    connect(d_view, &PreviewerView::currentPageChanged, this, &Previewer::currentPageChanged);
    connect(d_view, &PreviewerView::zoomedByScrolling, this, &Previewer::zoomedByScrolling);

    d_zoomComboBox = new QComboBox();
    d_zoomComboBox->setEditable(true);
    d_zoomComboBox->setValidator(new ZoomValidator(this));
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

    QAction* zoomOutAction = new QAction(this);
    zoomOutAction->setIcon(utils::themeIcon("zoom-out", "minus.magnifyingglass"));
    zoomOutAction->setToolTip(tr("Zoom Out Preview"));
    connect(zoomOutAction, &QAction::triggered, this, &Previewer::zoomOut);

    QAction* zoomInAction = new QAction(this);
    zoomInAction->setIcon(utils::themeIcon("zoom-in", "plus.magnifyingglass"));
    zoomInAction->setToolTip(tr("Zoom In Preview"));
    connect(zoomInAction, &QAction::triggered, this, &Previewer::zoomIn);

    d_currentPageLabel = new QLabel();
    d_currentPageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    d_currentPageLabel->setAlignment(Qt::AlignCenter);

    d_invertColorsAction = new QAction(this);
    d_invertColorsAction->setIcon(utils::themeIcon("color-mode-invert-text", "circle.lefthalf.filled"));
    d_invertColorsAction->setToolTip(tr("Invert Preview Colors"));
    d_invertColorsAction->setCheckable(true);
    connect(d_invertColorsAction, &QAction::toggled, d_view, &PreviewerView::setInvertColors);

    d_followEditorCursorAction = new QAction(this);
    d_followEditorCursorAction->setIcon(utils::themeIcon("debug-execute-from-cursor", "text.cursor"));
    d_followEditorCursorAction->setToolTip(tr("Follow Editor Cursor"));
    d_followEditorCursorAction->setCheckable(true);
    connect(d_followEditorCursorAction, &QAction::toggled, this, &Previewer::followEditorCursorChanged);

    QToolBar* toolBar = new QToolBar();
    toolBar->setIconSize(QSize(22, 22));
    toolBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    toolBar->addAction(zoomOutAction);
    toolBar->addWidget(d_zoomComboBox);
    toolBar->addAction(zoomInAction);
    toolBar->addWidget(d_currentPageLabel);
    toolBar->addAction(d_invertColorsAction);
    toolBar->addAction(d_followEditorCursorAction);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolBar);
    layout->addWidget(d_view, 1);
}

Previewer::~Previewer()
{
}

int Previewer::currentPage() const
{
    return d_view->currentPage();
}

bool Previewer::shouldFollowEditorCursor() const
{
    return d_followEditorCursorAction->isChecked();
}

void Previewer::restoreSettings(const QSettings& settings)
{
    QVariant zoomValue = settings.value(SETTING_PREVIEW_ZOOM, FIT_TO_WIDTH);
    setZoom(zoomValue);

    int index = d_zoomComboBox->findData(zoomValue);
    if (index >= 0) {
        d_zoomComboBox->setCurrentIndex(index);
    }

    bool invertColors = settings.value(SETTING_PREVIEW_INVERT_COLORS, false).toBool();
    d_invertColorsAction->setChecked(invertColors);
    d_view->setInvertColors(invertColors);

    bool followCursor = settings.value(SETTING_PREVIEW_FOLLOW_CURSOR, false).toBool();
    d_followEditorCursorAction->setChecked(followCursor);
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
    settings.setValue(SETTING_PREVIEW_INVERT_COLORS, d_invertColorsAction->isChecked());
    settings.setValue(SETTING_PREVIEW_FOLLOW_CURSOR, d_followEditorCursorAction->isChecked());
}

void Previewer::reset()
{
    d_view->setPages({});
    d_currentPageLabel->setText(QString());
}

void Previewer::jumpToPreview(int page, QPointF pos)
{
    d_view->jumpTo(page, pos);
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
    QString value = stripTrailingPercent(d_zoomComboBox->lineEdit()->text());

    bool ok = false;
    int factor = value.toInt(&ok);
    if (!ok) {
        return;
    }

    d_zoomComboBox->setCurrentIndex(-1);
    setCustomZoom(factor / 100.0);
}

void Previewer::zoomedByScrolling(int units)
{
    qreal zoom = roundFactor(d_view->effectiveZoom(d_view->currentPage()));
    setCustomZoom(zoom + units * 0.05);
}

void Previewer::currentPageChanged(int page)
{
    QString pageLabel = d_view->pageLabel(page);
    QString pageCount = QString::number(d_view->pageCount());

    d_currentPageLabel->setText(tr("Page %1 of %2").arg(pageLabel, pageCount));
}

void Previewer::followEditorCursorChanged(bool checked)
{
    if (checked) {
        Q_EMIT followCursorEnabled();
    }
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
    factor = qBound(0.05, factor, 10.0);

    d_view->setCustomZoomFactor(factor);
    d_zoomComboBox->setEditText(QString("%1%").arg(qRound(factor * 100)));
}

}

#include "moc_katvan_previewer.cpp"
