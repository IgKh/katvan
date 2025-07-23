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
#include "katvan_typstdriverwrapper.h"
#include "katvan_wordcounter.h"

#include <QTimer>

namespace katvan {

WordCounter::WordCounter(TypstDriverWrapper* driver, QObject* parent)
    : QObject(parent)
    , d_driver(driver)
{
    connect(d_driver, &TypstDriverWrapper::previewReady, this, &WordCounter::startRecalculateCount);
    connect(d_driver, &TypstDriverWrapper::pageWordCountUpdated, this, &WordCounter::pageWordCountUpdated);

    d_updateTimer = new QTimer(this);
    d_updateTimer->setSingleShot(true);
    d_updateTimer->setInterval(50);
    connect(d_updateTimer, &QTimer::timeout, this, &WordCounter::calculateTotalWordCount);
}

void WordCounter::reset()
{
    d_pageWordCounts.clear();

    Q_EMIT wordCountChanged(0);
}

void WordCounter::startRecalculateCount(QList<katvan::typstdriver::PreviewPageData> pages)
{
    d_pageWordCounts.resize(pages.size());
    for (qsizetype i = 0; i < d_pageWordCounts.size(); i++) {
        if (d_pageWordCounts[i].fingerprint != pages[i].fingerprint) {
            d_driver->requestPageWordCount(i);
        }
        d_pageWordCounts[i].fingerprint = pages[i].fingerprint;
    }
}

void WordCounter::pageWordCountUpdated(int page, size_t wordCount)
{
    if (page < 0 || page >= d_pageWordCounts.size()) {
        return;
    }
    d_pageWordCounts[page].count = wordCount;

    d_updateTimer->start();
}

void WordCounter::calculateTotalWordCount()
{
    size_t totalCount = 0;
    for (const auto& page : std::as_const(d_pageWordCounts)) {
        totalCount += page.count;
    }

    Q_EMIT wordCountChanged(totalCount);
}

}

#include "moc_katvan_wordcounter.cpp"
