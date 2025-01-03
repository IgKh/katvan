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

#include <memory>

namespace katvan::typstdriver {

class Logger;
class PackageManager;

struct TYPSTDRIVER_EXPORT PreviewPageData
{
    int pageNumber;
    QSizeF sizeInPoints;
    quint64 fingerprint;
};

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
    void toolTipReady(QPoint pos, QString toolTip);
    void completionsReady(int line, int column, QByteArray completionsJson);

public slots:
    void init();
    void setSource(const QString& text);
    void applyContentEdit(int from, int to, const QString& text);
    void compile();
    void renderPage(int page, qreal pointSize, bool invertColors);
    void exportToPdf(const QString& outputFile);
    void forwardSearch(int line, int column, int currentPreviewPage);
    void inverseSearch(int page, QPointF clickPoint);
    void requestToolTip(int line, int column, QPoint pos);
    void requestCompletions(int line, int column);
    void discardLookupCaches();

private:
    struct EnginePrivate;
    std::unique_ptr<EnginePrivate> d_ptr;
};

}

Q_DECLARE_METATYPE(katvan::typstdriver::PreviewPageData)
