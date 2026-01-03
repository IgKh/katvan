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
#include "katvan_infobar.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace katvan {

InfoBar::InfoBar(QWidget* parent)
    : QWidget(parent)
{
    d_icon = new QLabel();
    d_message = new QLabel();
    d_buttonLayout = new QHBoxLayout();

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(d_icon);
    layout->addWidget(d_message, 1);
    layout->addLayout(d_buttonLayout);

    setBackgroundRole(QPalette::Highlight);
    setForegroundRole(QPalette::HighlightedText);
    setAutoFillBackground(true);
}

void InfoBar::clear()
{
    QLayoutItem* child = d_buttonLayout->takeAt(0);
    while (child != nullptr) {
        delete child->widget();
        delete child;
        child = d_buttonLayout->takeAt(0);
    }

    hide();
}

void InfoBar::addButton(QAbstractButton* button)
{
    d_buttonLayout->addWidget(button);
}

void InfoBar::showMessage(const QString& message)
{
    d_message->setText(message);
    d_icon->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxWarning));
    show();
}

}

#include "moc_katvan_infobar.cpp"
