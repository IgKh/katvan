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

#include "katvan_spellchecker.h"

#include <QSet>

#include <map>
#include <memory>

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
class QThread;
QT_END_NAMESPACE

class Hunspell;

namespace katvan {

struct LoadedSpeller;

class HunspellSpellChecker : public SpellChecker
{
    Q_OBJECT

public:
    HunspellSpellChecker(QObject* parent = nullptr);
    ~HunspellSpellChecker();

    QMap<QString, QString> findDictionaries() override;
    void setPersonalDictionaryLocation(const QString& dirPath) override;

    void setCurrentDictionary(const QString& dictName, const QString& dictAffFile) override;

    MisspelledWordRanges checkSpelling(const QString& text) override;

    void addToPersonalDictionary(const QString& word) override;

private slots:
    void personalDictionaryFileChanged();
    void loaderWorkerDone(QString dictName, katvan::LoadedSpeller* speller);

private:
    void ensureWorkerThread();
    bool checkWord(Hunspell& speller, QChar::Script dictionaryScript, const QString& word);
    void flushPersonalDictionary();
    void loadPersonalDictionary();
    void setPersonalDictionaryPath();

    void requestSuggestionsImpl(const QString& word, int position) override;

    static QString s_personalDictionaryLocation;

    QString d_personalDictionaryPath;
    QSet<QString> d_personalDictionary;

    QFileSystemWatcher* d_watcher;
    QThread* d_workerThread;

    std::map<QString, std::unique_ptr<LoadedSpeller>> d_spellers;
};

class DictionaryLoaderWorker : public QObject
{
    Q_OBJECT

public:
    DictionaryLoaderWorker(const QString& dictName, const QString& dictAffFile)
        : d_dictName(dictName)
        , d_dictAffFile(dictAffFile) {}

public slots:
    void process();

signals:
    void dictionaryLoaded(QString dictName, katvan::LoadedSpeller* speller);

private:
    QString d_dictName;
    QString d_dictAffFile;
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
