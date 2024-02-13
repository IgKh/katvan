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

#include <QCache>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

#include <map>
#include <memory>

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
class QThread;
QT_END_NAMESPACE

class Hunspell;

namespace katvan {

struct LoadedSpeller;

class SpellChecker : public QObject
{
    Q_OBJECT

public:
    SpellChecker(QObject* parent = nullptr);
    ~SpellChecker();

    static QMap<QString, QString> findDictionaries();
    static QString dictionaryDisplayName(const QString& dictName);
    static void setPersonalDictionaryLocation(const QString& dirPath);

    QString currentDictionaryName() const { return d_currentDictName; }
    void setCurrentDictionary(const QString& dictName, const QString& dictAffFile);

    QList<std::pair<size_t, size_t>> checkSpelling(const QString& text);

    void addToPersonalDictionary(const QString& word);

    void requestSuggestions(const QString& word, int position);

signals:
    void suggestionsReady(const QString& word, int position, const QStringList& suggestions);

private slots:
    void personalDictionaryFileChanged();
    void suggestionsWorkerDone(QString word, int position, QStringList suggestions);

private:
    bool checkWord(Hunspell& speller, const QString& word);
    void flushPersonalDictionary();
    void loadPersonalDictionary();

    static QString s_personalDictionaryLocation;

    QString d_currentDictName;
    QCache<QString, QStringList> d_suggestionsCache;

    QString d_personalDictionaryPath;
    QSet<QString> d_personalDictionary;

    QFileSystemWatcher* d_watcher;
    QThread* d_suggestionThread;

    std::map<QString, std::unique_ptr<LoadedSpeller>> d_spellers;
};

class SpellingSuggestionsWorker : public QObject
{
    Q_OBJECT

public:
    SpellingSuggestionsWorker(LoadedSpeller* speller, const QString& word, int position)
        : d_speller(speller)
        , d_word(word)
        , d_pos(position) {}

public slots:
    void process();

signals:
    void suggestionsReady(QString word, int position, QStringList suggestions);

private:
    LoadedSpeller* d_speller;
    QString d_word;
    int d_pos;
};

}
