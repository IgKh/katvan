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

#include <QTextEdit>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class Editor : public QTextEdit
{
    Q_OBJECT

    friend class LineNumberGutter;

public:
    Editor(QWidget* parent = nullptr);

public slots:
    void insertLRM();
    void insertInlineMath();
    void toggleTextBlockDirection();
    void goToBlock(int blockNum);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int lineNumberGutterWidth();
    QTextBlock getFirstVisibleBlock();
    void lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event);

private slots:
    void updateLineNumberGutterWidth();
    void updateLineNumberGutters();

signals:
    void contentModified(const QString& text);

private:
    QWidget* d_leftLineNumberGutter;
    QWidget* d_rightLineNumberGutter;

    QTimer* d_debounceTimer;
};

}
