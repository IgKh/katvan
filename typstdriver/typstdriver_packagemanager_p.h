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

#include "rust/cxx.h"

namespace katvan::typstdriver {

class PackageEntry;
class PackageManager;

enum class PackageManagerError : ::std::uint8_t;

class PackageManagerProxy
{
public:
    PackageManagerProxy(PackageManager& manager);

    rust::String getPackageLocalPath(
        rust::Str packageNamespace,
        rust::Str name,
        rust::Str version);

    rust::Vec<PackageEntry> getPreviewPackagesListing();

    PackageManagerError error() const;
    rust::String errorMessage() const;

private:
    PackageManager& d_manager;
};

}
