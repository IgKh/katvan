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
#pragma once

#include <QDialog>

#include "katvan_editorsettings.h"

#include "typstdriver_compilersettings.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QListView;
class QRadioButton;
class QSpinBox;
class QStringListModel;
QT_END_NAMESPACE

namespace katvan {

class EditorSettingsTab;
class CompilerSettingsTab;
class PathList;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget* parent = nullptr);

    EditorSettings editorSettings() const;
    void setEditorSettings(const EditorSettings& settings);

    typstdriver::TypstCompilerSettings compilerSettings() const;
    void setCompilerSettings(const typstdriver::TypstCompilerSettings& settings);

private:
    void setupUI();

    EditorSettingsTab* d_editorSettingsTab;
    CompilerSettingsTab* d_compilerSettingsTab;
};

class EditorSettingsTab : public QWidget
{
    Q_OBJECT

public:
    EditorSettingsTab(QWidget* parent = nullptr);

    EditorSettings settings() const;
    void setSettings(const EditorSettings& settings);

private slots:
    void updateControlStates();
    void updateFontSizes();

private:
    void setupUI();

    QFontComboBox* d_editorFontComboBox;
    QComboBox* d_editorFontSizeComboBox;
    QComboBox* d_colorScheme;
    QComboBox* d_lineNumberStyle;
    QCheckBox* d_showControlChars;
    QComboBox* d_indentMode;
    QRadioButton* d_indentWithSpaces;
    QRadioButton* d_indentWithTabs;
    QSpinBox* d_indentWidth;
    QSpinBox* d_tabWidth;
    QCheckBox* d_autoBrackets;
    QCheckBox* d_autoTriggerCompletions;
    QSpinBox* d_backupInterval;
};

class CompilerSettingsTab : public QWidget
{
    Q_OBJECT

public:
    CompilerSettingsTab(QWidget* parent = nullptr);

    typstdriver::TypstCompilerSettings settings() const;
    void setSettings(const typstdriver::TypstCompilerSettings& settings);

private slots:
    void updateCacheSize();
    void browseCache();

protected:
    void showEvent(QShowEvent* event);

private:
    void setupUI();

    QCheckBox* d_allowPreviewPackages;
    PathList* d_allowedPaths;
    QLabel* d_cacheSize;
};

class PathList : public QWidget
{
    Q_OBJECT

public:
    PathList(QWidget* parent = nullptr);

    QStringList paths() const;
    void setPaths(const QStringList& paths);

private slots:
    void addPath();
    void removePath();
    void currentPathChanged();

private:
    QStringListModel* d_pathsModel;
    QListView* d_pathsListView;
    QPushButton* d_removePathButton;
};

}
