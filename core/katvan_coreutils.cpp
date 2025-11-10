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
#include "katvan_coreutils.h"

namespace katvan::utils {

WheelTracker::WheelTracker(QObject* parent)
    : QObject(parent)
    , d_accumulatedWheelUnits(0)
{
}

void WheelTracker::processEvent(QWheelEvent* event)
{
    int yAngle = event->angleDelta().y();
    if (yAngle == 0) {
        return;
    }

    // yAngle is in eighths of a degree. 15 degrees are typically considered
    // to be one "unit" of scroll. Therefore each 120 eights would be one
    // unit of change. However - those can accumulate over multiple events
    // if the scrolling device has high precision (e.g. a laptop trackpad)
    d_accumulatedWheelUnits += yAngle / 120.0;

    qreal units = std::trunc(d_accumulatedWheelUnits);
    if (!qFuzzyCompare(units, 0)) {
        if (event->inverted()) {
            units = -units;
        }

        d_accumulatedWheelUnits = 0;
        Q_EMIT scrolled(units);
    }
}

}

#include "moc_katvan_coreutils.cpp"
