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
#include "typstdriver_logger.h"
#include "typstdriver_logger_p.h"

namespace katvan::typstdriver {

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

void Logger::logMessage(const QString& message, bool toBatch)
{
    if (toBatch) {
        d_batchedMessages.append(message);
    }
    else {
        releaseBatched();
        Q_EMIT messagesLogged({ message });
    }
}

void Logger::releaseBatched()
{
    if (!d_batchedMessages.isEmpty()) {
        Q_EMIT messagesLogged(d_batchedMessages);
        d_batchedMessages.clear();
    }
}

LoggerProxy::LoggerProxy(Logger& logger)
    : d_logger(logger)
{
}

void LoggerProxy::logOne(rust::Str message) const
{
    d_logger.logMessage(QString::fromUtf8(message.data(), message.size()), false);
}

void LoggerProxy::logToBatch(rust::Str message) const
{
    d_logger.logMessage(QString::fromUtf8(message.data(), message.size()), true);
}

void LoggerProxy::releaseBatched() const
{
    d_logger.releaseBatched();
}

}
