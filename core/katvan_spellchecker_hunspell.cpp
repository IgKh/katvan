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
#include "katvan_spellchecker_hunspell.h"
#include "katvan_text_utils.h"

#include <hunspell.hxx>

#include <QApplication>
#include <QDir>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QMetaObject>
#include <QMutex>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextBoundaryFinder>
#include <QTextStream>
#include <QThread>

#include <mutex>

namespace katvan {

QString HunspellSpellChecker::s_personalDictionaryLocation;

static QMutex g_mutex;

struct LoadedSpeller
{
    LoadedSpeller(const char* affPath, const char* dicPath, QChar::Script script)
        : speller(affPath, dicPath)
        , encoder(speller.get_dic_encoding())
        , decoder(speller.get_dic_encoding())
        , script(script) {}

    Hunspell speller;
    QStringEncoder encoder;
    QStringDecoder decoder;
    QChar::Script script;
};

HunspellSpellChecker::HunspellSpellChecker(QObject* parent)
    : SpellChecker(parent)
{
    d_workerThread = new QThread(this);
    d_workerThread->setObjectName("HunspellWorkerThread");

    d_watcher = new QFileSystemWatcher(this);
    connect(d_watcher, &QFileSystemWatcher::fileChanged, this, &HunspellSpellChecker::personalDictionaryFileChanged);

    setPersonalDictionaryPath();
}

HunspellSpellChecker::~HunspellSpellChecker()
{
    if (d_workerThread->isRunning()) {
        d_workerThread->quit();
        d_workerThread->wait();
    }
}

void HunspellSpellChecker::ensureWorkerThread()
{
    if (!d_workerThread->isRunning()) {
        d_workerThread->start();
    }
}

void HunspellSpellChecker::setPersonalDictionaryLocation(const QString& dirPath)
{
    s_personalDictionaryLocation = dirPath;
}

/**
 * Scan system and executable-local locations for Hunspell dictionaries,
 * which are a pair of *.aff and *.dic files with the same base name.
 */
QMap<QString, QString> HunspellSpellChecker::findDictionaries()
{
    QStringList dictDirs;
    dictDirs.append(QCoreApplication::applicationDirPath() + "/hunspell");

    QStringList systemDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& dir : std::as_const(systemDirs)) {
        dictDirs.append(dir + "/hunspell");
    }

    QStringList nameFilters = { "*.aff" };
    QMap<QString, QString> affFiles;

    for (const QString& dirName : dictDirs) {
        QDir dir(dirName);
        QFileInfoList affixFiles = dir.entryInfoList(nameFilters, QDir::Files);

        for (QFileInfo& affInfo : affixFiles) {
            QString dictName = affInfo.baseName();
            QString dicFile = dirName + "/" + dictName + ".dic";
            if (!QFileInfo::exists(dicFile)) {
                continue;
            }

            if (!affFiles.contains(dictName)) {
                affFiles.insert(dictName, affInfo.absoluteFilePath());
            }
        }
    }
    return affFiles;
}

void HunspellSpellChecker::setCurrentDictionaries(const QList<DictionaryDef>& dicts)
{
    if (dicts.isEmpty()) {
        SpellChecker::setCurrentDictionaries(dicts);
        return;
    }

    DictionaryLoaderWorker* worker = new DictionaryLoaderWorker(dicts);
    ensureWorkerThread();
    worker->moveToThread(d_workerThread);

    connect(worker, &DictionaryLoaderWorker::dictionariesLoaded, this, &HunspellSpellChecker::loaderWorkerDone);
    QMetaObject::invokeMethod(worker, &DictionaryLoaderWorker::process, Qt::QueuedConnection);
}

static bool isSingleGrapheme(const QString& word)
{
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, word);
    qsizetype pos = finder.toNextBoundary();
    return (pos >= 0 && finder.toNextBoundary() < 0);
}

static bool isHebrewOrdinal(const QString& normalizedWord)
{
    if (normalizedWord.size() != 2) {
        return false;
    }

    QChar a = normalizedWord[0];
    QChar b = normalizedWord[1];

    return a.category() == QChar::Letter_Other
        && a.script() == QChar::Script_Hebrew
        && (b == QLatin1Char('\'') || b == QChar(0x05F3));
}

static QChar::Script dominantScriptForWord(const QString& word)
{
    for (const QChar& ch : word) {
        if (ch.script() > QChar::Script_Common) {
            return ch.script();
        }
    }
    return QChar::Script_Unknown;
}

