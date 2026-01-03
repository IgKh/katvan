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

#include <QPointer>
#include <QUrl>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QTextBrowser;
class QTextDocument;
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class EditorToolTipFrame : public QWidget
{
    Q_OBJECT

public:
    EditorToolTipFrame(bool byKeyboard, QWidget* parent = nullptr);

    bool isByKeyboard() const { return d_byKeyboard; }

    void setContent(const QString& text, const QUrl& link);
    void setPlacement(const QPoint& globalMousePos);
    void setPlacement(const QRect& globalCursorRect);

public slots:
    void hideDeferred();
    void hideImmediately();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void updatePalette();
    void updateSizeAndLayout(QTextDocument* newDocument);
    void updatePlacement(const QPoint& globalPos);

    QTextBrowser* d_browser;
    QLabel* d_extraInfoLabel;
    QTimer* d_hideTimer;
    bool d_byKeyboard;
};

class EditorToolTip
{
public:
    static void showByMouse(const QPoint& globalPos, QWidget* parent, const QString& text, const QUrl& link = QUrl());
    static void showByKeyboard(const QRect& globalCursorRect, QWidget* parent, const QString& text, const QUrl& link = QUrl());
    static void hide();

private:
    static QPointer<EditorToolTipFrame> s_frame;
};

}
