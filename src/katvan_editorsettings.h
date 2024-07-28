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
#pragma once

#include <QDialog>

#include <optional>

QT_BEGIN_NAMESPACE
class QComboBox;
class QFontComboBox;
class QRadioButton;
class QSpinBox;
QT_END_NAMESPACE

namespace katvan {

class EditorSettings
{
    Q_GADGET

public:
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
    explicit EditorSettings(const QString& mode) { parseModeLine(mode); }

    bool operator==(const EditorSettings&) const = default;

    QString toModeLine() const;

    QString fontFamily() const;
    int fontSize() const;
    QFont font() const;

    IndentMode indentMode() const;
    IndentStyle indentStyle() const;
    int indentWidth() const;
    int tabWidth() const;

    LineNumberStyle lineNumberStyle() const;

    void setFontFamily(const QString& fontFamily) { d_fontFamily = fontFamily; }
    void setFontSize(int fontSize) { d_fontSize = fontSize; }
    void setIndentMode(IndentMode indentMode) { d_indentMode = indentMode; }
    void setIndentStyle(IndentStyle indentStyle) { d_indentStyle = indentStyle; }
    void setIndentWidth(int indentWidth) { d_indentWidth = indentWidth; }
    void setTabWidth(int tabWidth) { d_tabWidth = tabWidth; }
    void setLineNumberStyle(LineNumberStyle style) { d_lineNumberStyle = style; }

    bool hasFontFamily() const { return d_fontFamily.has_value(); }
    bool hasFontSize() const { return d_fontSize.has_value(); }
    bool hasIndentMode() const { return d_indentMode.has_value(); }
    bool hasIndentStyle() const { return d_indentStyle.has_value(); }
    bool hasIndentWidth() const { return d_indentWidth.has_value(); }
    bool hasTabWidth() const { return d_tabWidth.has_value(); }
    bool hasLineNumberStyle() const { return d_lineNumberStyle.has_value(); }

    void mergeSettings(const EditorSettings& other);

private:
    void parseModeLine(QString mode);

    std::optional<QString> d_fontFamily;
    std::optional<int> d_fontSize;
    std::optional<IndentMode> d_indentMode;
    std::optional<IndentStyle> d_indentStyle;
    std::optional<int> d_indentWidth;
    std::optional<int> d_tabWidth;
    std::optional<LineNumberStyle> d_lineNumberStyle;
};

class EditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    EditorSettingsDialog(QWidget* parent = nullptr);

    EditorSettings settings() const;
    void setSettings(const EditorSettings& settings);

private slots:
    void updateControlStates();
    void updateFontSizes();

private:
    void setupUI();

    QFontComboBox* d_editorFontComboBox;
    QComboBox* d_editorFontSizeComboBox;
    QComboBox* d_lineNumberStyle;
    QComboBox* d_indentMode;
    QRadioButton* d_indentWithSpaces;
    QRadioButton* d_indentWithTabs;
    QSpinBox* d_indentWidth;
    QSpinBox* d_tabWidth;
};

}
