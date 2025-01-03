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

#include "rust/cxx.h"

namespace katvan::typstdriver {

class Logger;

class LoggerProxy
{
public:
    LoggerProxy(Logger& logger);

    void logNote(rust::Str message) const;
    void logWarning(rust::Str message, rust::Str file, int64_t line, int64_t col, rust::Vec<rust::Str> hints) const;
    void logError(rust::Str message, rust::Str file, int64_t line, int64_t col, rust::Vec<rust::Str> hints) const;

private:
    Logger& d_logger;
};

}
