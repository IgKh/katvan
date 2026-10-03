/*
 * This file is part of Katvan
 * Copyright (c) 2024 - 2026 Igor Khanin
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

#include <QObject>
#include <QSet>

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
QT_END_NAMESPACE

namespace katvan {

class PersonalDictionary : public QObject
{
    Q_OBJECT

public:
    PersonalDictionary(QObject* parent = nullptr);

    static void setDictionaryLocation(const QString& dirPath);

    bool isWordInDictionary(const QString& word) const;
    bool isNormalizedWordInDictionary(const QString& normalizedWord) const;
    void addToDictionary(const QString& word);

private slots:
    void dictionaryFileChanged();

private:
    void flushDictionary();
    void loadDictionary();
    void setDictionaryPath();

    static QString s_dictionaryLocation;

    QString d_path;
    QSet<QString> d_words;

    QFileSystemWatcher* d_watcher;
};

}
