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
#import "macshell_spellchecker.h"

#import <AppKit/AppKit.h>

KatvanMacSpellChecker::KatvanMacSpellChecker(QObject* parent)
    : katvan::SpellChecker(parent)
    , d_documentTag(0)
{
    d_documentTag = [NSSpellChecker uniqueSpellDocumentTag];

    auto block = ^(NSNotification* notification) {
        handleSpellerNotification(notification);
    };

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    d_notificationToken = [center addObserverForName:nil
                                  object:[NSSpellChecker sharedSpellChecker]
                                  queue:[NSOperationQueue mainQueue]
                                  usingBlock:block];
}

KatvanMacSpellChecker::~KatvanMacSpellChecker()
{
    [[NSNotificationCenter defaultCenter] removeObserver:d_notificationToken name:nil object:nil];
    [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:d_documentTag];
}

QMap<QString, QString> KatvanMacSpellChecker::findDictionaries()
{
    return QMap<QString, QString>();
}

void KatvanMacSpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictPath)
{
    Q_UNUSED(dictName)
    Q_UNUSED(dictPath)
}

katvan::SpellChecker::MisspelledWordRanges KatvanMacSpellChecker::checkSpelling(const QString& text)
{
    SpellChecker::MisspelledWordRanges ranges;

    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSString* str = text.toNSString();

    NSArray<NSTextCheckingResult*>* results = [checker checkString:str
                                                       range:NSMakeRange(0, [str length])
                                                       types:NSTextCheckingTypeSpelling
                                                       options:nil
                                                       inSpellDocumentWithTag:d_documentTag
                                                       orthography:nil
                                                       wordCount:NULL];

    for (NSTextCheckingResult* result in results) {
        ranges.append(std::make_pair(result.range.location, result.range.length));
    }
    return ranges;
}

bool KatvanMacSpellChecker::addToPersonalDictionary(const QString& word)
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    [checker learnWord:word.toNSString()];

    // Rehighlighting will happen through handleSpellerNotification
    return false;
}

void KatvanMacSpellChecker::ignoreWord(NSString* word)
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    [checker ignoreWord:word inSpellDocumentWithTag:d_documentTag];
}

void KatvanMacSpellChecker::requestSuggestionsImpl(const QString& word, int position)
{
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSString* str = word.toNSString();

    // Guess the word's language
    NSRange wordRange = NSMakeRange(0, [str length]);
    NSString* language = [checker languageForWordRange:wordRange inString:str orthography:nil];

    NSArray<NSString*>* guesses = [checker guessesForWordRange:wordRange
                                           inString:str
                                           language:language
                                           inSpellDocumentWithTag:d_documentTag];

    QList<QString> suggestions;
    for (NSString* guess in guesses) {
        suggestions.append(QString::fromNSString(guess));
    }

    suggestionsCalculated(word, position, suggestions);
}

void KatvanMacSpellChecker::handleSpellerNotification(NSNotification* notification)
{
    // The following notification names are not documentad, but were observed
    // to be emitted at least on macOS Sequoia. We use them to invalidate the
    // spell checking results when the user does things via the system provided
    // Spelling panel.
    if ([notification.name isEqualToString:@"NSSpellCheckerDidChangeLanguageNotification"] ||
        [notification.name isEqualToString:@"NSSpellCheckerDidLearnWordNotification"]) {
        Q_EMIT dictionaryChanged(QString());
    }
}

#include "moc_macshell_spellchecker.cpp"
