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
#include "katvan_typstdriverwrapper.h"

#include "typstdriver_engine.h"
#include "typstdriver_logger.h"
#include "typstdriver_packagemanager.h"

#include <QDebug>
#include <QThread>

#include <optional>

namespace katvan {

TypstDriverWrapper::TypstDriverWrapper(QObject* parent)
    : QObject(parent)
    , d_engine(nullptr)
    , d_status(Status::INITIALIZING)
{
    d_thread = new QThread(this);
    d_thread->setObjectName("TypstDriverThread");
    d_thread->start();

    d_compilerLogger = new typstdriver::Logger();
    d_compilerLogger->moveToThread(d_thread);
    connect(d_compilerLogger, &typstdriver::Logger::messagesLogged, this, &TypstDriverWrapper::compilerOutputLogged);

    d_packageManager = new typstdriver::PackageManager(d_compilerLogger);
    d_packageManager->moveToThread(d_thread);
}

TypstDriverWrapper::~TypstDriverWrapper()
{
    if (d_engine) {
        d_engine->deleteLater();
    }

    if (d_thread->isRunning()) {
        d_thread->quit();
        d_thread->wait();
    }
}

QString TypstDriverWrapper::typstVersion()
{
    return typstdriver::Engine::typstVersion();
}

QByteArray TypstDriverWrapper::pdfBuffer() const
{
    if (!d_engine) {
        return QByteArray();
    }
    return d_engine->pdfBuffer();
}

void TypstDriverWrapper::resetInputFile(const QString& sourceFileName)
{
    d_status = Status::INITIALIZING;
    Q_EMIT compilationStatusChanged();

    if (d_engine != nullptr) {
        d_engine->deleteLater();
    }

    d_engine = new typstdriver::Engine(sourceFileName, d_compilerLogger, d_packageManager);
    d_engine->moveToThread(d_thread);

    auto onInitialized = [this]() {
        d_status = Status::INITIALIZED;
        Q_EMIT compilationStatusChanged();

        if (!d_pendingSourceToCompile.isEmpty()) {
            updatePreview(d_pendingSourceToCompile);
            d_pendingSourceToCompile.clear();
        }
    };

    connect(d_engine, &typstdriver::Engine::previewReady, this, &TypstDriverWrapper::previewReady);
    connect(d_engine, &typstdriver::Engine::initialized, this, onInitialized, Qt::SingleShotConnection);

    QMetaObject::invokeMethod(d_engine, "init");
}

void TypstDriverWrapper::updatePreview(const QString& source)
{
    if (d_status == Status::PROCESSING) {
        qDebug() << "Compiler already processing, skipping";
        return;
    }
    else if (d_status == Status::INITIALIZING) {
        d_pendingSourceToCompile = source;
        return;
    }

    d_status = Status::PROCESSING;
    Q_EMIT compilationStatusChanged();

    d_compilerOutput.clear();
    QMetaObject::invokeMethod(d_engine, "compile", source);
}

void TypstDriverWrapper::compilerOutputLogged(QStringList messages)
{
    for (const QString& message : messages) {
        std::optional<Status> newStatus;
        if (message.indexOf("compiled successfully") >= 0 ) {
            newStatus = Status::SUCCESS;
        }
        else if (message.indexOf("compiled with warnings") >= 0) {
            newStatus = Status::SUCCESS_WITH_WARNINGS;
        }
        else if (message.indexOf("compiled with errors") >= 0) {
            newStatus = Status::FAILED;
        }

        if (newStatus) {
            d_status = newStatus.value();
            Q_EMIT compilationStatusChanged();
        }
    }

    d_compilerOutput.append(std::move(messages));
    Q_EMIT outputReady(d_compilerOutput);
}

}
