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
#include "typstdriver_logger.h"

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QDir;
class QNetworkAccessManager;
QT_END_NAMESPACE

namespace katvan::typstdriver {

class TYPSTDRIVER_EXPORT PackageManager : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        SUCCESS,
        NOT_FOUND,
        NETWORK_ERROR,
        IO_ERROR,
        ARCHIVE_ERROR,
    };

    PackageManager(Logger* logger, QObject* parent = nullptr);

    Error error() const { return d_error; }
    QString errorMessage() const { return d_errorMessage; }

    QString getPackageLocalPath(const QString& packageNamespace, const QString& name, const QString& version);

private:
    QString getPreviewPackageLocalPath(const QString& packageName, const QString& version);

    QString getLocalPackagePath(const QString& packageNamespace, const QString& packageName, const QString& version);

    bool downloadFile(const QString& url, const QString& targetPath);

    bool extractArchive(const QString& archivePath, const QDir& targetDir);

    Error d_error;
    QString d_errorMessage;

    Logger* d_logger;
    QNetworkAccessManager* d_networkManager;
};

}