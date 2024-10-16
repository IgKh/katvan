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

#include <QIcon>
#include <QString>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace katvan::utils {

static constexpr QChar ALM_MARK = (ushort)0x061c;
static constexpr QChar LRM_MARK = (ushort)0x200e;
static constexpr QChar RLM_MARK = (ushort)0x200f;
static constexpr QChar LRI_MARK = (ushort)0x2066;
static constexpr QChar RLI_MARK = (ushort)0x2067;
static constexpr QChar PDI_MARK = (ushort)0x2069;

bool isBidiControlChar(QChar ch);

QString getApplicationDir(bool& isInstalled);

QIcon themeIcon(const char* xdgIcon);
QIcon themeIcon(const char* xdgIcon, const char* macIcon);

QString formatFilePath(QString path);

Qt::LayoutDirection naturalTextDirection(const QString& text);

QString showPdfExportDialog(QWidget* parent, const QString& sourceFilePath);

}
