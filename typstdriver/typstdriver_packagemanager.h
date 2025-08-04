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

#include "typstdriver_export.h"
#include "typstdriver_logger.h"

#include <QDir>
#include <QMutex>
#include <QObject>
#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QDateTime;
class QLockFile;
class QNetworkAccessManager;
QT_END_NAMESPACE

namespace katvan::typstdriver {

class TypstCompilerSettings;

struct TYPSTDRIVER_EXPORT PackageManagerStatistics {
    qsizetype totalSize;
    qsizetype numPackages;
    qsizetype numPackageVersions;
};

struct TYPSTDRIVER_EXPORT PackageDetails {
    QString name;
    QString version;
    QString description;
};

class TYPSTDRIVER_EXPORT PackageManager : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        SUCCESS,
        NOT_ALLOWED,
        NOT_FOUND,
        NETWORK_ERROR,
        IO_ERROR,
        ARCHIVE_ERROR,
        PARSE_ERROR,
    };

    enum class DownloadResult {
        SUCCESS,
        NOT_MODIFIED,
        FAILED,
    };

    static void setDownloadCacheLocation(const QString& dirPath);
    static QString downloadCacheDirectory();
    static PackageManagerStatistics cacheStatistics();

    PackageManager(Logger* logger, QObject* parent = nullptr);

    Error error() const { return d_error; }
    QString errorMessage() const { return d_errorMessage; }

    void applySettings(std::shared_ptr<TypstCompilerSettings> settings);

    QString getPackageLocalPath(const QString& packageNamespace, const QString& name, const QString& version);

    QList<PackageDetails> getPreviewPackagesListing();

private:
    QString getPreviewPackageLocalPath(const QString& packageName, const QString& version);

    QString getLocalPackagePath(const QString& packageNamespace, const QString& packageName, const QString& version);

    QDir ensurePreviewCacheDir();
    std::unique_ptr<QLockFile> lockCacheDir(const QDir& cacheDir);

    QList<PackageDetails> parsePackageIndex(const QByteArray& indexData);

    DownloadResult downloadFile(const QString& url, const QString& targetPath, const QDateTime& lastModified);
    DownloadResult downloadFile(const QString& url, QIODevice* target, const QDateTime& lastModified);

    bool extractArchive(const QString& archivePath, const QDir& targetDir);

    static QString s_downloadCacheLocation;

    Error d_error;
    QString d_errorMessage;
    std::shared_ptr<TypstCompilerSettings> d_settings;
    QMutex d_settingsLock;

    Logger* d_logger;
    QNetworkAccessManager* d_networkManager;
};

}
