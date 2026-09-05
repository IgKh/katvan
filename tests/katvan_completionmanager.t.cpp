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

#include "katvan_completionmanager.h"
#include "katvan_editor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace katvan;

TEST(CompletionManagerTests, MultilineCompletionWithSpacesIndent) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    settings.setIndentWidth(4);

    EditorHolder holder(QString(), settings);
    CompletionManager* manager = holder.editor->completionManager();

    manager->completionsReady(0, 0, R"([{"apply":"while ${1 < 2} {\n\t${}\n}","detail":"","kind":"syntax","label":"while loop"}])");
    manager->activateSuggestion(0);

    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("while 1 < 2 {\n    \n}")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(6, 11)));
}

TEST(CompletionManagerTests, MultilineCompletionWithTabsIndent) {
    EditorSettings settings;
    settings.setIndentStyle(EditorSettings::IndentStyle::TABS);

    EditorHolder holder(QString(), settings);
    CompletionManager* manager = holder.editor->completionManager();

    manager->completionsReady(0, 0, R"([{"apply":"while ${1 < 2} {\n\t${}\n}","detail":"","kind":"syntax","label":"while loop"}])");
    manager->activateSuggestion(0);

    EXPECT_THAT(holder.text(), ::testing::Eq(QStringLiteral("while 1 < 2 {\n\t\n}")));
    EXPECT_THAT(holder.selectionRange(), ::testing::Eq(std::make_tuple(6, 11)));
}
