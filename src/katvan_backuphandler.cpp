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
#include "katvan_editor.h"

#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QTemporaryFile>
#include <QTimer>

namespace katvan {

static constexpr QLatin1StringView SETTING_PREFIX_TEMP_FILES = QLatin1StringView("compilertmp/");

// XXX Make configurable
static constexpr qint64 MAX_SECS_BETWEEN_BACKUPS = 15;

static QString settingsKeyForFile(QString fileName)
{
    fileName.replace(QDir::separator(), QLatin1Char('_'));
    return SETTING_PREFIX_TEMP_FILES + "/" + fileName;
}

BackupHandler::BackupHandler(Editor* editor, QObject* parent)
    : QObject(parent)
    , d_backupFile(nullptr)
    , d_lastSaveTimestamp(0)
    , d_editor(editor)
{
    d_timer = new QTimer(this);
    d_timer->setSingleShot(true);
    d_timer->callOnTimeout(this, &BackupHandler::saveContent);
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

    d_sourceFile = sourceFileName;
    d_lastSaveTimestamp = QDateTime::currentSecsSinceEpoch();

    // Check if there is a left over settings key for the new file, indicating
    // a previous crash with it open
    return settings.value(settingsKeyForFile(sourceFileName), QString()).toString();
}

void BackupHandler::editorContentChanged()
{
    if (d_backupFile == nullptr) {
        return;
    }

    qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now < d_lastSaveTimestamp + MAX_SECS_BETWEEN_BACKUPS) {
        if (!d_timer->isActive()) {
            d_timer->start((d_lastSaveTimestamp + MAX_SECS_BETWEEN_BACKUPS - now) * 1000);
        }
        return;
    }

    d_timer->stop();
    saveContent();
}

void BackupHandler::saveContent()
{
    if (d_backupFile == nullptr) {
        QFileInfo info(d_sourceFile);
        d_backupFile = new QTemporaryFile(info.absolutePath() + "/.katvan_XXXXXX.typ", this);
        d_backupFile->open();

        QSettings settings;
        settings.setValue(settingsKeyForFile(d_sourceFile), d_backupFile->fileName());
    }
    else {
        d_backupFile->seek(0);
    }

    QTextStream stream(d_backupFile);
    stream << d_editor->documentText();
    stream.flush();

    d_backupFile->resize(d_backupFile->pos());

    d_lastSaveTimestamp = QDateTime::currentSecsSinceEpoch();
}

}
