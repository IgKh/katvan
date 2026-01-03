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
#include "katvan_testutils.h"

#include "katvan_spellchecker_hunspell.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace katvan;

static constexpr int SIGNAL_WAIT_TIMEOUT_MSEC = 500;

static QString getDictionaryPath(const char* name)
{
    return QCoreApplication::applicationDirPath() + "/hunspell/" + QLatin1String(name) + ".aff";
}

TEST(SpellCheckerTests, DetectDictionaries) {
    HunspellSpellChecker checker;
    QMap<QString, QString> result = checker.findDictionaries();
    EXPECT_THAT(result.keys(), ::testing::IsSupersetOf({
        QStringLiteral("en_IL"),
        QStringLiteral("he_XX"),
    }));

    EXPECT_THAT(result.value("en_IL"), ::testing::Eq(getDictionaryPath("en_IL")));
    EXPECT_THAT(result.value("he_XX"), ::testing::Eq(getDictionaryPath("he_XX")));
}

TEST(SpellCheckerTests, BasicEnglish) {
    HunspellSpellChecker checker;
    QSignalSpy spy(&checker, &SpellChecker::dictionaryChanged);

    checker.setCurrentDictionary("en_IL", getDictionaryPath("en_IL"));
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIMEOUT_MSEC));

    auto result1 = checker.checkSpelling("A good bad 12 word עברית z");
    EXPECT_THAT(result1, ::testing::ElementsAre(
        std::make_pair( 7, 3)  // bad
    ));

    auto result2 = checker.checkSpelling("Good WORD foo");
    EXPECT_THAT(result2, ::testing::ElementsAre(
        std::make_pair(10, 3)  // foo
    ));
}

TEST(SpellCheckerTests, BasicHebrew) {
    HunspellSpellChecker checker;
    QSignalSpy spy(&checker, &SpellChecker::dictionaryChanged);

    checker.setCurrentDictionary("he_XX", getDictionaryPath("he_XX"));
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIMEOUT_MSEC));

    auto result = checker.checkSpelling("מילה בעברית טובה שהיא חלק ת'רד נתב\"ג 3 ד ה' ת\"א English");
    EXPECT_THAT(result, ::testing::ElementsAre(
        std::make_pair( 5, 6), // בעברית
        std::make_pair(17, 4), // שהיא
        std::make_pair(22, 3), // חלק
        std::make_pair(44, 3)  // ת"א
    ));
}

TEST(SpellCheckerTests, PersonalDict) {
    QTemporaryDir dir;

    HunspellSpellChecker checker1;
    QSignalSpy spy1(&checker1, &SpellChecker::dictionaryChanged);
    checker1.setPersonalDictionaryLocation(dir.path());

    checker1.setCurrentDictionary("en_IL", getDictionaryPath("en_IL"));
    EXPECT_TRUE(spy1.wait(SIGNAL_WAIT_TIMEOUT_MSEC));

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

    HunspellSpellChecker checker2;
    QSignalSpy spy2(&checker2, &SpellChecker::dictionaryChanged);

    checker2.setCurrentDictionary("en_IL", getDictionaryPath("en_IL"));
    EXPECT_TRUE(spy2.wait(SIGNAL_WAIT_TIMEOUT_MSEC));

    auto result3 = checker2.checkSpelling("good bar bad");
    EXPECT_THAT(result3, ::testing::ElementsAre(
        std::make_pair(5, 3) // bar
    ));
}
