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

struct ISpellCheckerFactory;
struct ISpellChecker;

namespace katvan {

class WindowsSpellChecker : public SpellChecker
{
    Q_OBJECT

public:
    WindowsSpellChecker(QObject* parent = nullptr);
    ~WindowsSpellChecker();

    QMap<QString, QString> findDictionaries() override;

    void setCurrentDictionary(const QString& dictName, const QString& dictPath) override;

    MisspelledWordRanges checkSpelling(const QString& text) override;

    void addToPersonalDictionary(const QString& word) override;

private:
    void requestSuggestionsImpl(const QString& word, int position) override;

    ISpellCheckerFactory* d_factory;
    ISpellChecker* d_checker;
};

}
