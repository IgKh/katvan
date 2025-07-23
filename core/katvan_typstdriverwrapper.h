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
#pragma once

#include "typstdriver_engine.h"
#include "typstdriver_logger.h"

#include <QList>
#include <QObject>
#include <QSet>

#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace katvan {

class DiagnosticsModel;

namespace typstdriver {
class PackageManager;
class TypstCompilerSettings;
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
    DiagnosticsModel* diagnosticsModel() { return d_diagnosticsModel; }

    void setCompilerSettings(const typstdriver::TypstCompilerSettings& settings);
    void resetInputFile(const QString& sourceFileName);

signals:
    void previewReady(QList<katvan::typstdriver::PreviewPageData> pages);
    void compilationStatusChanged();
    void pageRendered(int page, QImage renderedPage);
    void exportFinished(bool success);
    void jumpToPreview(int page, QPointF pos);
    void jumpToEditor(int line, int column);
    void showEditorToolTip(QPoint pos, QString toolTip, QUrl detailsUrl);
    void showEditorToolTipAtLocation(int line, int column, QString toolTip, QUrl detailsUrl);
    void completionsReady(int line, int column, QByteArray completionsJson);
    void outlineUpdated(katvan::typstdriver::OutlineNode* outline);
    void labelsUpdated(QList<katvan::typstdriver::DocumentLabel> labels);
    void pageWordCountUpdated(int page, size_t wordCount);
    void symbolsJsonReady(QByteArray symbols);

public slots:
    void setSource(const QString& text);
    void applyContentEdit(int from, int to, QString text);
    void updatePreview();
    void renderPage(int page, qreal pageSize);
    void exportToPdf(const QString& filePath);
    void forwardSearch(int line, int column, int currentPreviewPage);
    void inverseSearch(int page, QPointF clickPoint);
    void requestToolTip(int line, int column, QPoint pos);
    void requestCompletions(int line, int column, bool implicit);
    void searchDefinition(int line, int column);
    void requestPageWordCount(int page);
    void requestAllSymbolsJson();
    void discardLookupCaches();

private slots:
    void compilationFinished();
    void pageRenderComplete(int page, QImage renderedPage);
    void metadataUpdatedInternal(quint64 fingerprint, katvan::typstdriver::OutlineNode* outline, QList<katvan::typstdriver::DocumentLabel> labels);

private:
    struct PendingEdit
    {
        int from;
        int to;
        QString text;
    };

    typstdriver::Engine* d_engine;
    typstdriver::Logger* d_compilerLogger;
    typstdriver::PackageManager* d_packageManager;

    DiagnosticsModel* d_diagnosticsModel;
    QThread* d_thread;

    Status d_status;
    std::shared_ptr<typstdriver::TypstCompilerSettings> d_settings;
    std::optional<QString> d_pendingSource;
    QList<PendingEdit> d_pendingEdits;
    quint64 d_lastMetadataFingerprint;

    QSet<int> d_pendingPagesToRender;
};

}
