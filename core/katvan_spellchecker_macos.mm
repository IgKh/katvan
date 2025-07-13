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
#include "katvan_spellchecker_macos.h"

#include <QDebug>

#import <AppKit/AppKit.h>

namespace katvan {

MacOsSpellChecker::MacOsSpellChecker(QObject* parent)
    : SpellChecker(parent)
{
}

MacOsSpellChecker::~MacOsSpellChecker()
{
}

QMap<QString, QString> MacOsSpellChecker::findDictionaries()
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSArray<NSString*>* languages = [checker availableLanguages];

    QMap<QString, QString> result;
    for (NSString* lang in languages) {
        result.insert(QString::fromNSString(lang), QString());
    }
    return result;
}

void MacOsSpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictPath)
{
    if (dictName.isEmpty()) {
        SpellChecker::setCurrentDictionary(dictName, dictPath);
        return;
    }

    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];

    BOOL ok = [checker setLanguage: dictName.toNSString()];
    if (!ok) {
        qWarning() << "Failed to set macOS spell checking dictionary to" << dictName;
        return;
    }
    SpellChecker::setCurrentDictionary(dictName, dictPath);
}

SpellChecker::MisspelledWordRanges MacOsSpellChecker::checkSpelling(const QString& text)
{
    SpellChecker::MisspelledWordRanges result;
    if (currentDictionaryName().isEmpty()) {
        return result;
    }

    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSString* str = text.toNSString();

    NSUInteger start = 0;
    while (start < [str length]) {
        NSRange range = [checker checkSpellingOfString: str
                                 startingAt: start
                                 language: [checker language]
                                 wrap: NO
                                 inSpellDocumentWithTag: 0
                                 wordCount: NULL];

        if (range.length == 0) {
            break;
        }

        result.append(std::make_pair(range.location, range.length));
        start = NSMaxRange(range);
    }
    return result;
}

void MacOsSpellChecker::addToPersonalDictionary(const QString& word)
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSString* str = word.toNSString();

    [checker learnWord: str];
}

void MacOsSpellChecker::requestSuggestionsImpl(const QString& word, int position)
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSString* str = word.toNSString();

    NSArray<NSString*>* guesses = [checker guessesForWordRange: NSMakeRange(0, [str length])
                                           inString: str
                                           language: [checker language]
                                           inSpellDocumentWithTag: 0];

    QList<QString> suggestions;
    for (NSString* guess in guesses) {
        suggestions.append(QString::fromNSString(guess));
    }

    suggestionsCalculated(word, position, suggestions);
}

}

#include "moc_katvan_spellchecker_macos.cpp"
