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

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTimeZone>

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
    QString rootPath;
    if (!filePath.isEmpty()) {
        QFileInfo info { filePath };
        rootPath = info.dir().canonicalPath();
    }

    d_ptr.reset(new EnginePrivate(this, logger, packageManager, rootPath));
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

    QString now = QDateTime::currentDateTime(QTimeZone::systemTimeZone()).toString(Qt::ISODate);

    rust::Vec<PreviewPageDataInternal> pages = d_ptr->engine.value()->compile(qstringToRust(source), qstringToRust(now));
    if (!pages.empty()) {
        QList<PreviewPageData> result;
        result.reserve(pages.size());
        for (PreviewPageDataInternal& data : pages) {
            result.append(PreviewPageData {
                static_cast<int>(data.page_num),
                QSizeF(data.width_pts, data.height_pts),
                data.fingerprint
            });
        }

        Q_EMIT previewReady(result);
    }
}

static void cleanupBuffer(void* buffer)
{
    auto* buf = reinterpret_cast<rust::Vec<uint8_t>*>(buffer);
    delete buf;
}

void Engine::renderPage(int page, qreal pointSize)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        RenderedPage result = d_ptr->engine.value()->render_page(page, pointSize);
        rust::Vec<uint8_t>* buffer = new rust::Vec<uint8_t>(std::move(result.buffer));

        QImage image {
            buffer->data(),
            static_cast<int>(result.width_px),
            static_cast<int>(result.height_px),
            QImage::Format_RGBA8888_Premultiplied, // Per tiny-skia's documentation
            cleanupBuffer,
            buffer
        };
        Q_EMIT pageRendered(page, image);
    }
    catch (rust::Error& e) {
        qWarning() << "Error rendering page" << page << ":" << e.what();
    }
}

void Engine::exportToPdf(const QString& outputFile)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        d_ptr->engine.value()->export_pdf(qstringToRust(outputFile));
        Q_EMIT exportFinished(QString());
    }
    catch (rust::Error& e) {
        Q_EMIT exportFinished(QString::fromUtf8(e.what(), -1));
    }
}

void Engine::forwardSearch(int line, int column)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        PreviewPosition result = d_ptr->engine.value()->foward_search(static_cast<size_t>(line), static_cast<size_t>(column));
        Q_EMIT jumpToPreview(result.page, QPointF(result.x_pts, result.y_pts));
    }
    catch (rust::Error&) {
    }
}

void Engine::inverseSearch(int page, QPointF clickPoint)
{
    Q_ASSERT(d_ptr->engine.has_value());

    PreviewPosition pos {
        static_cast<size_t>(page),
        clickPoint.x(),
        clickPoint.y()
    };

    try {
        SourcePosition result = d_ptr->engine.value()->inverse_search(pos);
        Q_EMIT jumpToEditor(result.line, result.column);
    }
    catch (rust::Error&) {
    }
}

}
