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

#include <QByteArray>
#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace katvan {

namespace typstdriver {
class Engine;
class Logger;
class PackageManager;
}

class TypstDriverWrapper : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        INITIALIZING,
        INITIALIZED,
        PROCESSING,
        SUCCESS,
        SUCCESS_WITH_WARNINGS,
        FAILED
    };

public:
    TypstDriverWrapper(QObject* parent = nullptr);
    ~TypstDriverWrapper();

    static QString typstVersion();

    Status status() const { return d_status; }
    QByteArray pdfBuffer() const;

    void resetInputFile(const QString& sourceFileName);

signals:
    void previewReady(QByteArray pdfBuffer);
    void outputReady(const QStringList& output);
    void compilationStatusChanged();

public slots:
    void updatePreview(const QString& source);

private slots:
    void compilerOutputLogged(QStringList messages);

private:
    typstdriver::Engine* d_engine;
    typstdriver::Logger* d_compilerLogger;
    typstdriver::PackageManager* d_packageManager;

    QThread* d_thread;

    Status d_status;
    QStringList d_compilerOutput;
    QString d_pendingSourceToCompile;
};

}
