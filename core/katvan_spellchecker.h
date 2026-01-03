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

#include <QCache>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <utility>

namespace katvan {

class SpellChecker : public QObject
{
    Q_OBJECT

public:
    SpellChecker(QObject* parent = nullptr);
    ~SpellChecker();

    static SpellChecker* instance();

    virtual QMap<QString, QString> findDictionaries() = 0;
    virtual QString dictionaryDisplayName(const QString& dictName);
    virtual void setPersonalDictionaryLocation(const QString& dirPath);

    virtual QString currentDictionaryName() const { return d_currentDictName; }
    virtual void setCurrentDictionary(const QString& dictName, const QString& dictPath);

    using MisspelledWordRanges = QList<std::pair<size_t, size_t>>;
    virtual MisspelledWordRanges checkSpelling(const QString& text) = 0;
    virtual void addToPersonalDictionary(const QString& word) = 0;

    void requestSuggestions(const QString& word, int position);

signals:
    void dictionaryChanged(const QString& dictName);
    void suggestionsReady(const QString& word, int position, const QStringList& suggestions);

protected slots:
    void suggestionsCalculated(const QString& word, int position, const QStringList& suggestions);

protected:
    virtual void requestSuggestionsImpl(const QString& word, int position) = 0;

private:
    QString d_currentDictName;
    QCache<QString, QStringList> d_suggestionsCache;
};

}
