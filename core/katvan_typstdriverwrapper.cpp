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
#include "katvan_diagnosticsmodel.h"
#include "katvan_typstdriverwrapper.h"

#include "typstdriver_compilersettings.h"
#include "typstdriver_packagemanager.h"

#include <QDebug>
#include <QThread>

#include <optional>

namespace katvan {

TypstDriverWrapper::TypstDriverWrapper(QObject* parent)
    : QObject(parent)
    , d_engine(nullptr)
    , d_status(Status::INITIALIZING)
    , d_settings(std::make_shared<typstdriver::TypstCompilerSettings>())
    , d_lastMetadataFingerprint(0)
{
    d_thread = new QThread(this);
    d_thread->setObjectName("TypstDriverThread");
    d_thread->start();

    d_diagnosticsModel = new DiagnosticsModel(this);

    d_compilerLogger = new typstdriver::Logger();
    d_compilerLogger->moveToThread(d_thread);
    connect(d_compilerLogger, &typstdriver::Logger::diagnosticLogged, d_diagnosticsModel, &DiagnosticsModel::addDiagnostic);

    d_packageManager = new typstdriver::PackageManager(d_compilerLogger);
    d_packageManager->moveToThread(d_thread);
}

TypstDriverWrapper::~TypstDriverWrapper()
{
    if (d_engine) {
        d_engine->deleteLater();
    }

    d_packageManager->deleteLater();
    d_compilerLogger->deleteLater();

    if (d_thread->isRunning()) {
        d_thread->quit();
        d_thread->wait();
    }
}

QString TypstDriverWrapper::typstVersion()
{
    return typstdriver::Engine::typstVersion();
}

void TypstDriverWrapper::setCompilerSettings(const typstdriver::TypstCompilerSettings& settings)
{
    d_settings = std::make_shared<typstdriver::TypstCompilerSettings>(settings);
    d_packageManager->applySettings(d_settings);

    if (d_status != Status::INITIALIZING) {
        QMetaObject::invokeMethod(d_engine, "setAllowedPaths", d_settings->allowedPaths());
    }
}

void TypstDriverWrapper::resetInputFile(const QString& sourceFileName)
{
    d_status = Status::INITIALIZING;
    Q_EMIT compilationStatusChanged();

    d_diagnosticsModel->setInputFileName(sourceFileName);

    if (d_engine != nullptr) {
        d_engine->deleteLater();
    }

    d_engine = new typstdriver::Engine(sourceFileName, d_compilerLogger, d_packageManager);
    d_engine->moveToThread(d_thread);

    auto onInitialized = [this]() {
        d_status = Status::INITIALIZED;
        Q_EMIT compilationStatusChanged();

        QMetaObject::invokeMethod(d_engine, "setAllowedPaths", d_settings->allowedPaths());

        bool hasPending = false;
        if (d_pendingSource) {
            setSource(d_pendingSource.value());

            d_pendingSource = std::nullopt;
            hasPending = true;
        }
        if (!d_pendingEdits.isEmpty()) {
            for (const auto& edit : std::as_const(d_pendingEdits)) {
                applyContentEdit(edit.from, edit.to, edit.text);
            }

            d_pendingEdits.clear();
            hasPending = true;
        }

        if (hasPending) {
            updatePreview();
        }
    };

    connect(d_engine, &typstdriver::Engine::compilationFinished, this, &TypstDriverWrapper::compilationFinished);
    connect(d_engine, &typstdriver::Engine::previewReady, this, &TypstDriverWrapper::previewReady);
    connect(d_engine, &typstdriver::Engine::pageRendered, this, &TypstDriverWrapper::pageRenderComplete);
    connect(d_engine, &typstdriver::Engine::exportFinished, this, &TypstDriverWrapper::exportFinished);
    connect(d_engine, &typstdriver::Engine::jumpToPreview, this, &TypstDriverWrapper::jumpToPreview);
    connect(d_engine, &typstdriver::Engine::jumpToEditor, this, &TypstDriverWrapper::jumpToEditor);
    connect(d_engine, &typstdriver::Engine::toolTipReady, this, &TypstDriverWrapper::showEditorToolTip);
    connect(d_engine, &typstdriver::Engine::toolTipForLocation, this, &TypstDriverWrapper::showEditorToolTipAtLocation);
    connect(d_engine, &typstdriver::Engine::completionsReady, this, &TypstDriverWrapper::completionsReady);
    connect(d_engine, &typstdriver::Engine::metadataUpdated, this, &TypstDriverWrapper::metadataUpdatedInternal);
    connect(d_engine, &typstdriver::Engine::symbolsJsonReady, this, &TypstDriverWrapper::symbolsJsonReady);
    connect(d_engine, &typstdriver::Engine::initialized, this, onInitialized, Qt::SingleShotConnection);

    QMetaObject::invokeMethod(d_engine, "init");
}

void TypstDriverWrapper::setSource(const QString& text)
{
    if (d_status == Status::INITIALIZING) {
        d_pendingEdits.clear();
        d_pendingSource = text;
        return;
    }

    QMetaObject::invokeMethod(d_engine, "setSource", text);
}

void TypstDriverWrapper::applyContentEdit(int from, int to, QString text)
{
    if (d_status == Status::INITIALIZING) {
        d_pendingEdits.append(PendingEdit { from, to, text });
        return;
    }

    QMetaObject::invokeMethod(d_engine, "applyContentEdit", from, to, text);
}

void TypstDriverWrapper::updatePreview()
{
    if (d_status == Status::PROCESSING) {
        qDebug() << "Compiler already processing, skipping";
        return;
    }
    else if (d_status == Status::INITIALIZING) {
        return;
    }

    d_status = Status::PROCESSING;
    Q_EMIT compilationStatusChanged();

    d_diagnosticsModel->clear();
    QMetaObject::invokeMethod(d_engine, "compile");
}

void TypstDriverWrapper::renderPage(int page, qreal pointSize, bool invertColors)
{
    if (d_pendingPagesToRender.contains(page)) {
        return;
    }

    d_pendingPagesToRender.insert(page);
    QMetaObject::invokeMethod(d_engine, "renderPage", page, pointSize, invertColors);
}

void TypstDriverWrapper::exportToPdf(const QString& filePath)
{
    d_diagnosticsModel->clear();
    QMetaObject::invokeMethod(d_engine, "exportToPdf", filePath);
}

void TypstDriverWrapper::forwardSearch(int line, int column, int currentPreviewPage)
{
    QMetaObject::invokeMethod(d_engine, "forwardSearch", line, column, currentPreviewPage);
}

void TypstDriverWrapper::inverseSearch(int page, QPointF clickPoint)
{
    QMetaObject::invokeMethod(d_engine, "inverseSearch", page, clickPoint);
}

void TypstDriverWrapper::requestToolTip(int line, int column, QPoint pos)
{
    QMetaObject::invokeMethod(d_engine, "requestToolTip", line, column, pos);
}

void TypstDriverWrapper::requestCompletions(int line, int column, bool implicit)
{
    QMetaObject::invokeMethod(d_engine, "requestCompletions", line, column, implicit);
}

void TypstDriverWrapper::searchDefinition(int line, int column)
{
    QMetaObject::invokeMethod(d_engine, "searchDefinition", line, column);
}

void TypstDriverWrapper::requestAllSymbolsJson()
{
    QMetaObject::invokeMethod(d_engine, "requestAllSymbolsJson");
}

void TypstDriverWrapper::discardLookupCaches()
{
    QMetaObject::invokeMethod(d_engine, "discardLookupCaches");
}

void TypstDriverWrapper::compilationFinished()
{
    d_status = d_diagnosticsModel->impliedStatus();
    Q_EMIT compilationStatusChanged();

    if (d_status == Status::SUCCESS || d_status == Status::SUCCESS_WITH_WARNINGS) {
        // TODO: Possibly throttle this
        QMetaObject::invokeMethod(d_engine, "requestMetadata", d_lastMetadataFingerprint);
    }
}

void TypstDriverWrapper::pageRenderComplete(int page, QImage renderedPage)
{
    d_pendingPagesToRender.remove(page);
    Q_EMIT pageRendered(page, renderedPage);
}

void TypstDriverWrapper::metadataUpdatedInternal(
    quint64 fingerprint,
    katvan::typstdriver::OutlineNode* outline,
    QList<katvan::typstdriver::DocumentLabel> labels)
{
    d_lastMetadataFingerprint = fingerprint;
    Q_EMIT outlineUpdated(outline);
    Q_EMIT labelsUpdated(labels);

}

}

#include "moc_katvan_typstdriverwrapper.cpp"
