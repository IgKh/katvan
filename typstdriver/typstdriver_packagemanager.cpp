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
#include "typstdriver_packagemanager.h"
#include "typstdriver_packagemanager_p.h"

#include "typstdriver_ffi/bridge.h"

#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>

#include <archive.h>
#include <archive_entry.h>

#include <memory>

namespace katvan::typstdriver {

QString PackageManager::s_downloadCacheLocation;

void PackageManager::setDownloadCacheLocation(const QString& dirPath)
{
    s_downloadCacheLocation = dirPath;
}

PackageManager::PackageManager(Logger* logger, QObject* parent)
    : QObject(parent)
    , d_error(Error::SUCCESS)
    , d_logger(logger)
{
    d_networkManager = new QNetworkAccessManager(this);
}

QString PackageManager::getPackageLocalPath(const QString& packageNamespace, const QString& name, const QString& version)
{
    d_error = Error::SUCCESS;
    d_errorMessage.clear();

    if (packageNamespace == QStringLiteral("preview")) {
        return getPreviewPackageLocalPath(name, version);
    }
    else {
        return getLocalPackagePath(packageNamespace, name, version);
    }
}

QString PackageManager::getPreviewPackageLocalPath(const QString& name, const QString& version)
{
    QString cacheBase = s_downloadCacheLocation;
    if (cacheBase.isEmpty()) {
        cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }

    QDir cacheDir = cacheBase + "/typst/preview";
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    QDir packageDir = cacheDir.filesystemPath() / qPrintable(name) / qPrintable(version);
    if (packageDir.exists()) {
        return packageDir.path();
    }

    QString archiveName = QStringLiteral("%1-%2.tar.gz").arg(name, version);
    QString archivePath = cacheDir.filePath(archiveName);

    if (!QFile::exists(archivePath)) {
        QString url = "https://packages.typst.org/preview/" + archiveName;
        if (!downloadFile(url, archivePath)) {
            return QString();
        }
    }

    packageDir.mkpath(".");
    if (!extractArchive(archivePath, packageDir.absolutePath())) {
        return QString();
    }

    return packageDir.canonicalPath();
}

QString PackageManager::getLocalPackagePath(const QString& packageNamespace, const QString& name, const QString& version)
{
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& location : std::as_const(locations)) {
        QDir parent = location + "/typst/packages";
        auto path = parent.filesystemPath() /
            qPrintable(packageNamespace) /
            qPrintable(name) /
            qPrintable(version);

        QDir packageDir = path;
        if (packageDir.exists()) {
            return packageDir.canonicalPath();
        }
    }

    d_error = Error::NOT_FOUND;
    return QString();
}

bool PackageManager::downloadFile(const QString& url, const QString& targetPath)
{
    d_logger->logNote(QStringLiteral("downloading %1 ...").arg(url));

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        d_error = Error::IO_ERROR;
        d_errorMessage = QStringLiteral("failed to create %1: %2").arg(targetPath, file.errorString());
        return false;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Katvan/0");

    std::unique_ptr<QNetworkReply> reply { d_networkManager->get(request) };
    connect(reply.get(), &QNetworkReply::readyRead, this, [&file, &reply]() {
        file.write(reply->readAll());
    });

    QEventLoop loop;
    connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            d_error = Error::NOT_FOUND;
        }
        else {
            d_error = Error::NETWORK_ERROR;
            d_errorMessage = QStringLiteral("failed to download %1: %2").arg(url, reply->errorString());
        }

        file.remove();
        return false;
    }

    d_logger->logNote("download complete");
    return true;
}

static int copyArchiveData(struct archive* src, struct archive* dst)
{
    const void* buff;
    size_t buffSize;
    la_int64_t offset;

    while (true) {
        int rc = archive_read_data_block(src, &buff, &buffSize, &offset);
        if (rc == ARCHIVE_EOF) {
            return ARCHIVE_OK;
        }
        else if (rc < ARCHIVE_OK) {
            return rc;
        }

        rc = archive_write_data_block(dst, buff, buffSize, offset);
        if (rc < ARCHIVE_OK) {
            return rc;
        }
    }
}

bool PackageManager::extractArchive(const QString& archivePath, const QDir& targetDir)
{
    struct archive* ar = archive_read_new();
    archive_read_support_filter_all(ar);
    archive_read_support_format_all(ar);

    int rc = archive_read_open_filename(ar, qPrintable(archivePath), 10240);
    if (rc < ARCHIVE_OK) {
        d_error = Error::ARCHIVE_ERROR;
        d_errorMessage = QStringLiteral("failed to open archive %1: %2").arg(
            archivePath,
            QLatin1StringView(archive_error_string(ar)));

        archive_read_free(ar);
        return false;
    }

    struct archive* extractor = archive_write_disk_new();

    struct archive_entry* entry;
    while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
        QString targetPath = targetDir.absoluteFilePath(QString::fromLocal8Bit(archive_entry_pathname(entry)));

        archive_entry_update_pathname_utf8(entry, qUtf8Printable(targetPath));

        rc = archive_write_header(extractor, entry);
        if (rc < ARCHIVE_WARN) {
            d_error = Error::ARCHIVE_ERROR;
            d_errorMessage = QStringLiteral("failed to create %1: %2").arg(
                targetPath,
                QLatin1StringView(archive_error_string(extractor)));

            archive_write_free(extractor);
            archive_read_free(ar);
            return false;
        }

        if (archive_entry_size(entry) > 0) {
            rc = copyArchiveData(ar, extractor);
            if (rc < ARCHIVE_WARN) {
                d_error = Error::ARCHIVE_ERROR;
                d_errorMessage = QStringLiteral("failed to write file %1: %2").arg(
                    targetPath,
                    QLatin1StringView(archive_error_string(extractor)));

                archive_write_free(extractor);
                archive_read_free(ar);
                return false;
            }
        }
    }

    archive_write_free(extractor);
    archive_read_free(ar);
    return true;
}

PackageManagerProxy::PackageManagerProxy(PackageManager& manager)
    : d_manager(manager)
{
}

rust::String PackageManagerProxy::getPackageLocalPath(
    rust::Str packageNamespace,
    rust::Str name,
    rust::Str version)
{
    QString result = d_manager.getPackageLocalPath(
        QString::fromUtf8(packageNamespace.data(), packageNamespace.size()),
        QString::fromUtf8(name.data(), name.size()),
        QString::fromUtf8(version.data(), version.size())
    );

    return rust::String(qUtf8Printable(result));
}

PackageManagerError PackageManagerProxy::error() const
{
    switch (d_manager.error()) {
        case PackageManager::Error::SUCCESS: return PackageManagerError::Success;
        case PackageManager::Error::NOT_FOUND: return PackageManagerError::NotFound;
        case PackageManager::Error::NETWORK_ERROR: return PackageManagerError::NetworkError;
        case PackageManager::Error::IO_ERROR: return PackageManagerError::IoError;
        case PackageManager::Error::ARCHIVE_ERROR: return PackageManagerError::ArchiveError;
    }
    Q_UNREACHABLE();
}

rust::String PackageManagerProxy::errorMessage() const
{
    return rust::String(qUtf8Printable(d_manager.errorMessage()));
}

}
