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
#include "katvan_editortheme.h"

#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace katvan {

bool EditorTheme::isAppInDarkMode()
{
    QGuiApplication* app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!app) {
        // Happens in test suite
        return false;
    }

    if (app->styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        return true;
    }

    // If the Qt platform integration doesn't support signaling a color scheme,
    // sniff it from the global palette.
    QPalette palette = app->palette();
    return palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

}
