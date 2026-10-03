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
class QThread;
QT_END_NAMESPACE

class Hunspell;

namespace katvan {

struct LoadedSpeller;

class PersonalDictionary;

class HunspellSpellChecker : public SpellChecker
{
    Q_OBJECT

public:
    HunspellSpellChecker(QObject* parent = nullptr);
    ~HunspellSpellChecker();

    QMap<QString, QString> findDictionaries() override;
    void setCurrentDictionaries(const QList<DictionaryDef>& dicts) override;

    MisspelledWordRanges checkSpelling(const QString& text) override;

    bool addToPersonalDictionary(const QString& word) override;

private slots:
    void loaderWorkerDone(const QMap<QString, katvan::LoadedSpeller*>& spellers);

private:
    void ensureWorkerThread();
    bool checkWord(const QString& word);

    void requestSuggestionsImpl(const QString& word, int position) override;

    PersonalDictionary* d_personalDictionary;
    QThread* d_workerThread;

    std::map<QString, std::unique_ptr<LoadedSpeller>> d_spellers;
};

class DictionaryLoaderWorker : public QObject
{
    Q_OBJECT

public:
    DictionaryLoaderWorker(const QList<SpellChecker::DictionaryDef>& dicts)
        : d_dicts(dicts) {}

public slots:
    void process();

signals:
    void dictionariesLoaded(QMap<QString, katvan::LoadedSpeller*> spellers);

private:
    QList<SpellChecker::DictionaryDef> d_dicts;
};

class SpellingSuggestionsWorker : public QObject
{
    Q_OBJECT

public:
    SpellingSuggestionsWorker(QList<LoadedSpeller*> spellers, const QString& word, int position)
        : d_spellers(spellers)
        , d_word(word)
        , d_pos(position) {}

public slots:
    void process();

signals:
    void suggestionsReady(QString word, int position, QStringList suggestions);

private:
    QList<LoadedSpeller*> d_spellers;
    QString d_word;
    int d_pos;
};

}
