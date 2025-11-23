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
#pragma once

#include <QIcon>
#include <QString>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace katvan::utils {

QString getApplicationDir(bool& isInstalled);

QIcon themeIcon(const char* xdgIcon);
QIcon themeIcon(const char* xdgIcon, const char* macIcon);

QString getHostPath(QString path);
QString formatFilePath(QString path);

QString showPdfExportDialog(QWidget* parent, const QString& sourceFilePath);

}
