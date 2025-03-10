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

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QTemporaryFile;
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class Editor;

class BackupHandler : public QObject
{
    Q_OBJECT

public:
    BackupHandler(Editor* editor, QObject* parent = nullptr);
    ~BackupHandler();

    void setBackupInterval(int intervalSecs);
    QString resetSourceFile(const QString& sourceFileName);

public slots:
    void editorContentChanged();

private slots:
    void saveContent();

private:
    int d_backupIntervalSecs;
    QString d_sourceFile;
    QTemporaryFile* d_backupFile;
    qint64 d_lastSaveTimestamp;

    Editor* d_editor;
    QTimer* d_timer;
};

}
