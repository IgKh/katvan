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

#include <QObject>
#include <QTemporaryFile>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

namespace katvan {

class TypstDriver : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        INITIALIZED,
        PROCESSING,
        SUCCESS,
        FAILED
    };

public:
    TypstDriver(QObject* parent = nullptr);

    bool compilerFound() const { return !d_compilerPath.isEmpty(); }
    Status status() const { return d_status; }
    QString pdfFilePath() const { return d_outputFile->fileName(); }

    void resetInputFile(const QString& sourceFileName);

signals:
    void previewReady(const QString& pdfPath);
    void compilationFailed(const QString& output);

public slots:
    void updatePreview(const QString& source);

private slots:
    void processErrorOccurred();
    void compilerFinished(int exitCode);
    void compilerOutputReady();

private:
    Status d_status;
    QString d_compilerPath;
    QString d_compilerOutput;
    QTemporaryFile* d_outputFile;
    QTemporaryFile* d_inputFile;
    QProcess* d_process;
};

}
