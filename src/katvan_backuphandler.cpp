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
#include "katvan_backuphandler.h"

#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QTemporaryFile>

namespace katvan {

static constexpr QLatin1StringView SETTING_PREFIX_TEMP_FILES = QLatin1StringView("compilertmp/");

static QString settingsKeyForFile(QString fileName)
{
    fileName.replace(QDir::separator(), QLatin1Char('_'));
    return SETTING_PREFIX_TEMP_FILES + "/" + fileName;
}

BackupHandler::BackupHandler(QObject* parent)
    : QObject(parent)
    , d_backupFile(nullptr)
    , d_lastSaveTimestamp(0)
{
}

BackupHandler::~BackupHandler()
{
    if (!d_sourceFile.isEmpty()) {
        QSettings settings;
        settings.remove(settingsKeyForFile(d_sourceFile));
    }
}

QString BackupHandler::resetSourceFile(const QString& sourceFileName)
{
    QSettings settings;
    if (!d_sourceFile.isEmpty()) {
        settings.remove(settingsKeyForFile(d_sourceFile));
    }

    if (d_backupFile != nullptr) {
        delete d_backupFile;
        d_backupFile = nullptr;
    }

    if (sourceFileName.isEmpty()) {
        return QString();
    }

    QFileInfo info(sourceFileName);
    d_backupFile = new QTemporaryFile(info.absolutePath() + "/.katvan_XXXXXX.typ", this);
    d_backupFile->open();

    d_lastSaveTimestamp = QDateTime::currentSecsSinceEpoch();

    // Check if there is a left over settings key for the new file, indicating
    // a previous crash with it open
    QString key = settingsKeyForFile(sourceFileName);
    QString oldBackup = settings.value(key, QString()).toString();

    settings.setValue(key, d_backupFile->fileName());

    return oldBackup;
}

void BackupHandler::editorContentChanged(const QString& content)
{
    if (d_backupFile == nullptr) {
        return;
    }

    // XXX make configurable
    qint64 now = QDateTime::currentSecsSinceEpoch();
    if (d_lastSaveTimestamp > now - 30) {
        return;
    }

    d_lastSaveTimestamp = now;
    d_backupFile->seek(0);

    QTextStream stream(d_backupFile);
    stream << content;
    stream.flush();

    d_backupFile->resize(d_backupFile->pos());
}

}
