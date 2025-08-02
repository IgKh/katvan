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
#include <QTextBoundaryFinder>
#include <QTextStream>
#include <QThread>

namespace katvan {

QString HunspellSpellChecker::s_personalDictionaryLocation;

struct LoadedSpeller
{
    LoadedSpeller(const char* affPath, const char* dicPath)
        : speller(affPath, dicPath) {}

    Hunspell speller;
    QMutex mutex;
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

void HunspellSpellChecker::setPersonalDictionaryLocation(const QString& dirPath)
{
    s_personalDictionaryLocation = dirPath;
    setPersonalDictionaryPath();
}

void HunspellSpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictAffFile)
{
    if (dictName.isEmpty()) {
        SpellChecker::setCurrentDictionary(dictName, dictAffFile);
        return;
    }

    DictionaryLoaderWorker* worker = new DictionaryLoaderWorker(dictName, dictAffFile);

    ensureWorkerThread();
    worker->moveToThread(d_workerThread);

    connect(worker, &DictionaryLoaderWorker::dictionaryLoaded, this, &HunspellSpellChecker::loaderWorkerDone);
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

bool HunspellSpellChecker::checkWord(Hunspell& speller, QChar::Script dictionaryScript, const QString& word)
{
    QString normalizedWord = word.normalized(QString::NormalizationForm_D);
    if (d_personalDictionary.contains(normalizedWord)) {
        return true;
    }

    // Hunspell seems to emit a lot of false positives, so reduce them with
    // some heuristics:
    // - Ignore single Unicode grapheme words (single character, emoji, etc)
    // - Ignore Hebrew ordinals (single Hebrew letter followed by a Geresh)
    // - Ignore words written in script that doesn't fit the dictionary's locale.
    if (isSingleGrapheme(word)
        || isHebrewOrdinal(normalizedWord)
        || !isAppropriateScriptForDictionary(dictionaryScript, word)) {
        return true;
    }

    return speller.spell(word.toStdString());
}

SpellChecker::MisspelledWordRanges HunspellSpellChecker::checkSpelling(const QString& text)
{
    MisspelledWordRanges result;
    if (currentDictionaryName().isEmpty()) {
        return result;
    }

    LoadedSpeller* speller = d_spellers[currentDictionaryName()].get();
    if (!speller->mutex.tryLock()) {
        // Do not block the UI event loop! If we can't take the speller
        // lock (because suggestions are being generated at the moment),
        // just pretend there are no spelling mistakes here.
        return result;
    }

    QChar::Script dictScript = getDictionaryScript(currentDictionaryName());

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

            bool ok = checkWord(speller->speller, dictScript, word);
            if (!ok) {
                result.append(std::make_pair<size_t, size_t>(prevPos, pos - prevPos));
            }
        }
        prevPos = pos;
    }

    speller->mutex.unlock();
    return result;
}

void HunspellSpellChecker::addToPersonalDictionary(const QString& word)
{
    d_personalDictionary.insert(word.normalized(QString::NormalizationForm_D));
    flushPersonalDictionary();
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
    LoadedSpeller* speller = d_spellers[currentDictionaryName()].get();
    SpellingSuggestionsWorker* worker = new SpellingSuggestionsWorker(speller, word, position);

    ensureWorkerThread();
    worker->moveToThread(d_workerThread);

    connect(worker, &SpellingSuggestionsWorker::suggestionsReady, this, &HunspellSpellChecker::suggestionsCalculated);
    QMetaObject::invokeMethod(worker, &SpellingSuggestionsWorker::process, Qt::QueuedConnection);
}

void HunspellSpellChecker::loaderWorkerDone(QString dictName, katvan::LoadedSpeller* speller)
{
    std::unique_ptr<LoadedSpeller> spellerPtr(speller);

    if (!d_spellers.contains(dictName)) {
        d_spellers.emplace(dictName, std::move(spellerPtr));
    }

    SpellChecker::setCurrentDictionary(dictName, QString());
}

void DictionaryLoaderWorker::process()
{
    QString dicFile = QFileInfo(d_dictAffFile).path() + "/" + d_dictName + ".dic";

    QByteArray affPath = d_dictAffFile.toLocal8Bit();
    QByteArray dicPath = dicFile.toLocal8Bit();

    Q_EMIT dictionaryLoaded(d_dictName, new LoadedSpeller(affPath.data(), dicPath.data()));

    deleteLater();
}

void SpellingSuggestionsWorker::process()
{
    std::vector<std::string> suggestions;
    {
        QMutexLocker locker{ &d_speller->mutex };
        suggestions = d_speller->speller.suggest(d_word.toStdString());
    }

    QStringList result;
    result.reserve(suggestions.size());
    for (const auto& s : suggestions) {
        result.append(QString::fromStdString(s));
    }

    Q_EMIT suggestionsReady(d_word, d_pos, result);

    deleteLater();
}

}

#include "moc_katvan_spellchecker_hunspell.cpp"
