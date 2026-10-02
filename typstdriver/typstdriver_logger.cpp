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
#include "typstdriver_logger.h"
#include "typstdriver_logger_p.h"

#include "typstdriver_ffi/bridge.h"

namespace katvan::typstdriver {

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

void Logger::logNote(const QString& message, Diagnostic::Category category)
{
    Diagnostic diag;
    diag.setKind(Diagnostic::Kind::NOTE);
    diag.setCategory(category);
    diag.setMessage(message);

    logDiagnostic(std::move(diag));
}

void Logger::logDiagnostic(Diagnostic diagnostic)
{
    Q_EMIT diagnosticLogged(std::move(diagnostic));
}

LoggerProxy::LoggerProxy(Logger& logger)
    : d_logger(logger)
{
}

static Diagnostic::Category convertCategory(LogCategory category)
{
    switch (category) {
        case LogCategory::Compilation: return Diagnostic::Category::COMPILATION;
        case LogCategory::Export: return Diagnostic::Category::EXPORT;
    }
    return Diagnostic::Category::COMPILATION;
}

void LoggerProxy::logNote(LogCategory category, rust::Str message) const
{
    d_logger.logNote(
        QString::fromUtf8(message.data(), message.size()),
        convertCategory(category));
}

static Diagnostic makeDiagnostic(
    Diagnostic::Kind kind,
    LogCategory category,
    rust::Str message,
    rust::Str file,
    int64_t startLine,
    int64_t startCol,
    int64_t endLine,
    int64_t endCol,
    rust::Vec<rust::Str> rustHints)
{
    Diagnostic diag;
    diag.setKind(kind);
    diag.setCategory(convertCategory(category));
    diag.setMessage(QString::fromUtf8(message.data(), message.size()));
    diag.setFile(QString::fromUtf8(file.data(), file.size()));

    if (startLine >= 0) {
        diag.setStartLocation(Diagnostic::Location {
            static_cast<int>(startLine),
            static_cast<int>(startCol)
        });
    }
    if (endLine >= 0) {
        diag.setEndLocation(Diagnostic::Location {
            static_cast<int>(endLine),
            static_cast<int>(endCol)
        });
    }

    QStringList hints;
    for (const auto& str : rustHints) {
        hints.append(QString::fromUtf8(str.data(), str.size()));
    }
    diag.setHints(hints);

    return diag;
}

void LoggerProxy::logWarning(
    LogCategory category,
    rust::Str message,
    rust::Str file,
    int64_t startLine,
    int64_t startCol,
    int64_t endLine,
    int64_t endCol,
    rust::Vec<rust::Str> hints) const
{
    d_logger.logDiagnostic(makeDiagnostic(Diagnostic::Kind::WARNING, category, message, file, startLine, startCol, endLine, endCol, hints));
}

void LoggerProxy::logError(
    LogCategory category,
    rust::Str message,
    rust::Str file,
    int64_t startLine,
    int64_t startCol,
    int64_t endLine,
    int64_t endCol,
    rust::Vec<rust::Str> hints) const
{
    d_logger.logDiagnostic(makeDiagnostic(Diagnostic::Kind::ERROR, category, message, file, startLine, startCol, endLine, endCol, hints));
}

}

#include "moc_typstdriver_logger.cpp"
