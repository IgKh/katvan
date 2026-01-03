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

#include "katvan_typstdriverwrapper.h"

#include "typstdriver_logger.h"

#include <QAbstractTableModel>
#include <QFont>
#include <QList>

#include <optional>
#include <tuple>

namespace katvan {

class DiagnosticsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum {
        COLUMN_SEVERITY = 0,
        COLUMN_MESSAGE,
        COLUMN_SOURCE_LOCATION,
        COLUMN_COUNT
    };

    enum {
        ROLE_MOUSE_CURSOR = Qt::UserRole + 1,
    };

    DiagnosticsModel(QObject* parent = nullptr);

    void setInputFileName(const QString& fileName);

    TypstDriverWrapper::Status impliedStatus() const;
    QList<typstdriver::Diagnostic> sourceDiagnostics() const;

    std::optional<std::tuple<int, int>> getSourceLocation(const QModelIndex& index) const;

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

public slots:
    void clear();
    void addDiagnostic(const katvan::typstdriver::Diagnostic& diagnostic);

private:
    QFont d_font;
    QString d_shortFileName;
    QList<typstdriver::Diagnostic> d_diagnostics;
};

}
