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
#pragma once

#include "typstdriver_engine.h"
#include "typstdriver_logger.h"

#include <QList>
#include <QObject>
#include <QSet>

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace katvan {

class DiagnosticsModel;

namespace typstdriver {
class PackageManager;
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

    void resetInputFile(const QString& sourceFileName);

signals:
    void previewReady(QList<katvan::typstdriver::PreviewPageData> pages);
    void compilationStatusChanged();
    void pageRendered(int page, QImage renderedPage);
    void exportFinished(QString errorMessage);
    void jumpToPreview(int page, QPointF pos);
    void jumpToEditor(int line, int column);

public slots:
    void updatePreview(const QString& source);
    void renderPage(int page, qreal pageSize);
    void exportToPdf(const QString& filePath);
    void forwardSearch(int line, int column);
    void inverseSearch(int page, QPointF clickPoint);

private slots:
    void compilationFinished();
    void pageRenderComplete(int page, QImage renderedPage);

private:
    typstdriver::Engine* d_engine;
    typstdriver::Logger* d_compilerLogger;
    typstdriver::PackageManager* d_packageManager;

    DiagnosticsModel* d_diagnosticsModel;
    QThread* d_thread;

    Status d_status;
    QString d_pendingSourceToCompile;
    QSet<int> d_pendingPagesToRender;
};

}
