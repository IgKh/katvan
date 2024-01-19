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

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QTextEdit;
QT_END_NAMESPACE

namespace katvan {

class SearchBar : public QWidget
{
    Q_OBJECT

public:
    SearchBar(QTextEdit* editor, QWidget* parent = nullptr);

public slots:
    void ensureVisible();

private slots:
    void findNext();
    void findPrevious();

private:
    void setupUI();
    void find(bool forward);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QTextEdit* d_editor;

    QLineEdit* d_searchTerm;
};

}
