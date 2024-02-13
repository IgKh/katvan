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
#include "katvan_recentfiles.h"

#include <QMenu>
#include <QSettings>

namespace katvan {

constexpr qsizetype MAX_RECENT_FILES = 10;

static constexpr QLatin1StringView SETTING_RECENT_FILES("recentFiles");

RecentFiles::RecentFiles(QObject* parent)
    : QObject(parent)
{
    d_fileList.reserve(MAX_RECENT_FILES + 1);
}

void RecentFiles::setMenu(QMenu* menu)
{
    d_menu = menu;
    rebuildMenu();
}

void RecentFiles::restoreRecents(const QSettings& settings)
{
    if (settings.contains(SETTING_RECENT_FILES)) {
        d_fileList = settings.value(SETTING_RECENT_FILES).toStringList();
        rebuildMenu();
    }
}

void RecentFiles::saveRecents()
{
    QSettings settings;
    settings.setValue(SETTING_RECENT_FILES, d_fileList);
}

void RecentFiles::addRecent(const QString& filePath)
{
    d_fileList.removeOne(filePath);
    d_fileList.prepend(filePath);

    if (d_fileList.size() > MAX_RECENT_FILES) {
        d_fileList.removeLast();
    }

    rebuildMenu();
    saveRecents();
}

void RecentFiles::clear()
{
    d_fileList.clear();
    rebuildMenu();
    saveRecents();
}

void RecentFiles::rebuildMenu()
{
    d_menu->clear();

    for (QString& filePath : d_fileList) {
        d_menu->addAction(filePath, [this, filePath]() {
            Q_EMIT fileSelected(filePath);
        });
    }

    if (!d_fileList.isEmpty()) {
        d_menu->addSeparator();
    }

    QAction* clearAction = d_menu->addAction(tr("Clear"), this, &RecentFiles::clear);
    clearAction->setIcon(QIcon::fromTheme("edit-clear-history", QIcon(":/icons/edit-clear-history.svg")));
    clearAction->setEnabled(!d_fileList.isEmpty());
}

}
