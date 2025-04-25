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

#include "typstdriver_export.h"

#include <QList>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace katvan::typstdriver {

class TYPSTDRIVER_EXPORT TypstCompilerSettings {
public:
    TypstCompilerSettings();
    TypstCompilerSettings(const QSettings& settings);

    void save(QSettings& settings);

    bool allowPreviewPackages() const { return d_allowPreviewPackages; }
    QStringList allowedPaths() const { return d_allowedPaths; }

    void setAllowPreviewPackages(bool allow) { d_allowPreviewPackages = allow; }
    void setAllowedPaths(const QStringList& allowedPaths) { d_allowedPaths = allowedPaths; }

private:
    bool d_allowPreviewPackages;
    QStringList d_allowedPaths;
};

}
