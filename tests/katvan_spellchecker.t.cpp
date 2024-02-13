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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QTemporaryDir>

using namespace katvan;

void PrintTo(const QString& str, std::ostream* os) {
    *os << str.toStdString();
}

TEST(SpellCheckerTests, DetectDictionaries) {
    QMap<QString, QString> result = SpellChecker::findDictionaries();
    EXPECT_THAT(result.keys(), ::testing::IsSupersetOf({
        QStringLiteral("en_IL"),
        QStringLiteral("he_XX"),
    }));
}

TEST(SpellCheckerTests, BasicEnglish) {
    SpellChecker checker;
    checker.setCurrentDictionary("en_IL", "hunspell/en_IL.aff");

    auto result1 = checker.checkSpelling("A good bad 12 word עברית");
    EXPECT_THAT(result1, ::testing::ElementsAre(
        std::make_pair( 0, 1), // A
        std::make_pair( 7, 3), // bad
        std::make_pair(19, 5)  // עברית
    ));

    auto result2 = checker.checkSpelling("Good WORD foo");
    EXPECT_THAT(result2, ::testing::ElementsAre(
        std::make_pair(10, 3)  // foo
    ));
}

TEST(SpellCheckerTests, BasicHebrew) {
    SpellChecker checker;
    checker.setCurrentDictionary("he_XX", "hunspell/he_XX.aff");

    auto result = checker.checkSpelling("מילה בעברית טובה שהיא חלק ת'רד נתב\"ג 3 ד ה' ת\"א English");
    EXPECT_THAT(result, ::testing::ElementsAre(
        std::make_pair( 5, 6), // בעברית
        std::make_pair(17, 4), // שהיא
        std::make_pair(22, 3), // חלק
        std::make_pair(39, 1), // ד
        std::make_pair(41, 2), // ה'
        std::make_pair(44, 3), // ת"א
        std::make_pair(48, 7)  // English
    ));
}

TEST(SpellCheckerTests, PersonalDict) {
    QTemporaryDir dir;
    SpellChecker::setPersonalDictionaryLocation(dir.path());

    SpellChecker checker1;
    checker1.setCurrentDictionary("en_IL", "hunspell/en_IL.aff");

    auto result1 = checker1.checkSpelling("good bar bad");
    EXPECT_THAT(result1, ::testing::ElementsAre(
        std::make_pair(5, 3), // bar
        std::make_pair(9, 3)  // bad
    ));

    checker1.addToPersonalDictionary("bad");

    auto result2 = checker1.checkSpelling("good bar bad");
    EXPECT_THAT(result2, ::testing::ElementsAre(
        std::make_pair(5, 3) // bar
    ));

    SpellChecker checker2;
    checker2.setCurrentDictionary("en_IL", "hunspell/en_IL.aff");

    auto result3 = checker2.checkSpelling("good bar bad");
    EXPECT_THAT(result3, ::testing::ElementsAre(
        std::make_pair(5, 3) // bar
    ));
}
