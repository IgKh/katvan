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
#include "katvan_typstdriver.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

namespace katvan {

TypstDriver::TypstDriver(QObject* parent)
    : QObject(parent)
    , d_status(Status::INITIALIZED)
    , d_inputFile(nullptr)
{
    d_compilerPath = findTypstCompiler();
    qDebug() << "Found typst at" << d_compilerPath;

    d_outputFile = new QTemporaryFile(QDir::tempPath() + "/katvan_XXXXXX.pdf", this);
    d_outputFile->open();

    d_process = new QProcess(this);
    d_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(d_process, &QProcess::errorOccurred, this, &TypstDriver::processErrorOccurred);
    connect(d_process, &QProcess::finished, this, &TypstDriver::compilerFinished);
    connect(d_process, &QProcess::readyRead, this, &TypstDriver::compilerOutputReady);
}

QString TypstDriver::findTypstCompiler() const
{
    QString localPath = QStandardPaths::findExecutable("typst", { QCoreApplication::applicationDirPath() });
    if (!localPath.isEmpty()) {
        return localPath;
    }

    return QStandardPaths::findExecutable("typst");
}

void TypstDriver::resetInputFile(const QString& sourceFileName)
{
    d_status = Status::INITIALIZED;

    if (d_inputFile != nullptr) {
        d_inputFile->deleteLater();
    }

    if (sourceFileName.isEmpty()) {
        d_inputFile = new QTemporaryFile(this);
    }
    else {
        QFileInfo info(sourceFileName);
        d_inputFile = new QTemporaryFile(info.absolutePath() + "/.katvan_XXXXXX.typ", this);
    }
    d_inputFile->open();
}

void TypstDriver::updatePreview(const QString& source)
{
    if (d_compilerPath.isEmpty()) {
        return;
    }
    if (d_status == Status::PROCESSING) {
        qDebug() << "Compiler already running, skipping";
        return;
    }

    Q_ASSERT(d_inputFile != nullptr);

    d_status = Status::PROCESSING;
    d_compilerOutput.clear();
    d_inputFile->seek(0);

    QTextStream stream(d_inputFile);
    stream << source;
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        d_compilerOutput = QStringLiteral("Error preparing preview input %1: %2")
            .arg(d_inputFile->fileName())
            .arg(d_inputFile->errorString());

        compilerFinished(-1);
        return;
    }

    d_inputFile->resize(d_inputFile->pos());

    d_process->setWorkingDirectory(QFileInfo(d_inputFile->fileName()).path());
    d_process->setProgram(d_compilerPath);
    d_process->setArguments(QStringList()
        << "compile"
        << d_inputFile->fileName()
        << d_outputFile->fileName());

    d_process->start();
}

void TypstDriver::processErrorOccurred()
{
    d_compilerOutput += QStringLiteral("Error starting typst compiler at %1: %2")
        .arg(d_compilerPath)
        .arg(d_process->errorString());

    compilerFinished(-2);
}

void TypstDriver::compilerFinished(int exitCode)
{
    if (exitCode == 0) {
        d_status = Status::SUCCESS;
        emit previewReady(d_outputFile->fileName());
    }
    else {
        d_status = Status::FAILED;
        emit compilationFailed(d_compilerOutput);
    }
}

void TypstDriver::compilerOutputReady()
{
    QTextStream stream(d_process);
    d_compilerOutput += stream.readAll();
}

}
