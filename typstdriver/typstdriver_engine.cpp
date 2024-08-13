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
#include "typstdriver_engine.h"
#include "typstdriver_logger.h"

#include "typstdriver_ffi/bridge.h"

#include <QFileInfo>
#include <QUuid>

#include <optional>

namespace katvan::typstdriver {

static rust::String qstringToRust(const QString& str)
{
    QByteArray data = str.toUtf8();
    return rust::String { data.constData(), static_cast<size_t>(data.size()) };
}

struct Engine::EnginePrivate
{
    EnginePrivate(Engine* q, Logger* logger, PackageManager* packageManager, QString fileRoot)
        : innerLogger(*logger)
        , innerPackageManager(*packageManager)
        , engine()
        , fileRoot(fileRoot)
    {
    }

    LoggerProxy innerLogger;
    PackageManagerProxy innerPackageManager;
    std::optional<rust::Box<EngineImpl>> engine;
    QString fileRoot;
    QByteArray pdfBuffer;
};

Engine::Engine(const QString& filePath, Logger* logger, PackageManager* packageManager, QObject* parent)
    : QObject(parent)
{
    QFileInfo info { filePath };
    d_ptr.reset(new EnginePrivate(this, logger, packageManager, info.canonicalPath()));
}

Engine::~Engine()
{
}

QByteArray Engine::pdfBuffer() const
{
    return d_ptr->pdfBuffer;
}

void Engine::init()
{
    if (d_ptr->engine.has_value()) {
        return;
    }

    QString instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    d_ptr->engine = create_engine_impl(
        d_ptr->innerLogger,
        d_ptr->innerPackageManager,
        qstringToRust(instanceId),
        qstringToRust(d_ptr->fileRoot)
    );

    Q_EMIT initialized();
}

void Engine::compile(const QString& source)
{
    Q_ASSERT(d_ptr->engine.has_value());

    rust::Vec<uint8_t> pdf = d_ptr->engine.value()->compile(qstringToRust(source));
    if (!pdf.empty()) {
        d_ptr->pdfBuffer.clear();
        d_ptr->pdfBuffer.append(reinterpret_cast<char*>(pdf.data()), pdf.size());
        Q_EMIT previewReady(d_ptr->pdfBuffer);
    }
}

}
