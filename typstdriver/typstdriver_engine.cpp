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
#include "typstdriver_engine.h"
#include "typstdriver_logger.h"
#include "typstdriver_outlinenode.h"

#include "typstdriver_ffi/bridge.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTimeZone>

#include <algorithm>
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

void Engine::setSource(const QString& text)
{
    Q_ASSERT(d_ptr->engine.has_value());

    d_ptr->engine.value()->set_source(qstringToRust(text));
}

void Engine::applyContentEdit(int from, int to, const QString& text)
{
    Q_ASSERT(d_ptr->engine.has_value());

    d_ptr->engine.value()->apply_content_edit(
        static_cast<size_t>(from),
        static_cast<size_t>(to),
        qstringToRust(text));
}

void Engine::compile()
{
    Q_ASSERT(d_ptr->engine.has_value());

    QString now = QDateTime::currentDateTime(QTimeZone::systemTimeZone()).toString(Qt::ISODate);

    rust::Vec<PreviewPageDataInternal> pages = d_ptr->engine.value()->compile(qstringToRust(now));
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
    Q_EMIT compilationFinished();
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
        bool ok = d_ptr->engine.value()->export_pdf(qstringToRust(outputFile));
        Q_EMIT exportFinished(ok);
    }
    catch (rust::Error& e) {
        qWarning() << "Error exporting to PDF" << outputFile << ":" << e.what();
    }
}

void Engine::forwardSearch(int line, int column, int currentPreviewPage)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        rust::Vec<PreviewPosition> result = d_ptr->engine.value()->forward_search(
            static_cast<size_t>(line),
            static_cast<size_t>(column));

        // There can be multiple positions returned since the same syntax node
        // can appear in multiple places in the rendered document (e.g. a header
        // can also appear in a TOC). We should pick the one closest to the
        // current scroll position of the previewer; this is a lame implementation
        // of that.
        auto comp = [currentPreviewPage](const PreviewPosition& lhs, const PreviewPosition& rhs) {
            int lhsDistance = qAbs(static_cast<int>(lhs.page) - currentPreviewPage);
            int rhsDistance = qAbs(static_cast<int>(rhs.page) - currentPreviewPage);
            return lhsDistance < rhsDistance;
        };

        auto it = std::min_element(result.begin(), result.end(), comp);
        if (it != result.end()) {
            Q_EMIT jumpToPreview(it->page, QPointF(it->x_pts, it->y_pts));
        }
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

void Engine::requestToolTip(int line, int column, QPoint pos)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        ToolTip toolTip = d_ptr->engine.value()->get_tooltip(
            static_cast<size_t>(line),
            static_cast<size_t>(column));

        QString content = QString::fromUtf8(toolTip.content.data(), toolTip.content.size());
        QString url = QString::fromUtf8(toolTip.details_url.data(), toolTip.details_url.size());

        if (!pos.isNull()) {
            Q_EMIT toolTipReady(pos, content, url);
        }
        else {
            Q_EMIT toolTipForLocation(line, column, content, url);
        }
    }
    catch (rust::Error&) {
    }
}

void Engine::requestCompletions(int line, int column, bool implicit)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        Completions result = d_ptr->engine.value()->get_completions(
            static_cast<size_t>(line),
            static_cast<size_t>(column),
            implicit);

        QByteArray json { result.completions_json.data(), static_cast<qsizetype>(result.completions_json.size()) };

        Q_EMIT completionsReady(result.from.line, result.from.column, json);
    }
    catch (rust::Error& e) {
        // The completion manager is waiting for something, so even on failure
        // send an empty reply.
        Q_EMIT completionsReady(-1, -1, QByteArrayLiteral("[]"));
    }
}

void Engine::searchDefinition(int line, int column)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        DefinitionLocation result = d_ptr->engine.value()->get_definition(
            static_cast<size_t>(line),
            static_cast<size_t>(column));

        if (!result.in_std) {
            Q_EMIT jumpToEditor(result.position.line, result.position.column);
        }
        else {
            requestToolTip(line, column, QPoint());
        }
    }
    catch (rust::Error&) {
    }
}

void Engine::requestMetadata(quint64 previousFingerprint)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        DocumentMetadata metadata = d_ptr->engine.value()->get_metadata();
        if (metadata.fingerprint == previousFingerprint) {
            return;
        }

        std::span outlineEntries { metadata.outline.data(), metadata.outline.size() };
        OutlineNode* outline = new OutlineNode(outlineEntries);

        QList<DocumentLabel> labels;
        labels.reserve(metadata.labels.size());

        for (const auto& label : metadata.labels) {
            QString name = QString::fromUtf8(label.name.data(), label.name.size());

            if (label.has_position) {
                labels.push_back(std::make_tuple(name, label.position.line, label.position.column));
            }
            else {
                labels.push_back(std::make_tuple(name, -1, -1));
            }
        }

        Q_EMIT metadataUpdated(metadata.fingerprint, outline, labels);
    }
    catch (rust::Error&) {
    }
}

void Engine::requestPageWordCount(int page)
{
    Q_ASSERT(d_ptr->engine.has_value());

    try {
        size_t count = d_ptr->engine.value()->count_page_words(static_cast<size_t>(page));

        Q_EMIT pageWordCountUpdated(page, count);
    }
    catch (rust::Error&) {
    }
}

void Engine::requestAllSymbolsJson()
{
    try {
        rust::String symbolsJson = get_all_symbols_json();
        QByteArray result { symbolsJson.data(), static_cast<qsizetype>(symbolsJson.size()) };

        Q_EMIT symbolsJsonReady(result);
    }
    catch (rust::Error& e) {
        qWarning() << "Error getting supported symbols:" << e.what();
    }
}

void Engine::setAllowedPaths(const QStringList& allowedPaths)
{
    Q_ASSERT(d_ptr->engine.has_value());

    rust::Vec<rust::String> paths;
    paths.reserve(allowedPaths.size());

    for (const QString& path : allowedPaths) {
        paths.push_back(qstringToRust(path));
    }
    d_ptr->engine.value()->set_allowed_paths(paths);
}

void Engine::discardLookupCaches()
{
    Q_ASSERT(d_ptr->engine.has_value());
    d_ptr->engine.value()->discard_lookup_caches();
}

}

#include "moc_typstdriver_engine.cpp"
