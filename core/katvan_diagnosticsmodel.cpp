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
#include "katvan_diagnosticsmodel.h"

#include <QCursor>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>

namespace katvan {

static constexpr QLatin1StringView MAIN_SOURCE = QLatin1StringView("MAIN");

DiagnosticsModel::DiagnosticsModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    d_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

void DiagnosticsModel::setInputFileName(const QString& fileName)
{
    if (fileName.isEmpty()) {
        d_shortFileName = "Untitled";
    }
    else {
        d_shortFileName = QFileInfo(fileName).fileName();
    }
}

TypstDriverWrapper::Status DiagnosticsModel::impliedStatus() const
{
    bool hasWarnings = false;

    for (const auto& diagnostic : d_diagnostics) {
        if (diagnostic.kind() == typstdriver::Diagnostic::Kind::ERROR) {
            return TypstDriverWrapper::Status::FAILED;
        }
        else if (diagnostic.kind() == typstdriver::Diagnostic::Kind::WARNING) {
            hasWarnings = true;
        }
    }

    return hasWarnings
        ? TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS
        : TypstDriverWrapper::Status::SUCCESS;
}

QList<typstdriver::Diagnostic> DiagnosticsModel::sourceDiagnostics() const
{
    QList<typstdriver::Diagnostic> result = d_diagnostics;

    auto it = std::remove_if(result.begin(), result.end(), [](const typstdriver::Diagnostic& diagnostic) {
        return diagnostic.file() != MAIN_SOURCE;
    });
    result.erase(it, result.end());

    return result;
}

std::optional<std::tuple<int, int>> DiagnosticsModel::getSourceLocation(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= d_diagnostics.size()) {
        return std::nullopt;
    }

    const auto& diagnostic = d_diagnostics[index.row()];
    if (diagnostic.file() != MAIN_SOURCE) {
        return std::nullopt;
    }

    const auto& location = diagnostic.startLocation();
    if (location) {
        return std::make_tuple(location->line, location->column);
    }
    return std::nullopt;
}

int DiagnosticsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return d_diagnostics.size();
}

int DiagnosticsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return COLUMN_COUNT;
}

QVariant DiagnosticsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= d_diagnostics.size()) {
        return QVariant();
    }

    if (role == Qt::TextAlignmentRole) {
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    const auto& diagnostic = d_diagnostics[index.row()];

    if (role == Qt::FontRole) {
        QFont font = d_font;
        if (index.column() == COLUMN_SOURCE_LOCATION && diagnostic.file() == MAIN_SOURCE) {
            font.setUnderline(true);
        }
        return font;
    }
    if (role == ROLE_MOUSE_CURSOR) {
        if (index.column() == COLUMN_SOURCE_LOCATION && diagnostic.file() == MAIN_SOURCE) {
            return QCursor(Qt::PointingHandCursor);
        }
    }

    if (index.column() == COLUMN_SEVERITY) {
        auto kind = diagnostic.kind();
        if (role == Qt::DecorationRole) {
            switch (kind) {
                case typstdriver::Diagnostic::Kind::NOTE:
                    return QIcon(":/icons/data-information.svg");
                case typstdriver::Diagnostic::Kind::WARNING:
                    return QIcon(":/icons/data-warning.svg");
                case typstdriver::Diagnostic::Kind::ERROR:
                    return QIcon(":/icons/data-error.svg");
                default:
                    return QVariant();
            }
        }
        else if (role == Qt::AccessibleTextRole || role == Qt::ToolTipRole) {
            switch (kind) {
                case typstdriver::Diagnostic::Kind::NOTE:
                    return tr("Note");
                case typstdriver::Diagnostic::Kind::WARNING:
                    return tr("Warning");
                case typstdriver::Diagnostic::Kind::ERROR:
                    return tr("Error");
                default:
                    return QVariant();
            }
        }
    }
    else if (index.column() == COLUMN_MESSAGE && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
        QString message = diagnostic.message();
        QStringList hints = diagnostic.hints();
        for (const QString& hint : std::as_const(hints)) {
            message += QChar::LineFeed + QStringLiteral("Hint: ") + hint;
        }
        return message;
    }
    else if (index.column() == COLUMN_SOURCE_LOCATION && (role == Qt::DisplayRole || role == Qt::ToolTipRole)) {
        QString result = diagnostic.file();
        if (result == MAIN_SOURCE) {
            result = d_shortFileName;
        }

        if (diagnostic.startLocation()) {
            auto location = diagnostic.startLocation().value();
            result += QStringLiteral(" (%1:%2)").arg(location.line + 1).arg(location.column);
        }
        return result;
    }
    return QVariant();
}

void DiagnosticsModel::clear()
{
    beginResetModel();
    d_diagnostics.clear();
    endResetModel();
}

void DiagnosticsModel::addDiagnostic(const typstdriver::Diagnostic& diagnostic)
{
    beginInsertRows(QModelIndex(), d_diagnostics.size(), d_diagnostics.size());
    d_diagnostics.append(diagnostic);
    endInsertRows();
}

}

#include "moc_katvan_diagnosticsmodel.cpp"
