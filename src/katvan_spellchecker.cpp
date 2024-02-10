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
#include "katvan_spellchecker.h"

#include <hunspell.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBoundaryFinder>

namespace katvan {

SpellChecker::SpellChecker(QObject* parent) : QObject(parent)
{
}

SpellChecker::~SpellChecker()
{
}

/**
 * Scan system and executable-local locations for Hunspell dictionaries,
 * which are a pair of *.aff and *.dic files with the same base name. 
 */
QMap<QString, QString> SpellChecker::findDictionaries()
{
    QStringList dictDirs;
    dictDirs.append(QCoreApplication::applicationDirPath() + "/hunspell");

    QStringList systemDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& dir : systemDirs) {
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

QString SpellChecker::dictionaryDisplayName(const QString& dictName)
{
    QLocale locale(dictName);
    if (locale.language() == QLocale::C) {
        return tr("Unknown");
    }

    return QStringLiteral("%1 (%2)").arg(
        QLocale::languageToString(locale.language()),
        QLocale::territoryToString(locale.territory())
    );
}

void SpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictAffFile)
{
    if (!dictName.isEmpty() && !d_spellers.contains(dictName)) {
        QString dicFile = QFileInfo(dictAffFile).path() + "/" + dictName + ".dic";

        d_spellers.emplace(dictName, std::make_unique<Hunspell>(
            dictAffFile.toLocal8Bit().data(),
            dicFile.toLocal8Bit().data()));
    }

    d_currentDictName = dictName;
}

QList<std::pair<size_t, size_t>> SpellChecker::checkSpelling(const QString& text)
{
    QList<std::pair<size_t, size_t>> result;
    if (d_currentDictName.isEmpty()) {
        return result;
    }

    Hunspell* speller = d_spellers[d_currentDictName].get();

    QTextBoundaryFinder boundryFinder(QTextBoundaryFinder::Word, text);

    qsizetype prevPos = 0;
    while (boundryFinder.toNextBoundary() >= 0) {
        qsizetype pos = boundryFinder.position();
        if (boundryFinder.boundaryReasons() & QTextBoundaryFinder::EndOfItem) {
            QString word = text.sliced(prevPos, pos - prevPos);
            bool ok = speller->spell(word.toStdString());
            if (!ok) {
                result.append(std::make_pair<size_t, size_t>(prevPos, pos - prevPos));
            }
        }
        prevPos = pos;
    }

    return result;
}

}