static bool isAppropriateScriptForDictionary(QChar::Script dictionaryScript, const QString& word)
{
    if (dictionaryScript == QChar::Script_Unknown) {
        return true;
    }

    QChar::Script wordScript = dominantScriptForWord(word);
    return wordScript == QChar::Script_Unknown || wordScript == dictionaryScript;
}

static QChar::Script getDictionaryScript(const QString& dictName)
{
    QLocale locale(dictName);
    if (locale.language() == QLocale::C) {
        return QChar::Script_Unknown;
    }

    switch (locale.script()) {
    case QLocale::ArabicScript:     return QChar::Script_Arabic;
    case QLocale::CyrillicScript:   return QChar::Script_Cyrillic;
    case QLocale::HebrewScript:     return QChar::Script_Hebrew;
    case QLocale::LatinScript:      return QChar::Script_Latin;
    default:                        return QChar::Script_Unknown;
    }
}

bool HunspellSpellChecker::checkWord(const QString& word)
{
    QString normalizedWord = word.normalized(QString::NormalizationForm_D);
    if (d_personalDictionary.contains(normalizedWord)) {
        return true;
    }

    // Hunspell seems to emit a lot of false positives, so reduce them with
    // some heuristics:
    // - Ignore single Unicode grapheme words (single character, emoji, etc)
    // - Ignore Hebrew ordinals (single Hebrew letter followed by a Geresh)
    if (isSingleGrapheme(word) || isHebrewOrdinal(normalizedWord)) {
        return true;
    }

    bool hasNegative = false;

    for (const QString& dictName : currentDictionaryNames()) {
        LoadedSpeller* speller = d_spellers[dictName].get();

        if (!isAppropriateScriptForDictionary(speller->script, word)) {
            continue;
        }

        QByteArray encodedWord = speller->encoder.encode(word);
        if (speller->speller.spell(encodedWord.toStdString())) {
            return true;
        }
        hasNegative = true;
    }

    // The word is considered correct as long as at least one dictionary
    // accepted it, or no dictionary has explicitly rejected it. This is
    // to not mark words that are not relevant to any selected speller
    // as misspelled.
    return !hasNegative;
}

SpellChecker::MisspelledWordRanges HunspellSpellChecker::checkSpelling(const QString& text)
{
    MisspelledWordRanges result;
    if (currentDictionaryNames().isEmpty()) {
        return result;
    }

    std::unique_lock<QMutex> locker { g_mutex, std::defer_lock };
    if (!locker.try_lock()) {
        // Do not block the UI event loop! If we can't take the speller
        // lock (because suggestions are being generated at the moment),
        // just pretend there are no spelling mistakes here.
        return result;
    }

    QTextBoundaryFinder boundaryFinder(QTextBoundaryFinder::Word, text);

    qsizetype prevPos = 0;
    while (boundaryFinder.toNextBoundary() >= 0) {
        qsizetype pos = boundaryFinder.position();
        if (boundaryFinder.boundaryReasons() & QTextBoundaryFinder::EndOfItem) {
            QString word = text.sliced(prevPos, pos - prevPos);

            // Rule WB4 of UAX #29 says that format characters aren't a word
            // break boundary, but we still don't want to feed Hunspell with
            // BiDi control characters.
            word.removeIf(utils::isBidiControlChar);

            bool ok = checkWord(word);
            if (!ok) {
                result.append(std::make_pair<size_t, size_t>(prevPos, pos - prevPos));
            }
        }
        prevPos = pos;
    }

    return result;
}

bool HunspellSpellChecker::addToPersonalDictionary(const QString& word)
{
    d_personalDictionary.insert(word.normalized(QString::NormalizationForm_D));
    flushPersonalDictionary();
    return true;
}

void HunspellSpellChecker::flushPersonalDictionary()
{
    QDir dictDir = QFileInfo(d_personalDictionaryPath).dir();
    if (!dictDir.exists()) {
        dictDir.mkpath(".");
    }

    QSaveFile file(d_personalDictionaryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
            QApplication::activeWindow(),
            QCoreApplication::applicationName(),
            tr("Saving personal dictionary to %1 failed: %2").arg(d_personalDictionaryPath, file.errorString()));

        return;
    }

    QTextStream stream(&file);
    for (const QString& word : std::as_const(d_personalDictionary)) {
        stream << word << "\n";
    }
    file.commit();
}

