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

#include "typstdriver_export.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QUrl>

#include <memory>
#include <utility>

namespace katvan::typstdriver {

class Logger;
class OutlineNode;
class PackageManager;
class TypstCompilerSettings;

struct TYPSTDRIVER_EXPORT PreviewPageData
{
    int pageNumber;
    QSizeF sizeInPoints;
    quint64 fingerprint;
};

using DocumentLabel = std::tuple<QString, int, int>;

class TYPSTDRIVER_EXPORT Engine : public QObject
{
    Q_OBJECT

public:
    Engine(const QString& filePath, Logger* logger, PackageManager* packageManager, QObject* parent = nullptr);
    ~Engine();

    static QString typstVersion();

signals:
    void initialized();
    void compilationFinished();
    void previewReady(QList<katvan::typstdriver::PreviewPageData> pages);
    void pageRendered(int page, QImage renderedPage);
    void exportFinished(bool success);
    void jumpToPreview(int page, QPointF pos);
    void jumpToEditor(int line, int column);
    void toolTipReady(QPoint pos, QString toolTip, QUrl detailsUrl);
    void toolTipForLocation(int line, int column, QString toolTip, QUrl detailsUrl);
    void completionsReady(int line, int column, QByteArray completionsJson);
    void metadataUpdated(quint64 fingerprint, katvan::typstdriver::OutlineNode* outline, QList<katvan::typstdriver::DocumentLabel> labels);
    void pageWordCountUpdated(int page, size_t wordCount);
    void symbolsJsonReady(QByteArray symbols);

public slots:
    void init();
    void setSource(const QString& text);
    void applyContentEdit(int from, int to, const QString& text);
    void compile();
    void renderPage(int page, qreal pointSize);
    void exportToPdf(const QString& outputFile, const QString& pdfVersion, const QString& pdfaStandard, bool tagged);
    void exportToPng(const QString& outputFile, int dpi);
    void exportToPngMulti(const QString& outputDir, const QString& filePattern, int dpi);
    void forwardSearch(int line, int column, int currentPreviewPage);
    void inverseSearch(int page, QPointF clickPoint);
    void requestToolTip(int line, int column, QPoint pos);
    void requestCompletions(int line, int column, bool implicit);
    void searchDefinition(int line, int column);
    void requestMetadata(quint64 previousFingerprint);
    void requestPageWordCount(int page);
    void requestAllSymbolsJson();
    void applySettings(const TypstCompilerSettings& settings);
    void discardLookupCaches();

private:
    struct EnginePrivate;
    std::unique_ptr<EnginePrivate> d_ptr;
};

}

Q_DECLARE_METATYPE(katvan::typstdriver::PreviewPageData)
