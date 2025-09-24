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
#include "katvan_testutils.h"

#include "katvan_editorsettings.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace katvan;

TEST(EditorSettingsTests, Empty) {
    EditorSettings s;
    EXPECT_THAT(s.hasFontFamily(), ::testing::IsFalse());
    EXPECT_THAT(s.hasFontSize(), ::testing::IsFalse());
    EXPECT_THAT(s.hasLineNumberStyle(), ::testing::IsFalse());
    EXPECT_THAT(s.hasIndentMode(), ::testing::IsFalse());
    EXPECT_THAT(s.hasIndentStyle(), ::testing::IsFalse());
    EXPECT_THAT(s.hasIndentWidth(), ::testing::IsFalse());
    EXPECT_THAT(s.hasTabWidth(), ::testing::IsFalse());

    EXPECT_THAT(s.toModeLine(), ::testing::Eq(QString()));
}

TEST(EditorSettingsTests, FontFamily) {
    EditorSettings s1 { "font Verdana" };
    EXPECT_THAT(s1.hasFontFamily(), ::testing::IsTrue());
    EXPECT_THAT(s1.fontFamily(), ::testing::Eq(QStringLiteral("Verdana")));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("font Verdana;")));

    EditorSettings s2 { "font Super 123 that; other" };
    EXPECT_THAT(s2.hasFontFamily(), ::testing::IsTrue());
    EXPECT_THAT(s2.fontFamily(), ::testing::Eq(QStringLiteral("Super 123 that")));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("font Super 123 that;")));

    EditorSettings s5 { "font" };
    EXPECT_THAT(s5.hasFontFamily(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, FontSize) {
    EditorSettings s1 { "font-size 10" };
    EXPECT_THAT(s1.hasFontSize(), ::testing::IsTrue());
    EXPECT_THAT(s1.fontSize(), ::testing::Eq(10));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("font-size 10;")));

    EditorSettings s2 { "font-size 2a" };
    EXPECT_THAT(s2.hasFontSize(), ::testing::IsFalse());

    EditorSettings s3 { "font-size -5" };
    EXPECT_THAT(s3.hasFontSize(), ::testing::IsFalse());

    EditorSettings s4 { "font-size 0" };
    EXPECT_THAT(s4.hasFontSize(), ::testing::IsFalse());

    EditorSettings s5 { "font-size" };
    EXPECT_THAT(s5.hasFontSize(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, ColorScheme) {
    EditorSettings s1 { "scheme dark" };
    EXPECT_THAT(s1.hasColorScheme(), ::testing::IsTrue());
    EXPECT_THAT(s1.colorScheme(), ::testing::Eq(QStringLiteral("dark")));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("scheme dark;")));

    EditorSettings s2 { "scheme" };
    EXPECT_THAT(s2.hasColorScheme(), ::testing::IsFalse());
    EXPECT_THAT(s2.colorScheme(), ::testing::Eq(QStringLiteral("auto")));
}

