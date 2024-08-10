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

#include "typstdriver_export.h"

#include <QList>
#include <QObject>
#include <QString>

namespace katvan::typstdriver {

class TYPSTDRIVER_EXPORT Logger : public QObject
{
    Q_OBJECT

public:
    Logger(QObject* parent = nullptr);

    void logMessage(const QString& message, bool toBatch = false);
    void releaseBatched();

signals:
    void messagesLogged(QStringList messages);

private:
    QStringList d_batchedMessages;
};

}
