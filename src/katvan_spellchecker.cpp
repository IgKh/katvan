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
#include "katvan_spellchecker_hunspell.h"
#include "katvan_spellchecker.h"

#include <QLocale>

namespace katvan {

static constexpr size_t SUGGESTIONS_CACHE_SIZE = 25;

SpellChecker::SpellChecker(QObject* parent)
    : QObject(parent)
    , d_suggestionsCache(SUGGESTIONS_CACHE_SIZE)
{
}

SpellChecker::~SpellChecker()
{
}

SpellChecker* SpellChecker::instance()
{
    static SpellChecker* checker = nullptr;
    if (!checker) {
        checker = new HunspellSpellChecker();
    }
    return checker;
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

void SpellChecker::setPersonalDictionaryLocation(const QString& dirPath)
{
    Q_UNUSED(dirPath)
}

void SpellChecker::setCurrentDictionary(const QString& dictName, const QString& dictPath)
{
    Q_UNUSED(dictPath)
    d_currentDictName = dictName;

    d_suggestionsCache.clear();
    Q_EMIT dictionaryChanged(dictName);
}

void SpellChecker::requestSuggestions(const QString& word, int position)
{
    if (d_currentDictName.isEmpty()) {
        qWarning() << "Asked for suggestions, but no dictionary is active!";
        return;
    }

    if (d_suggestionsCache.contains(word)) {
        Q_EMIT suggestionsReady(word, position, *d_suggestionsCache.object(word));
        return;
    }

    requestSuggestionsImpl(word, position);
}

void SpellChecker::suggestionsCalculated(const QString& word, int position, const QStringList& suggestions)
{
    d_suggestionsCache.insert(word, new QStringList(suggestions));
    Q_EMIT suggestionsReady(word, position, suggestions);
}

}
