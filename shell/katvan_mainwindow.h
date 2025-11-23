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

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
class QMovie;
class QSessionManager;
class QSettings;
class QToolButton;
QT_END_NAMESPACE

namespace katvan
{

class BackupHandler;
class CompilerOutput;
class ExportDialog;
class InfoBar;
class LabelsView;
class OutlineView;
class Previewer;
class RecentFiles;
class SearchBar;

class Document;
class Editor;
class SettingsDialog;
class SymbolPicker;
class TypstDriverWrapper;
class WordCounter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class ExportType {
        NONE,
        PDF,
        FULL
    };
    Q_ENUM(ExportType)

    MainWindow();

    void loadFile(const QString& fileName);

public slots:
    void commitSession(QSessionManager& manager);

    void newFile();

private slots:
    void openFile();
    void openNamedFile(const QString& fileName);
    bool saveFile();
    bool saveFileAs();
    void exportAs();
    void exportPdf();
    void goToLine();
    void jumpToPreview();
    void goToDefinition();
    void showTypstDocs();
    void showAbout();

    void currentFileChangedOnDisk();
    void cursorPositionChanged();
    void editorFontZoomFactorChanged(qreal factor);
    void changeSpellCheckingDictionary();
    void toggleCursorMovementStyle();
    void showSettingsDialog();
    void settingsDialogAccepted();
    void showSymbolPicker();
    void showColorPicker();
    void previewReady();
    void compilationStatusChanged();
    void exportComplete(bool ok);
    void updateWordCount(size_t wordCount);

private:
    void setupUI();
    void setupActions();
    void setupStatusBar();
    void readSettings();
    void saveSettings();

    void setIconTheme();
    void restoreSpellingDictionary(const QSettings& settings);

    bool maybeSave();
    void setCurrentFile(const QString& fileName);
    void tryRecover(const QString& fileName, const QString& tmpFile);

    void closeEvent(QCloseEvent* event) override;
    bool event(QEvent* event) override;

    bool verifyCanExport(ExportType type);

    QString d_currentFileName;
    QString d_currentFileShortName;
    ExportType d_pendingExport;
    bool d_suppressFileChangeNotification;

    Document* d_document;
    RecentFiles* d_recentFiles;
    TypstDriverWrapper* d_driver;
    BackupHandler* d_backupHandler;
    WordCounter* d_wordCounter;

    Editor* d_editor;
    InfoBar* d_infoBar;
    SearchBar* d_searchBar;
    Previewer* d_previewer;
    CompilerOutput* d_compilerOutput;
    OutlineView* d_outlineView;
    LabelsView* d_labelsView;

    ExportDialog* d_exportDialog = nullptr;
    SettingsDialog* d_settingsDialog = nullptr;
    SymbolPicker* d_symbolPickerDialog = nullptr;

    QFileSystemWatcher* d_fileWatcher;
    QMovie* d_compilingMovie;

    QToolButton* d_compilationStatusButton;
    QToolButton* d_wordCountButton;
    QToolButton* d_cursorPosButton;
    QToolButton* d_fontZoomFactorButton;
    QToolButton* d_spellingButton;
    QToolButton* d_cursorStyleButton;

    QDockWidget* d_previewDock;
    QDockWidget* d_compilerOutputDock;
    QDockWidget* d_outlineDock;
    QDockWidget* d_labelsDock;
};

}
