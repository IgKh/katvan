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
#include "typstdriver_compilersettings.h"

#include <QSettings>

static constexpr QLatin1StringView SETTING_ALLOW_PREVIEW_PACKAGES("compiler/allow-preview-packages");
static constexpr QLatin1StringView SETTING_ENABLE_A11Y_EXTRAS("compiler/enable-a11y-extras");
static constexpr QLatin1StringView SETTING_ALLOWED_PATHS = QLatin1StringView("compiler/allowedPaths");

namespace katvan::typstdriver {

TypstCompilerSettings::TypstCompilerSettings()
    : d_allowPreviewPackages(false)
    , d_enableA11yExtras(false)
{
}

TypstCompilerSettings::TypstCompilerSettings(const QSettings& settings)
    : d_allowPreviewPackages(settings.value(SETTING_ALLOW_PREVIEW_PACKAGES, true).toBool())
    , d_enableA11yExtras(settings.value(SETTING_ENABLE_A11Y_EXTRAS, false).toBool())
    , d_allowedPaths(settings.value(SETTING_ALLOWED_PATHS).toStringList())
{
}

void TypstCompilerSettings::save(QSettings& settings)
{
    settings.setValue(SETTING_ALLOW_PREVIEW_PACKAGES, d_allowPreviewPackages);
    settings.setValue(SETTING_ENABLE_A11Y_EXTRAS, d_enableA11yExtras);
    settings.setValue(SETTING_ALLOWED_PATHS, d_allowedPaths);
}

}
