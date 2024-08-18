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

#include "typstdriver_export.h"

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
    void previewReady(QList<katvan::typstdriver::PreviewPageData> pages);
    void pageRendered(int page, QImage renderedPage);
    void exportFinished(QString errorMessage);

public slots:
    void init();
    void compile(const QString& source);
    void renderPage(int page, qreal pointSize);
    void exportToPdf(const QString& outputFile);

private:
    struct EnginePrivate;
    std::unique_ptr<EnginePrivate> d_ptr;
};

}

Q_DECLARE_METATYPE(katvan::typstdriver::PreviewPageData)
