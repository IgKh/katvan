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
#include "katvan_editortooltip.h"

#include <qpa/qplatformcursor.h>
#include <qpa/qplatformscreen.h>

#include <QApplication>
#include <QLabel>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>

namespace katvan {

/*
 * This unit contains an implementation for a custom Tool Tip component meant to
 * be used for Typst contextual tool tips in the editor. The differences from
 * the built-in QToolTip:
 *
 * - Based on QTextBrowser instead of QLabel, which allows for larger and richer
 *   tool tip content with scrolling, links, etc.
 *
 * - Allows for a "See More" link for pointing at a relevant web page (in Typst
 *   online documentation, or otherwise) at a fixed position on the tool tip.
 *
 * - Slightly different behaviour which is more like hover tips in other code
 *   editors, i.e no expiration timer and the tip is only dismissed on explicit
 *   action.
 *
 * The code here is heavily adapted from Qt's qtbase:src/widgets/kernel/qtooltip.cpp
 *
 * Original copyright:
 * Copyright (C) 2016 The Qt Company Ltd.
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
 *
 * TODO:
 * - Better keyboard accessibility (navigate to links, etc)
 */

EditorToolTipFrame::EditorToolTipFrame(QWidget* parent)
    : QWidget(parent, Qt::ToolTip)
    , d_byKeyboard(false)
{
    d_browser = new QTextBrowser(this);
    d_browser->setOpenExternalLinks(true);
    d_browser->setContextMenuPolicy(Qt::NoContextMenu);
    d_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_browser->setFrameStyle(QFrame::NoFrame);

    d_extraInfoLabel = new QLabel(this);
    d_extraInfoLabel->setOpenExternalLinks(true);
    d_extraInfoLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);

    d_hideTimer = new QTimer(this);
    d_hideTimer->callOnTimeout(this, &EditorToolTipFrame::hideImmediately);

    QPalette palette = QToolTip::palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Inactive, QPalette::ToolTipBase));
    palette.setColor(QPalette::Text, palette.color(QPalette::Inactive, QPalette::ToolTipText));

    setPalette(palette);
    ensurePolished();

    qApp->installEventFilter(this);
    setWindowOpacity(style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, nullptr, this) / 255.0);
}

void EditorToolTipFrame::setContent(const QString& text, const QUrl& link)
{
    d_hideTimer->stop();
    d_browser->setDocument(nullptr);

    QTextDocument* doc = new QTextDocument(d_browser);
    doc->setDocumentMargin(0);
    doc->setHtml(text);

    if (!link.isEmpty()) {
        d_extraInfoLabel->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(
            link.toString(),
            tr("See More...")));
        d_extraInfoLabel->show();
    }
    else {
        d_extraInfoLabel->hide();
    }

    updateSizeAndLayout(doc);

    // Set the document to the browser only after calculating size so events
    // from it won't interfere
    d_browser->setDocument(doc);
}

static QSizeF documentNaturalSize(QTextDocument* doc)
{
    qreal width = 0;

    // Also forces whole layout to complete
    QSizeF fullSize = doc->size();

    for (QTextBlock b = doc->begin(); b != doc->end(); b = b.next()) {
        QTextLayout* tl = b.layout();
        for (int i = 0; i < tl->lineCount(); i++) {
            QTextLine line = tl->lineAt(i);
            width = qMax(width, line.naturalTextWidth());
        }
    }
    return QSizeF { width, fullSize.height() };
}

static QSize calculateBrowserSize(QTextDocument* doc, bool& needsScrollBar)
{
    constexpr int MAX_WIDTH = 500;
    constexpr int MAX_HEIGHT = 600;

    doc->setTextWidth(MAX_WIDTH);

    QSizeF documentSize = documentNaturalSize(doc);

    int width = qCeil(documentSize.width());
    int height = qCeil(documentSize.height());
    if (height > MAX_HEIGHT) {
        height = MAX_HEIGHT;
        needsScrollBar = true;
    }
    else {
        needsScrollBar = false;
    }

    return QSize(width, height);
}

void EditorToolTipFrame::updateSizeAndLayout(QTextDocument* newDocument)
{
    const int margin = 5 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, this);
    const int spacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);

    int totalWidth = 2 * margin;
    int totalHeight = 2 * margin;

    bool needsScrollBar = false;
    QSize browserSize = calculateBrowserSize(newDocument, needsScrollBar);
    if (needsScrollBar) {
        browserSize.rwidth() += (d_browser->verticalScrollBar()->width() + 5);
        d_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        d_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    totalWidth += browserSize.width();
    totalHeight += browserSize.height();

    d_browser->move(margin, margin);
    d_browser->resize(browserSize);

    if (d_extraInfoLabel->isVisibleTo(this)) {
        QSize labelSize = d_extraInfoLabel->sizeHint();
        totalWidth = qMax(totalWidth, labelSize.width());
        totalHeight += (labelSize.height() + spacing);

        d_extraInfoLabel->move(margin, margin + browserSize.height() + spacing);
        d_extraInfoLabel->resize(qMax(browserSize.width(), labelSize.width()), labelSize.height());
    }

    resize(totalWidth, totalHeight);
}

