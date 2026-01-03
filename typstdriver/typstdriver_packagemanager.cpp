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
#include "typstdriver_compilersettings.h"
#include "typstdriver_packagemanager.h"
#include "typstdriver_packagemanager_p.h"

#include "typstdriver_ffi/bridge.h"

#include <QBuffer>
#include <QDateTime>
#include <QDirIterator>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QStandardPaths>

#include <archive.h>
#include <archive_entry.h>

namespace katvan::typstdriver {

static constexpr QLatin1StringView PREVIEW_REPOSITORY_BASE("https://packages.typst.org/preview/");

QString PackageManager::s_downloadCacheLocation;

void PackageManager::setDownloadCacheLocation(const QString& dirPath)
{
    s_downloadCacheLocation = dirPath;
}

QString PackageManager::downloadCacheDirectory()
{
    QString location = s_downloadCacheLocation;
    if (location.isEmpty()) {
        location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    return location + "/typst";
}

PackageManagerStatistics PackageManager::cacheStatistics()
{
    QString cacheDir = typstdriver::PackageManager::downloadCacheDirectory();
    QDirIterator it {
        cacheDir,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    };

    qsizetype totalSize = 0;
    QSet<QString> distinctPackages;
    QSet<QString> distinctPackageVersions;

    while (it.hasNext()) {
        QFileInfo info = it.nextFileInfo();
        totalSize += info.size();

        if (info.isDir()) {
            QStringList parts = info.filePath().remove(cacheDir).split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (parts.length() == 3) {
                distinctPackages.insert(parts[1]);
                distinctPackageVersions.insert(parts[1] + ":" + parts[2]);
            }
        }
    }

    return PackageManagerStatistics {
        totalSize,
        distinctPackages.size(),
        distinctPackageVersions.size()
    };
}

PackageManager::PackageManager(Logger* logger, QObject* parent)
    : QObject(parent)
    , d_error(Error::SUCCESS)
    , d_logger(logger)
    , d_settings(std::make_shared<TypstCompilerSettings>())
{
    d_networkManager = new QNetworkAccessManager(this);
}

void PackageManager::applySettings(std::shared_ptr<TypstCompilerSettings> settings)
{
    QMutexLocker locker { &d_settingsLock };
    d_settings = settings;
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
    bool allowed = false;
    {
        QMutexLocker locker { &d_settingsLock };
        allowed = d_settings->allowPreviewPackages();
    }

    if (!allowed) {
        d_error = Error::NOT_ALLOWED;
        d_errorMessage = "use of Typst Universe preview packages is disabled in application settings";
        return QString();
    }

    QDir cacheDir = ensurePreviewCacheDir();

    auto cacheLock = lockCacheDir(cacheDir);
    if (!cacheLock) {
        return QString();
    }

    QDir packageDir = cacheDir.filesystemPath() / qPrintable(name) / qPrintable(version);
    if (packageDir.exists()) {
        return packageDir.path();
    }

    QString archiveName = QStringLiteral("%1-%2.tar.gz").arg(name, version);
    QString archivePath = cacheDir.filePath(archiveName);

    if (!QFile::exists(archivePath)) {
        QString url = PREVIEW_REPOSITORY_BASE + archiveName;
        DownloadResult result = downloadFile(url, archivePath, QDateTime());
        if (result != DownloadResult::SUCCESS) {
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

#if defined(Q_OS_LINUX)
    // Also explicitly look in the home directory (in case we are in flatpak or
    // another environment that overrides XDG_DATA_HOME)
    locations.append(QDir::homePath() + "/.local/share");
    locations.removeDuplicates();
#endif

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

QList<PackageDetails> PackageManager::getPreviewPackagesListing()
{
    QDir cacheDir = ensurePreviewCacheDir();

    auto cacheLock = lockCacheDir(cacheDir);
    if (!cacheLock) {
        return QList<PackageDetails>();
    }

    QString indexPath = cacheDir.filePath("index.json");

    QDateTime lastModified;
    QFileInfo info { indexPath };
    if (info.exists()) {
        lastModified = info.lastModified().toUTC();
    }

    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::ReadWrite)) {
        // Though it really shouldn't happen...
        d_error = Error::IO_ERROR;
        d_errorMessage = QStringLiteral("failed to open in-memory buffer (out of memory?)");
        return QList<PackageDetails>();
    }

    DownloadResult result = downloadFile(PREVIEW_REPOSITORY_BASE + "index.json", &buffer, lastModified);
    if (result == DownloadResult::FAILED) {
        return QList<PackageDetails>();
    }

    QFile file(indexPath);
    if (!file.open(QIODevice::ReadWrite)) {
        d_error = Error::IO_ERROR;
        d_errorMessage = QStringLiteral("failed to open %1: %2").arg(indexPath, file.errorString());
        return QList<PackageDetails>();
    }

    if (result == DownloadResult::SUCCESS) {
        // First time, or changed since last download. Save to disk.
        file.write(data);
    }
    else {
        Q_ASSERT(result == DownloadResult::NOT_MODIFIED);

        // Not changed since last download. The buffer is empty, so read from disk.
        data = file.readAll();
    }

    return parsePackageIndex(data);
}

QList<PackageDetails> PackageManager::parsePackageIndex(const QByteArray& indexData)
{
    QList<PackageDetails> result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(indexData, &error);
    if (error.error != QJsonParseError::NoError) {
        d_error = Error::PARSE_ERROR;
        d_errorMessage = QStringLiteral("failed to parse package index: %1").arg(error.errorString());
        return result;
    }

    if (!doc.isArray()) {
        qWarning() << "Package index JSON is not an array";
        return result;
    }

    const QJsonArray packages = doc.array();
    for (const auto& packageJson : packages) {
        if (!packageJson.isObject()) {
            continue;
        }
        QJsonObject obj = packageJson.toObject();

        PackageDetails details;
        details.name = obj["name"].toString();
        details.version = obj["version"].toString();
        details.description = obj["description"].toString();

        result.append(std::move(details));
    }
    return result;
}

QDir PackageManager::ensurePreviewCacheDir()
{
    QDir cacheDir = downloadCacheDirectory() + "/preview";
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }
    return cacheDir;
}

std::unique_ptr<QLockFile> PackageManager::lockCacheDir(const QDir& cacheDir)
{
    QString lockFile = cacheDir.filePath(".lock");

    auto lock = std::make_unique<QLockFile>(lockFile);

    if (!lock->tryLock(30000)) { // 30 seconds
        d_error = Error::IO_ERROR;
        d_errorMessage = QStringLiteral("failed to lock the package cache directory");

        lock.reset();
    }
    return lock;
}

PackageManager::DownloadResult PackageManager::downloadFile(const QString& url, const QString& targetPath, const QDateTime& lastModified)
{
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        d_error = Error::IO_ERROR;
        d_errorMessage = QStringLiteral("failed to create %1: %2").arg(targetPath, file.errorString());
        return DownloadResult::FAILED;
    }

    DownloadResult result = downloadFile(url, &file, lastModified);
    if (result == DownloadResult::FAILED) {
        file.remove();
    }
    return result;
}

PackageManager::DownloadResult PackageManager::downloadFile(const QString& url, QIODevice* target, const QDateTime& lastModified)
{
    d_logger->logNote(QStringLiteral("downloading %1 ...").arg(url));

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Katvan/0");

    if (!lastModified.isNull()) {
        // Wed, 21 Oct 2015 07:28:00 GMT
        QString since = lastModified.toString(QStringLiteral("ddd, dd MMM yyyy HH:mm:ss GMT"));
        request.setHeader(QNetworkRequest::IfModifiedSinceHeader, since);
    }

    std::unique_ptr<QNetworkReply> reply { d_networkManager->get(request) };
    connect(reply.get(), &QNetworkReply::readyRead, this, [&target, &reply]() {
        target->write(reply->readAll());
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
        return DownloadResult::FAILED;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!lastModified.isNull() && statusCode == 304) {
        d_logger->logNote("no changes");
        return DownloadResult::NOT_MODIFIED;
    }

    d_logger->logNote("download complete");
    return DownloadResult::SUCCESS;
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

rust::Vec<PackageEntry> PackageManagerProxy::getPreviewPackagesListing()
{
    const QList<PackageDetails> packages = d_manager.getPreviewPackagesListing();

    rust::Vec<PackageEntry> result;
    result.reserve(packages.size());

    for (const auto& package : packages) {
        result.push_back(PackageEntry {
            rust::String(qUtf8Printable(package.name)),
            rust::String(qUtf8Printable(package.version)),
            rust::String(qUtf8Printable(package.description))
        });
    }

    return result;
}

PackageManagerError PackageManagerProxy::error() const
{
    switch (d_manager.error()) {
        case PackageManager::Error::SUCCESS: return PackageManagerError::Success;
        case PackageManager::Error::NOT_ALLOWED: return PackageManagerError::NotAllowed;
        case PackageManager::Error::NOT_FOUND: return PackageManagerError::NotFound;
        case PackageManager::Error::NETWORK_ERROR: return PackageManagerError::NetworkError;
        case PackageManager::Error::IO_ERROR: return PackageManagerError::IoError;
        case PackageManager::Error::ARCHIVE_ERROR: return PackageManagerError::ArchiveError;
        case PackageManager::Error::PARSE_ERROR: return PackageManagerError::ParseError;
    }
    Q_UNREACHABLE();
}

rust::String PackageManagerProxy::errorMessage() const
{
    return rust::String(qUtf8Printable(d_manager.errorMessage()));
}

}

#include "moc_typstdriver_packagemanager.cpp"