TEST(EditorSettingsTests, LineNumberStyle) {
    EditorSettings s1 { "show-line-numbers both" };
    EXPECT_THAT(s1.hasLineNumberStyle(), ::testing::IsTrue());
    EXPECT_THAT(s1.lineNumberStyle(), ::testing::Eq(EditorSettings::LineNumberStyle::BOTH_SIDES));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("show-line-numbers both;")));

    EditorSettings s2 { "show-line-numbers primary" };
    EXPECT_THAT(s2.hasLineNumberStyle(), ::testing::IsTrue());
    EXPECT_THAT(s2.lineNumberStyle(), ::testing::Eq(EditorSettings::LineNumberStyle::PRIMARY_ONLY));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("show-line-numbers primary;")));

    EditorSettings s3 { "show-line-numbers none" };
    EXPECT_THAT(s3.hasLineNumberStyle(), ::testing::IsTrue());
    EXPECT_THAT(s3.lineNumberStyle(), ::testing::Eq(EditorSettings::LineNumberStyle::NONE));
    EXPECT_THAT(s3.toModeLine(), ::testing::Eq(QStringLiteral("show-line-numbers none;")));

    EditorSettings s4 { "show-line-numbers NoNe" };
    EXPECT_THAT(s4.hasLineNumberStyle(), ::testing::IsFalse());

    EditorSettings s5 { "show-line-numbers 10" };
    EXPECT_THAT(s5.hasLineNumberStyle(), ::testing::IsFalse());

    EditorSettings s6 { "show-line-numbers" };
    EXPECT_THAT(s6.hasLineNumberStyle(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, ShowControlChars) {
    EditorSettings s1 { "show-control-chars ON" };
    EXPECT_THAT(s1.hasShowControlChars(), ::testing::IsTrue());
    EXPECT_THAT(s1.showControlChars(), ::testing::IsTrue());
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("show-control-chars on;")));

    EditorSettings s2 { "show-control-chars 1" };
    EXPECT_THAT(s2.hasShowControlChars(), ::testing::IsTrue());
    EXPECT_THAT(s2.showControlChars(), ::testing::IsTrue());
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("show-control-chars on;")));

    EditorSettings s3 { "show-control-chars false" };
    EXPECT_THAT(s3.hasShowControlChars(), ::testing::IsTrue());
    EXPECT_THAT(s3.showControlChars(), ::testing::IsFalse());
    EXPECT_THAT(s3.toModeLine(), ::testing::Eq(QStringLiteral("show-control-chars off;")));

    EditorSettings s4 { "show-control-chars foo" };
    EXPECT_THAT(s4.hasShowControlChars(), ::testing::IsFalse());

    EditorSettings s5 { "show-control-chars" };
    EXPECT_THAT(s5.hasShowControlChars(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, IndentMode) {
    EditorSettings s1 { "indent-mode none" };
    EXPECT_THAT(s1.hasIndentMode(), ::testing::IsTrue());
    EXPECT_THAT(s1.indentMode(), ::testing::Eq(EditorSettings::IndentMode::NONE));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("indent-mode none;")));

    EditorSettings s2 { "indent-mode normal" };
    EXPECT_THAT(s2.hasIndentMode(), ::testing::IsTrue());
    EXPECT_THAT(s2.indentMode(), ::testing::Eq(EditorSettings::IndentMode::NORMAL));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("indent-mode normal;")));

    EditorSettings s3 { "indent-mode typst" };
    EXPECT_THAT(s3.hasIndentMode(), ::testing::IsTrue());
    EXPECT_THAT(s3.indentMode(), ::testing::Eq(EditorSettings::IndentMode::SMART));
    EXPECT_THAT(s3.toModeLine(), ::testing::Eq(QStringLiteral("indent-mode typst;")));

    EditorSettings s4 { "indent-mode cstyle" };
    EXPECT_THAT(s4.hasIndentMode(), ::testing::IsFalse());

    EditorSettings s5 { "indent-mode" };
    EXPECT_THAT(s5.hasLineNumberStyle(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, IndentStyle) {
    EditorSettings s1 { "replace-tabs ON" };
    EXPECT_THAT(s1.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s1.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::SPACES));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs on;")));

    EditorSettings s2 { "replace-tabs oN" };
    EXPECT_THAT(s2.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s2.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::SPACES));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs on;")));

    EditorSettings s3 { "replace-tabs 1" };
    EXPECT_THAT(s3.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s3.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::SPACES));
    EXPECT_THAT(s3.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs on;")));

    EditorSettings s4 { "replace-tabs TrUe" }; // spellchecker:disable-line
    EXPECT_THAT(s4.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s4.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::SPACES));
    EXPECT_THAT(s4.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs on;")));

    EditorSettings s5 { "replace-tabs false" };
    EXPECT_THAT(s5.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s5.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::TABS));
    EXPECT_THAT(s5.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs off;")));

    EditorSettings s6 { "replace-tabs off" };
    EXPECT_THAT(s6.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s6.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::TABS));
    EXPECT_THAT(s6.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs off;")));

    EditorSettings s7 { "replace-tabs 0" };
    EXPECT_THAT(s7.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s7.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::TABS));
    EXPECT_THAT(s7.toModeLine(), ::testing::Eq(QStringLiteral("replace-tabs off;")));

    EditorSettings s8 { "replace-tabs 3" };
    EXPECT_THAT(s8.hasIndentStyle(), ::testing::IsFalse());

    EditorSettings s9 { "replace-tabs foo" };
    EXPECT_THAT(s9.hasIndentStyle(), ::testing::IsFalse());

    EditorSettings s10 { "replace-tabs" };
    EXPECT_THAT(s10.hasIndentStyle(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, IndentWidth) {
    EditorSettings s1 { "indent-width 2" };
    EXPECT_THAT(s1.hasIndentWidth(), ::testing::IsTrue());
    EXPECT_THAT(s1.indentWidth(), ::testing::Eq(2));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("indent-width 2;")));

    EditorSettings s2 { "indent-width 0" };
    EXPECT_THAT(s2.hasIndentWidth(), ::testing::IsTrue());
    EXPECT_THAT(s2.indentWidth(), ::testing::Eq(0));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("indent-width 0;")));

    EditorSettings s3 { "indent-width baz" };
    EXPECT_THAT(s3.hasIndentWidth(), ::testing::IsFalse());

    EditorSettings s4 { "indent-width -5" };
    EXPECT_THAT(s4.hasIndentWidth(), ::testing::IsFalse());

    EditorSettings s5 { "indent-width" };
    EXPECT_THAT(s5.hasIndentWidth(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, TabWidth) {
    EditorSettings s1 { "tab-width 8" };
    EXPECT_THAT(s1.hasTabWidth(), ::testing::IsTrue());
    EXPECT_THAT(s1.tabWidth(), ::testing::Eq(8));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("tab-width 8;")));

    EditorSettings s2 { "tab-width 0" };
    EXPECT_THAT(s2.hasTabWidth(), ::testing::IsTrue());
    EXPECT_THAT(s2.tabWidth(), ::testing::Eq(0));
    EXPECT_THAT(s2.toModeLine(), ::testing::Eq(QStringLiteral("tab-width 0;")));

    EditorSettings s3 { "tab-width bar" };
    EXPECT_THAT(s3.hasTabWidth(), ::testing::IsFalse());

    EditorSettings s4 { "tab-width -10" };
    EXPECT_THAT(s4.hasTabWidth(), ::testing::IsFalse());

    EditorSettings s5 { "tab-width" };
    EXPECT_THAT(s5.hasTabWidth(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, AutoBrackets) {
    EditorSettings s1 { "auto-brackets ON" };
    EXPECT_THAT(s1.hasAutoBrackets(), ::testing::IsTrue());
    EXPECT_THAT(s1.autoBrackets(), ::testing::IsTrue());
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("auto-brackets on;")));

    EditorSettings s2 { "replace-tabs" };
    EXPECT_THAT(s2.hasAutoBrackets(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, AutoTriggerCompletions) {
    EditorSettings s1 { "auto-trigger-completions 1" };
    EXPECT_THAT(s1.hasAutoTriggerCompletions(), ::testing::IsTrue());
    EXPECT_THAT(s1.autoTriggerCompletions(), ::testing::IsTrue());
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("auto-trigger-completions on;")));

    EditorSettings s2 { "auto-brackets" };
    EXPECT_THAT(s2.hasAutoTriggerCompletions(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, AutoBackupInterval) {
    EditorSettings s1 { "backup-interval 10", EditorSettings::ModeSource::SETTINGS };
    EXPECT_THAT(s1.hasAutoBackupInterval(), ::testing::IsTrue());
    EXPECT_THAT(s1.autoBackupInterval(), ::testing::Eq(10));
    EXPECT_THAT(s1.toModeLine(), ::testing::Eq(QStringLiteral("backup-interval 10;")));

    EditorSettings s2 { "backup-interval 10", EditorSettings::ModeSource::DOCUMENT };
    EXPECT_THAT(s2.hasAutoBackupInterval(), ::testing::IsFalse());
}

TEST(EditorSettingsTests, Mixed) {
    EditorSettings s { "katvan: font Arial Special; no-such-flag; replace-tabs on; replace-tabs off; tab-width     5; font-size 10; indent-mode normal;" };
    EXPECT_THAT(s.hasFontFamily(), ::testing::IsTrue());
    EXPECT_THAT(s.hasFontSize(), ::testing::IsTrue());
    EXPECT_THAT(s.hasLineNumberStyle(), ::testing::IsFalse());
    EXPECT_THAT(s.hasIndentMode(), ::testing::IsTrue());
    EXPECT_THAT(s.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(s.hasIndentWidth(), ::testing::IsFalse());
    EXPECT_THAT(s.hasTabWidth(), ::testing::IsTrue());

    EXPECT_THAT(s.fontFamily(), ::testing::Eq(QStringLiteral("Arial Special")));
    EXPECT_THAT(s.fontSize(), ::testing::Eq(10));
    EXPECT_THAT(s.indentMode(), ::testing::Eq(EditorSettings::IndentMode::NORMAL));
    EXPECT_THAT(s.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::TABS));
    EXPECT_THAT(s.tabWidth(), ::testing::Eq(5));

    EXPECT_THAT(s.toModeLine(), ::testing::Eq(QStringLiteral("font Arial Special; font-size 10; indent-mode normal; replace-tabs off; tab-width 5;")));
}

TEST(EditorSettingsTests, Overrides) {
    EditorSettings s1 { "katvan: font Arial; replace-tabs on" };
    EditorSettings s2 { "kate: indent-width 8; font Verdana" };
    EditorSettings s3 { "foobar: font-size 4" };

    EditorSettings result;
    result.mergeSettings(s1);
    result.mergeSettings(s2);
    result.mergeSettings(s3);

    EXPECT_THAT(result.hasFontFamily(), ::testing::IsTrue());
    EXPECT_THAT(result.hasFontSize(), ::testing::IsTrue());
    EXPECT_THAT(result.hasLineNumberStyle(), ::testing::IsFalse());
    EXPECT_THAT(result.hasIndentMode(), ::testing::IsFalse());
    EXPECT_THAT(result.hasIndentStyle(), ::testing::IsTrue());
    EXPECT_THAT(result.hasIndentWidth(), ::testing::IsTrue());
    EXPECT_THAT(result.hasTabWidth(), ::testing::IsFalse());

    EXPECT_THAT(result.fontFamily(), ::testing::Eq(QStringLiteral("Verdana")));
    EXPECT_THAT(result.fontSize(), ::testing::Eq(4));
    EXPECT_THAT(result.indentStyle(), ::testing::Eq(EditorSettings::IndentStyle::SPACES));
    EXPECT_THAT(result.indentWidth(), ::testing::Eq(8));
}
