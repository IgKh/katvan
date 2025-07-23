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

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class TypstDriverWrapper;

class WordCounter : public QObject {
    Q_OBJECT

public:
    WordCounter(TypstDriverWrapper* driver, QObject* parent = nullptr);

signals:
    void wordCountChanged(size_t count);

public slots:
    void reset();

private slots:
    void startRecalculateCount(QList<katvan::typstdriver::PreviewPageData> pages);
    void pageWordCountUpdated(int page, size_t wordCount);
    void calculateTotalWordCount();

private:
    TypstDriverWrapper* d_driver;
    QTimer* d_updateTimer;

    struct PageWordCount {
        PageWordCount()
            : fingerprint(0)
            , count(0) {}

        PageWordCount(quint64 fingerprint, size_t count)
            : fingerprint(fingerprint)
            , count(count) {}

        quint64 fingerprint;
        size_t count;
    };
    QList<PageWordCount> d_pageWordCounts;
};

}