void HunspellSpellChecker::loadPersonalDictionary()
{
    if (!QFileInfo::exists(d_personalDictionaryPath)) {
        return;
    }

    QFile file(d_personalDictionaryPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(
            QApplication::activeWindow(),
            QCoreApplication::applicationName(),
            tr("Loading personal dictionary from %1 failed: %2").arg(d_personalDictionaryPath, file.errorString()));

        return;
    }

    d_personalDictionary.clear();

    QString line;
    QTextStream stream(&file);
    while (stream.readLineInto(&line)) {
        if (line.isEmpty()) {
            continue;
        }
        d_personalDictionary.insert(line.normalized(QString::NormalizationForm_D));
    }
}

void HunspellSpellChecker::setPersonalDictionaryPath()
{
    QString loc = s_personalDictionaryLocation;
    if (loc.isEmpty()) {
        loc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    d_personalDictionaryPath = loc + QDir::separator() + "/personal.dic";

    loadPersonalDictionary();

    const QStringList watchedFiles = d_watcher->files();
    for (const QString& file : watchedFiles) {
        d_watcher->removePath(file);
    }
    d_watcher->addPath(d_personalDictionaryPath);
}

void HunspellSpellChecker::personalDictionaryFileChanged()
{
    qDebug() << "Personal dictionary file changed on disk";
    loadPersonalDictionary();

    if (!d_watcher->files().contains(d_personalDictionaryPath)) {
        d_watcher->addPath(d_personalDictionaryPath);
    }
}

void HunspellSpellChecker::requestSuggestionsImpl(const QString& word, int position)
{
    const QStringList dictionaries = currentDictionaryNames();
    if (dictionaries.isEmpty()) {
        qWarning() << "Asked for suggestions, but no dictionaries are active!";
        return;
    }

    QList<LoadedSpeller*> spellers;
    for (const auto& dictName : dictionaries) {
        spellers.append(d_spellers[dictName].get());
    }

    SpellingSuggestionsWorker* worker = new SpellingSuggestionsWorker(spellers, word, position);

    ensureWorkerThread();
    worker->moveToThread(d_workerThread);

    connect(worker, &SpellingSuggestionsWorker::suggestionsReady, this, &HunspellSpellChecker::suggestionsCalculated);
    QMetaObject::invokeMethod(worker, &SpellingSuggestionsWorker::process, Qt::QueuedConnection);
}

void HunspellSpellChecker::loaderWorkerDone(const QMap<QString, katvan::LoadedSpeller*>& spellers)
{
    QList<DictionaryDef> dicts;

    for (auto [dictName, speller] : spellers.asKeyValueRange()) {
        std::unique_ptr<LoadedSpeller> spellerPtr(speller);

        if (!d_spellers.contains(dictName)) {
            d_spellers.emplace(dictName, std::move(spellerPtr));
        }
        dicts.append(std::make_pair(dictName, QString()));
    }
    SpellChecker::setCurrentDictionaries(dicts);
}

void DictionaryLoaderWorker::process()
{
    QMap<QString, LoadedSpeller*> result;

    for (const auto& [dictName, dictAffFile] : std::as_const(d_dicts)) {
        QString dicFile = QFileInfo(dictAffFile).path() + "/" + dictName + ".dic";

        QByteArray affPath = dictAffFile.toLocal8Bit();
        QByteArray dicPath = dicFile.toLocal8Bit();

        result.insert(dictName, new LoadedSpeller(
            affPath.data(), dicPath.data(), getDictionaryScript(dictName)));
    }

    Q_EMIT dictionariesLoaded(result);

    deleteLater();
}

void SpellingSuggestionsWorker::process()
{
    QStringList result;
    {
        std::unique_lock<QMutex> locker { g_mutex };

        for (LoadedSpeller* speller : d_spellers) {
            if (!isAppropriateScriptForDictionary(speller->script, d_word)) {
                continue;
            }

            QByteArray encodedWord = speller->encoder.encode(d_word);
            std::vector<std::string> suggestions = speller->speller.suggest(encodedWord.toStdString());

            result.reserve(result.size() + suggestions.size());
            for (const auto& s : suggestions) {
                QString decoded = speller->decoder.decode(s.c_str());

                if (!result.contains(decoded)) {
                    result.append(decoded);
                }
            }
        }
    }

    Q_EMIT suggestionsReady(d_word, d_pos, result);

    deleteLater();
}

}

#include "moc_katvan_spellchecker_hunspell.cpp"
