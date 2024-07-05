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
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

#include <optional>

namespace katvan {

static constexpr QLatin1StringView SETTING_PREFIX_TEMP_FILES = QLatin1StringView("compilertmp/");

static QString settingsKeyForFile(QString fileName)
{
    fileName.replace(QDir::separator(), QLatin1Char('_'));
    return SETTING_PREFIX_TEMP_FILES + "/" + fileName;
}

TypstDriver::TypstDriver(QObject* parent)
    : QObject(parent)
    , d_status(Status::INITIALIZING)
    , d_inputFile(nullptr)
{
    d_compilerPath = findTypstCompiler();
    if (!d_compilerPath.isEmpty()) {
        qDebug() << "Found typst at" << d_compilerPath;
    }
    else {
        qWarning() << "Did not find typst CLI";
    }

    d_outputFile = new QTemporaryFile(QDir::tempPath() + "/katvan_XXXXXX.pdf", this);
    d_outputFile->open();

    d_process = new QProcess(this);
    d_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(d_process, &QProcess::errorOccurred, this, &TypstDriver::processErrorOccurred);
    connect(d_process, &QProcess::readyRead, this, &TypstDriver::compilerOutputReady);
}

TypstDriver::~TypstDriver()
{
    if (!d_inputSourceFile.isEmpty()) {
        QSettings settings;
        settings.remove(settingsKeyForFile(d_inputSourceFile));
    }

    if (d_process->state() != QProcess::NotRunning) {
        d_process->disconnect();
        terminateCompiler();
        d_process->waitForFinished();
    }
}

QString TypstDriver::findTypstCompiler()
{
    QString localPath = QStandardPaths::findExecutable("typst", { QCoreApplication::applicationDirPath() });
    if (!localPath.isEmpty()) {
        return localPath;
    }

    return QStandardPaths::findExecutable("typst");
}

void TypstDriver::terminateCompiler()
{
#ifdef Q_OS_WINDOWS
    // No better way known to gracefuly kill a CLI process on Windows :(
    d_process->kill();
#else
    d_process->terminate();
#endif
}

void TypstDriver::startCompiler(const QString& sourceFileName)
{
    Q_ASSERT(d_status == Status::INITIALIZING);
    Q_ASSERT(d_inputFile == nullptr);

    QString workingDir;
    if (sourceFileName.isEmpty()) {
        workingDir = QDir::tempPath();
        d_inputFile = new QTemporaryFile(workingDir + "/katvan_XXXXXX.typ", this);
    }
    else {
        QFileInfo info(sourceFileName);
        workingDir = info.absolutePath();
        d_inputFile = new QTemporaryFile(workingDir + "/.katvan_XXXXXX.typ", this);
    }
    d_inputFile->open();

    d_process->setWorkingDirectory(workingDir);
    d_process->setProgram(d_compilerPath);
    d_process->setArguments(QStringList()
        << "watch"
        << "--diagnostic-format" << "short"
        << d_inputFile->fileName()
        << d_outputFile->fileName());

    d_process->start();
    d_status = Status::INITIALIZED;

    d_inputSourceFile = sourceFileName;
    if (!sourceFileName.isEmpty()) {
        QSettings settings;
        settings.setValue(settingsKeyForFile(sourceFileName), d_inputFile->fileName());
    }
}

QString TypstDriver::resetInputFile(const QString& sourceFileName)
{
    if (d_compilerPath.isEmpty()) {
        return QString();
    }

    d_status = Status::INITIALIZING;
    Q_EMIT compilationStatusChanged();

    QSettings settings;
    if (!d_inputSourceFile.isEmpty()) {
        settings.remove(settingsKeyForFile(d_inputSourceFile));
    }

    if (d_inputFile != nullptr) {
        delete d_inputFile;
        d_inputFile = nullptr;
    }

    // To avoid blocking the event loop while we wait for the Typst compiler
    // to exit, schedule a start of the compiler when it is done. For
    // consistency, do the same even when a compiler isn't already running
    auto startCompilerCallback = [this, sourceFileName]() {
        startCompiler(sourceFileName);
    };

    if (d_process->state() != QProcess::NotRunning) {
        connect(d_process, &QProcess::finished, this, startCompilerCallback, Qt::SingleShotConnection);
        terminateCompiler();
    }
    else {
        QMetaObject::invokeMethod(this, startCompilerCallback, Qt::QueuedConnection);
    }

    // Check if there is a left over settings key for the new file, indicating
    // a previous crash with it open
    return settings.value(settingsKeyForFile(sourceFileName), QString()).toString();
}

void TypstDriver::updatePreview(const QString& source)
{
    if (d_status == Status::PROCESSING || d_status == Status::INITIALIZING) {
        qDebug() << "Compiler not available at the moment, skipping";
        return;
    }

    Q_ASSERT(d_inputFile != nullptr);

    d_status = Status::PROCESSING;
    Q_EMIT compilationStatusChanged();

    d_compilerOutput.clear();
    d_inputFile->seek(0);

    QTextStream stream(d_inputFile);
    stream << source;
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        d_compilerOutput.append(QStringLiteral("Error preparing preview input %1: %2").arg(
            d_inputFile->fileName(),
            d_inputFile->errorString()));

        signalCompilerFailed();
        return;
    }

    d_inputFile->resize(d_inputFile->pos());
}

void TypstDriver::processErrorOccurred()
{
    if (d_status == Status::INITIALIZING && d_process->error() == QProcess::Crashed) {
        // Going to assume this is because of a kill on Windows. Ignore...
        return;
    }

    d_compilerOutput.append(QStringLiteral("Error starting typst compiler at %1: %2").arg(
        d_compilerPath,
        d_process->errorString()));

    signalCompilerFailed();
}

void TypstDriver::signalCompilerFailed()
{
    if (d_status != Status::INITIALIZING) {
        d_status = Status::FAILED;
    }
    Q_EMIT outputReady(d_compilerOutput);
    Q_EMIT compilationStatusChanged();
}

void TypstDriver::compilerOutputReady()
{
    if (d_status == Status::INITIALIZING) {
        return;
    }

    while (!d_process->atEnd()) {
        QByteArray buffer = d_process->readLine();
        QString line = QString::fromUtf8(buffer);

        if (!line.endsWith('\n')) {
            d_compilerOutputLineBuffer += line;
            continue;
        }
        if (!d_compilerOutputLineBuffer.isEmpty()) {
            line = d_compilerOutputLineBuffer + line;
            d_compilerOutputLineBuffer.clear();
        }
        line = line.trimmed();

        if (line.startsWith(QStringLiteral("watching ")) || line.startsWith(QStringLiteral("writing to "))) {
            continue;
        }

        if (!line.isEmpty()) {
            d_compilerOutput.append(line);
        }

        std::optional<Status> newStatus;
        if (line.indexOf("compiled successfully") >= 0 ) {
            newStatus = Status::SUCCESS;
        }
        else if (line.indexOf("compiled with warnings") >= 0) {
            newStatus = Status::SUCCESS_WITH_WARNINGS;
        }
        else if (line.indexOf("compiled with errors") >= 0) {
            newStatus = Status::FAILED;
        }

        if (newStatus) {
            d_status = newStatus.value();
            Q_EMIT compilationStatusChanged();

            if (d_status != Status::FAILED) {
                Q_EMIT previewReady(d_outputFile->fileName());
            }
        }
    }

    Q_EMIT outputReady(d_compilerOutput);
}

}