static QScreen* screenForPosition(QWidget* parent, const QPoint& globalPos)
{
    QScreen* parentScreen;
    if (parent) {
        parentScreen = parent->screen();
    }
    else {
        parentScreen = qApp->primaryScreen();
    }

    QScreen* exactScreen = parentScreen->virtualSiblingAt(globalPos);
    return exactScreen ? exactScreen : parentScreen;
}

static QSize screenCursorSize(QScreen* screen)
{
    QPlatformScreen* nativeScreen = screen->handle();

    QSize cursorPhysicalSize { 16, 16 };
    QPlatformCursor* cursor = nativeScreen->cursor();
    if (cursor) {
        cursorPhysicalSize = cursor->size();
    }

    return cursorPhysicalSize / screen->devicePixelRatio();
}

static void adjustPosition(QPoint& pos, const QSize& size, QScreen* screen)
{
    QRect screenRect = screen->geometry();
    if (pos.x() + size.width() > screenRect.right()) {
        pos.rx() -= (4 + size.width());
    }
    if (pos.y() + size.height() > screenRect.bottom()) {
        pos.ry() -= (24 + size.height());
    }

    pos.setX(qBound(screenRect.left(), pos.x(), screenRect.right() - size.width()));
    pos.setY(qBound(screenRect.top(), pos.y(), screenRect.bottom() - size.height()));
}

void EditorToolTipFrame::setPlacement(const QPoint& globalMousePos)
{
    d_byKeyboard = false;

    QScreen* screen = screenForPosition(parentWidget(), globalMousePos);
    QSize cursorSize = screenCursorSize(screen);

    QPoint pos {
        globalMousePos.x() + cursorSize.width() / 2,
        globalMousePos.y() + cursorSize.height()
    };

    adjustPosition(pos, size(), screen);
    move(pos);
}

void EditorToolTipFrame::setPlacement(const QRect& globalCursorRect)
{
    d_byKeyboard = true;

    QPoint pos = globalCursorRect.bottomLeft();
    pos.ry() += 5;

    adjustPosition(pos, size(), parentWidget()->screen());
    move(pos);
}

void EditorToolTipFrame::hideDeferred()
{
    if (!d_hideTimer->isActive()) {
        d_hideTimer->start(300);
    }
}

void EditorToolTipFrame::hideImmediately()
{
    close();
    deleteLater();
}

void EditorToolTipFrame::resizeEvent(QResizeEvent* e)
{
    QStyleOption option;
    option.initFrom(this);

    QStyleHintReturnMask frameMask;
    if (style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask)) {
        setMask(frameMask.region);
    }

    QWidget::resizeEvent(e);
}

void EditorToolTipFrame::paintEvent(QPaintEvent* e)
{
    QStyleOptionFrame opt;
    opt.initFrom(this);

    QStylePainter p(this);
    p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
    p.end();

    QWidget::paintEvent(e);
}

static bool isDescendant(QObject* obj, QObject* parent)
{
    while (obj != nullptr) {
        if (obj == parent) {
            return true;
        }
        obj = obj->parent();
    }
    return false;
}

bool EditorToolTipFrame::eventFilter(QObject* obj, QEvent* e)
{
    switch (e->type()) {
        case QEvent::Enter:
        case QEvent::WindowActivate:
        case QEvent::FocusIn:
            if (obj == this) {
                d_hideTimer->stop();
            }
            else if (obj != windowHandle() && !isDescendant(obj, this)) {
                if (e->type() != QEvent::Enter || !d_byKeyboard) {
                    hideDeferred();
                }
            }
            break;
        case QEvent::Leave:
        case QEvent::WindowDeactivate:
        case QEvent::FocusOut:
            if (!isDescendant(obj, this)) {
                if (e->type() != QEvent::Leave || !d_byKeyboard) {
                    hideDeferred();
                }
            }
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::KeyPress:
            if (obj != windowHandle() && !isDescendant(obj, this)) {
                hideImmediately();
            }
            break;
    }
    return false;
}

QPointer<EditorToolTipFrame> EditorToolTip::s_frame;

void EditorToolTip::showByMouse(const QPoint& globalPos, QWidget* parent, const QString& text, const QUrl& link)
{
    if (text.isEmpty()) {
        hide();
        return;
    }

    QToolTip::hideText();

    if (!s_frame) {
        s_frame = new EditorToolTipFrame(parent);
    }
    else if (s_frame->parentWidget() != parent) {
        s_frame->setParent(parent);
    }

    s_frame->setContent(text, link);
    s_frame->setPlacement(globalPos);
    s_frame->showNormal();
}

void EditorToolTip::showByKeyboard(const QRect& globalCursorRect, QWidget* parent, const QString& text, const QUrl& link)
{
    if (text.isEmpty()) {
        hide();
        return;
    }

    QToolTip::hideText();

    if (!s_frame) {
        s_frame = new EditorToolTipFrame(parent);
    }
    else if (s_frame->parentWidget() != parent) {
        s_frame->setParent(parent);
    }

    s_frame->setContent(text, link);
    s_frame->setPlacement(globalCursorRect);
    s_frame->showNormal();
}

void EditorToolTip::hide()
{
    if (s_frame) {
        s_frame->hideImmediately();
    }
}

}

#include "moc_katvan_editortooltip.cpp"
