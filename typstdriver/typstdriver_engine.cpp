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

QString Engine::typstVersion()
{
    rust::String version = typst_version();
    return QString::fromUtf8(version.data(), version.size());
}

void Engine::init()
{
    if (d_ptr->engine.has_value()) {
        return;
    }

    d_ptr->engine = create_engine_impl(
        d_ptr->innerLogger,
        d_ptr->innerPackageManager,
        qstringToRust(d_ptr->fileRoot)
    );

    Q_EMIT initialized();
}

void Engine::compile(const QString& source)
{
    Q_ASSERT(d_ptr->engine.has_value());

    rust::Vec<PreviewPageDataInternal> pages = d_ptr->engine.value()->compile(qstringToRust(source));
    if (!pages.empty()) {
        QList<PreviewPageData> result;
        result.reserve(pages.size());
        for (PreviewPageDataInternal& data : pages) {
            result.append(PreviewPageData {
                static_cast<int>(data.page_num),
                QSizeF(data.width_pts, data.height_pts)
            });
        }

        Q_EMIT previewReady(result);
    }
}

static void cleanupBuffer(void* buffer)
{
    unsigned char* buf = reinterpret_cast<unsigned char*>(buffer);
    delete[] buf;
}

void Engine::renderPage(int page, qreal pointSize)
{
    Q_ASSERT(d_ptr->engine.has_value());

    RenderedPage result = d_ptr->engine.value()->render_page(page, pointSize);

    if (!result.buffer.empty()) {
        unsigned char* buffer = new unsigned char[result.buffer.size()];
        memcpy(buffer, result.buffer.data(), result.buffer.size());

        QImage image {
            buffer,
            static_cast<int>(result.width_px),
            static_cast<int>(result.height_px),
            QImage::Format_RGBA8888_Premultiplied, // Per tiny-skia's documentation
            cleanupBuffer,
            buffer
        };
        Q_EMIT pageRendered(page, image);
    }
}

void Engine::exportToPdf(const QString& outputFile)
{
    Q_ASSERT(d_ptr->engine.has_value());

    rust::String error = d_ptr->engine.value()->export_pdf(qstringToRust(outputFile));
    if (error.empty()) {
        Q_EMIT exportFinished(QString());
    }
    else {
        Q_EMIT exportFinished(QString::fromUtf8(error.data(), error.size()));
    }
}

}
