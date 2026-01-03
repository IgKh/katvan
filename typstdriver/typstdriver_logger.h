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

#include "typstdriver_export.h"

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace katvan::typstdriver {

class TYPSTDRIVER_EXPORT Diagnostic
{
    Q_GADGET

public:
    enum class Kind {
        NOTE,
        WARNING,
        ERROR
    };
    Q_ENUM(Kind);

    struct Location {
        int line;
        int column;
    };

    Diagnostic() : d_kind(Kind::NOTE) {}

    Kind kind() const { return d_kind; }
    QString message() const { return d_message; }
    QString file() const { return d_file; }
    std::optional<Location> startLocation() const { return d_startLocation; }
    std::optional<Location> endLocation() const { return d_endLocation; }
    QStringList hints() const { return d_hints; }

    void setKind(Kind kind) { d_kind = kind; }
    void setMessage(const QString& message) { d_message = message; }
    void setFile(const QString& file) { d_file = file; }
    void setStartLocation(Location location) { d_startLocation = location; }
    void setEndLocation(Location location) { d_endLocation = location; }
    void setHints(const QStringList& hints) { d_hints = hints; }

private:
    Kind d_kind;
    QString d_message;
    QString d_file;
    std::optional<Location> d_startLocation;
    std::optional<Location> d_endLocation;
    QStringList d_hints;
};

class TYPSTDRIVER_EXPORT Logger : public QObject
{
    Q_OBJECT

public:
    Logger(QObject* parent = nullptr);

    void logNote(const QString& message);
    void logDiagnostic(Diagnostic diagnostic);

signals:
    void diagnosticLogged(katvan::typstdriver::Diagnostic diagnostic);
};

}
