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

#include <QFont>
#include <QString>

#include <optional>

namespace katvan {

class EditorSettings
{
    Q_GADGET

public:
    enum class ModeSource {
        SETTINGS,
        DOCUMENT,
    };
    Q_ENUM(ModeSource);

    enum class LineNumberStyle {
        BOTH_SIDES,
        PRIMARY_ONLY,
        NONE
    };
    Q_ENUM(LineNumberStyle);

    enum class IndentMode {
        NONE,
        NORMAL,
        SMART
    };
    Q_ENUM(IndentMode);

    enum class IndentStyle {
        SPACES,
        TABS
    };
    Q_ENUM(IndentStyle);

    EditorSettings() {}
    explicit EditorSettings(const QString& mode, ModeSource source = ModeSource::DOCUMENT) {
        parseModeLine(mode, source);
    }

    bool operator==(const EditorSettings&) const = default;

    QString toModeLine() const;

    QString fontFamily() const;
    int fontSize() const;
    QFont font() const;

    IndentMode indentMode() const;
    IndentStyle indentStyle() const;
    int indentWidth() const;
    int tabWidth() const;

    QString colorScheme() const;
    LineNumberStyle lineNumberStyle() const;
    bool showControlChars() const;

    bool autoBrackets() const;
    bool autoTriggerCompletions() const;

    int autoBackupInterval() const;

    void setFontFamily(const QString& fontFamily) { d_fontFamily = fontFamily; }
    void setFontSize(int fontSize) { d_fontSize = fontSize; }
    void setIndentMode(IndentMode indentMode) { d_indentMode = indentMode; }
    void setIndentStyle(IndentStyle indentStyle) { d_indentStyle = indentStyle; }
    void setIndentWidth(int indentWidth) { d_indentWidth = indentWidth; }
    void setTabWidth(int tabWidth) { d_tabWidth = tabWidth; }
    void setColorScheme(const QString& colorScheme) { d_colorScheme = colorScheme; }
    void setLineNumberStyle(LineNumberStyle style) { d_lineNumberStyle = style; }
    void setShowControlChars(bool show) { d_showControlChars = show; }
    void setAutoBrackets(bool enable) { d_autoBrackets = enable; }
    void setAutoTriggerCompletions(bool enable) { d_autoTriggerCompletions = enable; }
    void setAutoBackupInterval(int interval) { d_autoBackupInterval = interval; }

    bool hasFontFamily() const { return d_fontFamily.has_value(); }
    bool hasFontSize() const { return d_fontSize.has_value(); }
    bool hasIndentMode() const { return d_indentMode.has_value(); }
    bool hasIndentStyle() const { return d_indentStyle.has_value(); }
    bool hasIndentWidth() const { return d_indentWidth.has_value(); }
    bool hasTabWidth() const { return d_tabWidth.has_value(); }
    bool hasColorScheme() const { return d_colorScheme.has_value(); }
    bool hasLineNumberStyle() const { return d_lineNumberStyle.has_value(); }
    bool hasShowControlChars() const { return d_showControlChars.has_value(); }
    bool hasAutoBrackets() const { return d_autoBrackets.has_value(); }
    bool hasAutoTriggerCompletions() const { return d_autoTriggerCompletions.has_value(); }
    bool hasAutoBackupInterval() const { return d_autoBackupInterval.has_value(); }

    void mergeSettings(const EditorSettings& other);

private:
    void parseModeLine(QString mode, ModeSource source);

    std::optional<QString> d_fontFamily;
    std::optional<int> d_fontSize;
    std::optional<IndentMode> d_indentMode;
    std::optional<IndentStyle> d_indentStyle;
    std::optional<int> d_indentWidth;
    std::optional<int> d_tabWidth;
    std::optional<QString> d_colorScheme;
    std::optional<LineNumberStyle> d_lineNumberStyle;
    std::optional<bool> d_showControlChars;
    std::optional<bool> d_autoBrackets;
    std::optional<bool> d_autoTriggerCompletions;
    std::optional<int> d_autoBackupInterval;
};

}
