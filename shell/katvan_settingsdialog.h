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

#include "katvan_editorsettings.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QRadioButton;
class QSpinBox;
QT_END_NAMESPACE

namespace katvan {

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
    QCheckBox* d_showControlChars;
    QComboBox* d_indentMode;
    QRadioButton* d_indentWithSpaces;
    QRadioButton* d_indentWithTabs;
    QSpinBox* d_indentWidth;
    QSpinBox* d_tabWidth;
};

}
