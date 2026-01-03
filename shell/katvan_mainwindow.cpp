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
#include "katvan_backuphandler.h"
#include "katvan_compileroutput.h"
#include "katvan_exportdialog.h"
#include "katvan_infobar.h"
#include "katvan_labelsview.h"
#include "katvan_mainwindow.h"
#include "katvan_outlineview.h"
#include "katvan_previewer.h"
#include "katvan_recentfiles.h"
#include "katvan_searchbar.h"
#include "katvan_settingsdialog.h"
#include "katvan_utils.h"

#include "katvan_aboutdialog.h"
#include "katvan_completionmanager.h"
#include "katvan_diagnosticsmodel.h"
#include "katvan_document.h"
#include "katvan_editor.h"
#include "katvan_editorsettings.h"
#include "katvan_spellchecker.h"
#include "katvan_symbolpicker.h"
#include "katvan_typstdriverwrapper.h"
#include "katvan_wordcounter.h"

#include "typstdriver_compilersettings.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMovie>
#include <QPushButton>
#include <QSessionManager>
#include <QSettings>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace katvan {

static constexpr QLatin1StringView SETTING_MAIN_WINDOW_STATE = QLatin1StringView("MainWindow/state");
static constexpr QLatin1StringView SETTING_MAIN_WINDOW_GEOMETRY = QLatin1StringView("MainWindow/geometry");
static constexpr QLatin1StringView SETTING_SPELLING_DICT = QLatin1StringView("spelling/dict");
static constexpr QLatin1StringView SETTING_EDITOR_MODE = QLatin1StringView("editor/mode");

MainWindow::MainWindow()
    : QMainWindow(nullptr)
    , d_pendingExport(ExportType::NONE)
    , d_suppressFileChangeNotification(false)
{
    setObjectName("katvanMainWindow");

    d_recentFiles = new RecentFiles(this);
    connect(d_recentFiles, &RecentFiles::fileSelected, this, &MainWindow::openNamedFile);

    d_document = new Document(this);
    d_driver = new TypstDriverWrapper(this);
    d_wordCounter = new WordCounter(d_driver, this);

    setIconTheme();
    setupUI();
    setupActions();
    setupStatusBar();

    d_compilerOutput->setModel(d_driver->diagnosticsModel());

    connect(d_document, &Document::contentEdited, d_driver, &TypstDriverWrapper::applyContentEdit);
    connect(d_document, &Document::contentModified, d_driver, &TypstDriverWrapper::updatePreview);
    connect(d_document, &QTextDocument::modificationChanged, this, &QMainWindow::setWindowModified);

    connect(d_driver, &TypstDriverWrapper::previewReady, this, &MainWindow::previewReady);
    connect(d_driver, &TypstDriverWrapper::compilationStatusChanged, this, &MainWindow::compilationStatusChanged);
    connect(d_driver, &TypstDriverWrapper::exportFinished, this, &MainWindow::exportComplete);
    connect(d_driver, &TypstDriverWrapper::jumpToPreview, d_previewer, &Previewer::jumpToPreview);
    connect(d_driver, &TypstDriverWrapper::jumpToEditor, d_editor, qOverload<int, int>(&Editor::goToBlock));
    connect(d_driver, &TypstDriverWrapper::showEditorToolTip, d_editor, &Editor::showToolTip);
    connect(d_driver, &TypstDriverWrapper::showEditorToolTipAtLocation, d_editor, &Editor::showToolTipAtLocation);
    connect(d_driver, &TypstDriverWrapper::completionsReady, d_editor->completionManager(), &CompletionManager::completionsReady);
    connect(d_driver, &TypstDriverWrapper::outlineUpdated, d_outlineView, &OutlineView::outlineUpdated);
    connect(d_driver, &TypstDriverWrapper::labelsUpdated, d_labelsView, &LabelsView::labelsUpdated);

    connect(d_wordCounter, &WordCounter::wordCountChanged, this, &MainWindow::updateWordCount);

    d_backupHandler = new BackupHandler(d_editor, this);
    connect(d_document, &Document::contentModified, d_backupHandler, &BackupHandler::editorContentChanged);

    d_fileWatcher = new QFileSystemWatcher(this);
    connect(d_fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::currentFileChangedOnDisk);

    readSettings();
    cursorPositionChanged();
}

void MainWindow::setupUI()
{
    setWindowTitle(QString("%1[*]").arg(QCoreApplication::applicationName()));
    setWindowIcon(QIcon(":/assets/katvan.svg"));

    d_editor = new Editor(d_document);
    connect(d_editor, &Editor::toolTipRequested, d_driver, &TypstDriverWrapper::requestToolTip);
    connect(d_editor, &Editor::goToDefinitionRequested, d_driver, &TypstDriverWrapper::searchDefinition);
    connect(d_editor->completionManager(), &CompletionManager::completionsRequested, d_driver, &TypstDriverWrapper::requestCompletions);
    connect(d_editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::cursorPositionChanged);
    connect(d_editor, &Editor::fontZoomFactorChanged, this, &MainWindow::editorFontZoomFactorChanged);
    connect(d_editor, &Editor::showSymbolPicker, this, &MainWindow::showSymbolPicker);
    connect(d_editor, &Editor::showColorPicker, this, &MainWindow::showColorPicker);

    d_infoBar = new InfoBar();
    d_infoBar->setVisible(false);

    d_searchBar = new SearchBar(d_editor);
    d_searchBar->setVisible(false);

    QWidget* centralWidget = new QWidget();
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(d_infoBar, 0);
    centralLayout->addWidget(d_editor, 1);
    centralLayout->addWidget(d_searchBar, 0);

    setCentralWidget(centralWidget);

    d_previewer = new Previewer(d_driver);
    connect(d_previewer, &Previewer::followCursorEnabled, this, &MainWindow::cursorPositionChanged);

    d_compilerOutput = new CompilerOutput();
    connect(d_compilerOutput, &CompilerOutput::goToPosition, d_editor, qOverload<int, int>(&Editor::goToBlock));

    d_outlineView = new OutlineView();
    connect(d_outlineView, &OutlineView::goToPosition, d_editor, qOverload<int, int>(&Editor::goToBlock));

    d_labelsView = new LabelsView();
    connect(d_labelsView, &LabelsView::goToPosition, d_editor, qOverload<int, int>(&Editor::goToBlock));

    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::VerticalTabs);

    d_previewDock = new QDockWidget(tr("Preview"));
    d_previewDock->setObjectName("previewDockPanel");
    d_previewDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    d_previewDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    d_previewDock->setWidget(d_previewer);
    addDockWidget(Qt::RightDockWidgetArea, d_previewDock);

    d_compilerOutputDock = new QDockWidget(tr("Compiler Output"));
    d_compilerOutputDock->setObjectName("compilerOutputDockPanel");
    d_compilerOutputDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    d_compilerOutputDock->setWidget(d_compilerOutput);
    addDockWidget(Qt::RightDockWidgetArea, d_compilerOutputDock);

    d_outlineDock = new QDockWidget(tr("Outline"));
    d_outlineDock->setObjectName("outlineDockPanel");
    d_outlineDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    d_outlineDock->setWidget(d_outlineView);
    addDockWidget(Qt::LeftDockWidgetArea, d_outlineDock);

    d_labelsDock = new QDockWidget(tr("Labels"));
    d_labelsDock->setObjectName("labelsDockPanel");
    d_labelsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    d_labelsDock->setWidget(d_labelsView);
    addDockWidget(Qt::LeftDockWidgetArea, d_labelsDock);
    tabifyDockWidget(d_outlineDock, d_labelsDock);
}

void MainWindow::setupActions()
{
    /*
     * File Menu
     */
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newFileAction = fileMenu->addAction(tr("&New"), this, &MainWindow::newFile);
    newFileAction->setIcon(utils::themeIcon("document-new"));
    newFileAction->setShortcut(QKeySequence::New);
    newFileAction->setMenuRole(QAction::NoRole);

    fileMenu->addSeparator();

    QAction* openFileAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::openFile);
    openFileAction->setIcon(utils::themeIcon("document-open"));
    openFileAction->setShortcut(QKeySequence::Open);
    openFileAction->setMenuRole(QAction::NoRole);

    d_recentFiles->setMenu(fileMenu->addMenu(tr("&Recent Files")));

    fileMenu->addSeparator();

    QAction* saveFileAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::saveFile);
    saveFileAction->setIcon(utils::themeIcon("document-save"));
    saveFileAction->setShortcut(QKeySequence::Save);
    saveFileAction->setMenuRole(QAction::NoRole);
    connect(d_document, &QTextDocument::modificationChanged, saveFileAction, &QAction::setEnabled);

    QAction* saveFileAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveFileAs);
    saveFileAsAction->setIcon(utils::themeIcon("document-save-as"));
    saveFileAsAction->setShortcut(QKeySequence::SaveAs);
    saveFileAsAction->setMenuRole(QAction::NoRole);

    fileMenu->addSeparator();

    QAction* exportAsAction = fileMenu->addAction(tr("&Export As..."), this, &MainWindow::exportAs);
    exportAsAction->setIcon(utils::themeIcon("document-send"));
    exportAsAction->setMenuRole(QAction::NoRole);

    QAction* exportPdfAction = fileMenu->addAction(tr("Quick Export &PDF..."), this, &MainWindow::exportPdf);
    exportPdfAction->setIcon(utils::themeIcon("application-pdf"));
    exportPdfAction->setMenuRole(QAction::NoRole);

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("&Quit"), qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    quitAction->setIcon(utils::themeIcon("application-exit"));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);

    /*
     * Edit Menu
     */
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    QAction* undoAction = editMenu->addAction(tr("&Undo"), d_editor, &QTextEdit::undo);
    undoAction->setIcon(utils::themeIcon("edit-undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setMenuRole(QAction::NoRole);
    undoAction->setEnabled(false);
    connect(d_editor, &QTextEdit::undoAvailable, undoAction, &QAction::setEnabled);

    QAction* redoAction = editMenu->addAction(tr("&Redo"), d_editor, &QTextEdit::redo);
    redoAction->setIcon(utils::themeIcon("edit-redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setMenuRole(QAction::NoRole);
    redoAction->setEnabled(false);
    connect(d_editor, &QTextEdit::redoAvailable, redoAction, &QAction::setEnabled);

    editMenu->addSeparator();

    QAction* cutAction = editMenu->addAction(tr("Cu&t"), d_editor, &QTextEdit::cut);
    cutAction->setIcon(utils::themeIcon("edit-cut"));
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setMenuRole(QAction::NoRole);
    cutAction->setEnabled(false);
    connect(d_editor, &QTextEdit::copyAvailable, cutAction, &QAction::setEnabled);

    QAction* copyAction = editMenu->addAction(tr("&Copy"), d_editor, &QTextEdit::copy);
    copyAction->setIcon(utils::themeIcon("edit-copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setMenuRole(QAction::NoRole);
    copyAction->setEnabled(false);
    connect(d_editor, &QTextEdit::copyAvailable, copyAction, &QAction::setEnabled);

    QAction* pasteAction = editMenu->addAction(tr("&Paste"), d_editor, &QTextEdit::paste);
    pasteAction->setIcon(utils::themeIcon("edit-paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setMenuRole(QAction::NoRole);
    pasteAction->setEnabled(d_editor->canPaste());
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this, pasteAction]() {
        pasteAction->setEnabled(d_editor->canPaste());
    });

    editMenu->addSeparator();

    QMenu* insertMenu = d_editor->createInsertMenu();
    insertMenu->setTitle(tr("&Insert"));
    editMenu->addMenu(insertMenu);

    editMenu->addSeparator();

    QAction* findAction = editMenu->addAction(tr("&Find..."), d_searchBar, &SearchBar::ensureFindVisible);
    findAction->setIcon(utils::themeIcon("edit-find"));
    findAction->setShortcut(QKeySequence::Find);
    findAction->setMenuRole(QAction::NoRole);

    QAction* replaceAction = editMenu->addAction(tr("&Replace..."), d_searchBar, &SearchBar::ensureReplaceVisible);
    replaceAction->setIcon(utils::themeIcon("edit-find-replace"));
    replaceAction->setShortcut(QKeySequence::Replace);
    replaceAction->setMenuRole(QAction::NoRole);

    /*
     * Go Menu
     */
    QMenu* goMenu = menuBar()->addMenu(tr("&Go"));

    QAction* goBackAction = goMenu->addAction(tr("&Back"), d_editor, &Editor::goBack);
    goBackAction->setIcon(utils::themeIcon("go-previous"));
    goBackAction->setShortcut(QKeySequence::Back);
    goBackAction->setMenuRole(QAction::NoRole);
    goBackAction->setEnabled(false);
    connect(d_editor, &Editor::goBackAvailable, goBackAction, &QAction::setEnabled);

    QAction* goForwardAction = goMenu->addAction(tr("&Forward"), d_editor, &Editor::goForward);
    goForwardAction->setIcon(utils::themeIcon("go-next"));
    goForwardAction->setShortcut(QKeySequence::Forward);
    goForwardAction->setMenuRole(QAction::NoRole);
    goForwardAction->setEnabled(false);
    connect(d_editor, &Editor::goForwardAvailable, goForwardAction, &QAction::setEnabled);

    goMenu->addSeparator();

    QAction* gotoLineAction = goMenu->addAction(tr("Go to &Line..."), this, &MainWindow::goToLine);
    gotoLineAction->setShortcut(Qt::CTRL | Qt::Key_G);
    gotoLineAction->setMenuRole(QAction::NoRole);

    QAction* jumpToPreviewAction = goMenu->addAction(tr("&Jump to Preview"), this, &MainWindow::jumpToPreview);
    jumpToPreviewAction->setShortcut(Qt::CTRL | Qt::Key_J);
    jumpToPreviewAction->setMenuRole(QAction::NoRole);

    QAction* gotoDefinitionAction = goMenu->addAction(tr("Go to &Definition"), this, &MainWindow::goToDefinition);
    gotoDefinitionAction->setShortcut(Qt::Key_F12);
    gotoDefinitionAction->setMenuRole(QAction::NoRole);

    /*
     * View Menu
     */
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    QAction* enlargeFontAction = viewMenu->addAction(tr("&Enlarge Font"), d_editor, &Editor::increaseFontSize);
    enlargeFontAction->setIcon(utils::themeIcon("zoom-in"));
    enlargeFontAction->setShortcut(QKeySequence::ZoomIn);
    enlargeFontAction->setMenuRole(QAction::NoRole);

    QAction* shrinkFontAction = viewMenu->addAction(tr("&Shrink Font"), d_editor, &Editor::decreaseFontSize);
    shrinkFontAction->setIcon(utils::themeIcon("zoom-out"));
    shrinkFontAction->setShortcut(QKeySequence::ZoomOut);
    shrinkFontAction->setMenuRole(QAction::NoRole);

    QAction* resetFontSizeAction = viewMenu->addAction(tr("&Reset Font Size"), d_editor, &Editor::resetFontSize);
    resetFontSizeAction->setIcon(utils::themeIcon("zoom-original"));
    resetFontSizeAction->setShortcut(Qt::CTRL | Qt::Key_0);
    resetFontSizeAction->setMenuRole(QAction::NoRole);

    viewMenu->addSeparator();

    viewMenu->addAction(d_previewDock->toggleViewAction());
    viewMenu->addAction(d_compilerOutputDock->toggleViewAction());
    viewMenu->addAction(d_outlineDock->toggleViewAction());
    viewMenu->addAction(d_labelsDock->toggleViewAction());

    /*
     * Tools Menu
     */
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));

    QAction* settingsAction = toolsMenu->addAction(tr("&Settings..."), this, &MainWindow::showSettingsDialog);
    settingsAction->setMenuRole(QAction::PreferencesRole);

    QAction* spellingAction = toolsMenu->addAction(tr("Spell &Checking..."), this, &MainWindow::changeSpellCheckingDictionary);
    spellingAction->setIcon(utils::themeIcon("tools-check-spelling"));
    spellingAction->setMenuRole(QAction::NoRole);

    /*
     * Help Menu
     */
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction* docsAction = helpMenu->addAction(tr("Typst &Documentation..."), this, &MainWindow::showTypstDocs);
    docsAction->setIcon(utils::themeIcon("help-contents"));
    docsAction->setMenuRole(QAction::NoRole);

    helpMenu->addSeparator();

    QAction* aboutAction = helpMenu->addAction(tr("&About..."), this, &MainWindow::showAbout);
    aboutAction->setIcon(utils::themeIcon("help-about"));
    aboutAction->setMenuRole(QAction::AboutRole);
}

void MainWindow::setupStatusBar()
{
    auto buildStatusBarButton = []() {
        QToolButton* button = new QToolButton();
        button->setAutoRaise(true);
        button->setMaximumHeight(18);
        return button;
    };

    d_compilationStatusButton = buildStatusBarButton();
    d_compilationStatusButton->setToolTip(tr("Compilation status"));
    d_compilationStatusButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(d_compilationStatusButton, &QToolButton::clicked, d_compilerOutputDock, &QDockWidget::show);

    statusBar()->addPermanentWidget(d_compilationStatusButton);

    d_compilingMovie = new QMovie(this);
    d_compilingMovie->setFileName(":/assets/spinner.gif");
    connect(d_compilingMovie, &QMovie::frameChanged, this, [this]() {
        d_compilationStatusButton->setIcon(d_compilingMovie->currentPixmap());
    });

    d_wordCountButton = buildStatusBarButton();

    statusBar()->addPermanentWidget(d_wordCountButton);

    d_cursorPosButton = buildStatusBarButton();
    connect(d_cursorPosButton, &QToolButton::clicked, this, &MainWindow::goToLine);

    statusBar()->addPermanentWidget(d_cursorPosButton);

    d_fontZoomFactorButton = buildStatusBarButton();
    d_fontZoomFactorButton->setVisible(false);
    d_fontZoomFactorButton->setToolTip(tr("Font Zoom"));
    connect(d_fontZoomFactorButton, &QToolButton::clicked, d_editor, &Editor::resetFontSize);

    statusBar()->addPermanentWidget(d_fontZoomFactorButton);

    d_spellingButton = buildStatusBarButton();
    d_spellingButton->setToolTip(tr("Spell checking dictionary"));
    connect(d_spellingButton, &QToolButton::clicked, this, &MainWindow::changeSpellCheckingDictionary);

    statusBar()->addPermanentWidget(d_spellingButton);

    d_cursorStyleButton = buildStatusBarButton();
    d_cursorStyleButton->setText(tr("Logical"));
    d_cursorStyleButton->setToolTip(tr("Cursor movement style"));
    connect(d_cursorStyleButton, &QToolButton::clicked, this, &MainWindow::toggleCursorMovementStyle);

    statusBar()->addPermanentWidget(d_cursorStyleButton);
}

void MainWindow::readSettings()
{
    QSettings settings;

    if (settings.contains(SETTING_MAIN_WINDOW_GEOMETRY)) {
        QByteArray mainWindowGeometry = settings.value(SETTING_MAIN_WINDOW_GEOMETRY).toByteArray();
        restoreGeometry(mainWindowGeometry);
    }

    if (settings.contains(SETTING_MAIN_WINDOW_STATE)) {
        QByteArray mainWindowState = settings.value(SETTING_MAIN_WINDOW_STATE).toByteArray();
        restoreState(mainWindowState);
    }

    EditorSettings editorSettings;
    if (settings.contains(SETTING_EDITOR_MODE)) {
        QString mode = settings.value(SETTING_EDITOR_MODE).toString();
        editorSettings = EditorSettings(mode, EditorSettings::ModeSource::SETTINGS);
    }
    d_editor->applySettings(editorSettings);

    d_backupHandler->setBackupInterval(editorSettings.autoBackupInterval());
    d_driver->setCompilerSettings(typstdriver::TypstCompilerSettings(settings));
    d_recentFiles->restoreRecents(settings);
    d_previewer->restoreSettings(settings);
    restoreSpellingDictionary(settings);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(SETTING_MAIN_WINDOW_STATE, saveState());
    settings.setValue(SETTING_MAIN_WINDOW_GEOMETRY, saveGeometry());

    d_previewer->saveSettings(settings);
}

void MainWindow::loadFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Loading file %1 failed: %2").arg(utils::formatFilePath(fileName), file.errorString()));

        return;
    }

    QTextStream stream(&file);
    d_document->setDocumentText(stream.readAll());
    d_editor->setTextCursor(QTextCursor(d_document));

    d_previewer->reset();
    d_wordCounter->reset();
    d_outlineView->resetView();
    d_labelsView->resetView();

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("Loaded %1").arg(utils::formatFilePath(d_currentFileName)));
}

bool MainWindow::maybeSave()
{
    if (!d_document->isModified()) {
        return true;
    }

    QMessageBox::StandardButton res = QMessageBox::warning(
        this,
        QCoreApplication::applicationName(),
        tr("The file %1 has been modified.\nDo you want to save your changes?").arg(d_currentFileShortName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (res == QMessageBox::Save) {
        return saveFile();
    }
    else if (res == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::setCurrentFile(const QString& fileName)
{
    if (!d_fileWatcher->files().isEmpty()) {
        d_fileWatcher->removePaths(d_fileWatcher->files());
    }

    if (fileName.isEmpty()) {
        d_currentFileName.clear();
        d_currentFileShortName = tr("Untitled");
    }
    else {
        QFileInfo info(fileName);
        d_currentFileName = info.exists() ? info.canonicalFilePath() : fileName;
        d_currentFileShortName = info.fileName();

        d_recentFiles->addRecent(d_currentFileName);

        if (info.exists()) {
            d_fileWatcher->addPath(d_currentFileName);
        }
    }

    d_editor->checkForModelines();

    setWindowTitle(QString("%1[*] - %2").arg(
        QCoreApplication::applicationName(),
        d_currentFileShortName));

    d_document->setModified(false);
    setWindowModified(false);

    d_driver->resetInputFile(fileName);
    d_driver->setSource(d_document->textForPreview());

    QString previousTmpFile = d_backupHandler->resetSourceFile(fileName);
    if (!previousTmpFile.isEmpty()) {
        tryRecover(fileName, previousTmpFile);
    }

    d_infoBar->hide();
    d_searchBar->resetSearchRange();
    d_searchBar->hide();
}

static bool areFilesEqual(const QString& fileName1, const QString& fileName2)
{
    QFile file1(fileName1);
    QFile file2(fileName2);

    if (!file1.open(QIODevice::ReadOnly) || !file2.open(QIODevice::ReadOnly)) {
        return false;
    }

    while (!file1.atEnd() && !file2.atEnd())
    {
        QByteArray buff1 = file1.read(4096);
        QByteArray buff2 = file2.read(4096);

        if (buff1 != buff2) {
            return false;
        }
    }
    return file1.atEnd() && file2.atEnd();
}

void MainWindow::tryRecover(const QString& fileName, const QString& tmpFile)
{
    qDebug() << "left over temporary file" << tmpFile << "was found for" << fileName;

    QFileInfo fileInfo(fileName);
    QFileInfo tmpInfo(tmpFile);

    if (!tmpInfo.exists()) {
        return;
    }
    if (areFilesEqual(fileName, tmpFile)) {
        QFile::remove(tmpFile);
        return;
    }

    QString msg = tr("Unsaved changes were found for the file %1. Would you like to recover these changes?")
        .arg(fileInfo.fileName());

    QString timeDiff;
    if (tmpInfo.lastModified() > fileInfo.lastModified()) {
        timeDiff = tr("newer than last saved version");
    }
    else if (tmpInfo.lastModified() < fileInfo.lastModified()) {
        timeDiff = tr("older than last saved version");
    }
    else {
        timeDiff = tr("same time as last saved version");
    }
    QString lastModified = QLocale().toString(tmpInfo.lastModified(), QLocale::ShortFormat);
    QString informativeText = tr("These changes were made at %2 (%3).").arg(lastModified, timeDiff);

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText(msg);
    msgBox.setInformativeText(informativeText);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Discard);

    auto result = msgBox.exec();
    if (result == QMessageBox::Yes) {
        QFile file(tmpFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }

        QTextStream stream(&file);

        QTextCursor cursor(d_document);
        cursor.select(QTextCursor::Document);
        cursor.insertText(stream.readAll());
    }

    QFile::remove(tmpFile);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave()) {
        saveSettings();
        event->accept();
    }
    else {
        event->ignore();
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::ThemeChange || event->type() == QEvent::ApplicationPaletteChange) {
        d_editor->updateEditorTheme();
        setIconTheme();
    }

    return QMainWindow::event(event);
}

void MainWindow::commitSession(QSessionManager& manager)
{
    if (!d_document->isModified()) {
        return;
    }

    if (!manager.allowsInteraction()) {
        return;
    }

    bool ok = maybeSave();

    manager.release();
    if (!ok) {
        manager.cancel();
    }
}

void MainWindow::currentFileChangedOnDisk()
{
    if (d_suppressFileChangeNotification) {
        d_suppressFileChangeNotification = false;
        return;
    }

    d_document->setModified(true);
    d_infoBar->clear();

    QString path = utils::formatFilePath(d_currentFileName);
    QFileInfo info { d_currentFileName };

    QString message;
    if (info.exists()) {
        qDebug() << "File" << d_currentFileName << "modified on disk";
        message = tr("The file %1 was modified on disk").arg(path);

        QPushButton* reloadButton = new QPushButton(utils::themeIcon("view-refresh"), tr("&Reload"));
        connect(reloadButton, &QPushButton::clicked, this, [this]() {
            loadFile(d_currentFileName);
        });

        d_infoBar->addButton(reloadButton);
    }
    else {
        qDebug() << "File" << d_currentFileName << "removed from disk";
        message = tr("The file %1 was deleted or moved on disk").arg(path);
    }

    QPushButton* saveAsButton = new QPushButton(utils::themeIcon("document-save-as"), tr("Save &As..."));
    connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::saveFileAs);

    QPushButton* ignoreButton = new QPushButton(utils::themeIcon("window-close"), tr("&Ignore"));
    connect(ignoreButton, &QPushButton::clicked, d_infoBar, &InfoBar::hide);

    d_infoBar->addButton(saveAsButton);
    d_infoBar->addButton(ignoreButton);
    d_infoBar->showMessage(message);
}

void MainWindow::newFile()
{
    if (!maybeSave()) {
        return;
    }
    d_document->setDocumentText(QString());
    d_previewer->reset();
    d_wordCounter->reset();
    d_outlineView->resetView();
    d_labelsView->resetView();
    setCurrentFile(QString());
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Document"),
        QString(),
        tr("Typst files (*.typ);;All files (*)"));

    openNamedFile(fileName);
}

void MainWindow::openNamedFile(const QString& fileName)
{
    if (fileName.isEmpty()) {
        return;
    }

    QFileInfo info(fileName);
    if (!info.exists()) {
        QMessageBox::warning(
            this,
            QCoreApplication::applicationName(),
            tr("The file %1 no longer exists").arg(utils::formatFilePath(fileName)));

        d_recentFiles->removeFile(fileName);
        return;
    }

    if (info.canonicalFilePath() == d_currentFileName) {
        return;
    }

    if (maybeSave()) {
        loadFile(fileName);
    }
}

bool MainWindow::saveFile()
{
    if (d_currentFileName.isEmpty()) {
        return saveFileAs();
    }

    QFile file(d_currentFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Opening file %1 for saving failed: %2").arg(utils::formatFilePath(d_currentFileName), file.errorString()));

        return false;
    }

    d_suppressFileChangeNotification = true;

    QTextStream stream(&file);
    stream << d_editor->toPlainText();
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Saving file %1 failed: %2").arg(utils::formatFilePath(d_currentFileName), file.errorString()));

        d_suppressFileChangeNotification = false;
        return false;
    }

    d_document->setModified(false);
    d_editor->checkForModelines();
    d_infoBar->hide();
    statusBar()->showMessage(tr("Saved %1").arg(utils::formatFilePath(d_currentFileName)));

    // If this is a new file, watching it in setCurrentFile would have failed.
    // Now that it definitely exists, add it here.
    if (d_fileWatcher->files().isEmpty()) {
        d_fileWatcher->addPath(d_currentFileName);
    }

    return true;
}

bool MainWindow::saveFileAs()
{
    QFileDialog dialog(this, tr("Save Document"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("Typst files (*.typ)"));
    dialog.setDefaultSuffix("typ");

    if (!d_currentFileName.isEmpty()) {
        QFileInfo info(d_currentFileName);
        dialog.setDirectory(info.dir());
    }

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QString fileName = dialog.selectedFiles().at(0);

    setCurrentFile(fileName);
    return saveFile();
}

bool MainWindow::verifyCanExport(MainWindow::ExportType type)
{
    if (d_driver->status() != TypstDriverWrapper::Status::SUCCESS &&
        d_driver->status() != TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS) {
        if (d_driver->status() == TypstDriverWrapper::Status::FAILED) {
            QMessageBox::critical(
                this,
                QCoreApplication::applicationName(),
                tr("The document %1 has errors.\nTo export the document, please correct them.").arg(d_currentFileShortName));
        }
        else if (d_driver->status() == TypstDriverWrapper::Status::INITIALIZED) {
            d_driver->setSource(d_document->textForPreview());
            d_driver->updatePreview();
            d_pendingExport = type;
        }
        return false;
    }
    return true;
}

void MainWindow::exportAs()
{
    if (!d_exportDialog) {
        d_exportDialog = new ExportDialog(d_driver, this);
    }

    if (!verifyCanExport(ExportType::FULL)) {
        return;
    }

    d_exportDialog->setSourceFile(d_currentFileName);
    d_exportDialog->open();
}

void MainWindow::exportPdf()
{
    if (!verifyCanExport(ExportType::PDF)) {
        return;
    }

    QString targetFileName = utils::showPdfExportDialog(this, d_currentFileName);
    if (targetFileName.isEmpty()) {
        return;
    }

    qApp->setOverrideCursor(Qt::WaitCursor);
    d_driver->exportToPdf(targetFileName);
}

void MainWindow::goToLine()
{
    int blockCount = d_editor->document()->blockCount();

    bool ok;
    int line = QInputDialog::getInt(
        this,
        tr("Go to Line"),
        tr("Enter a line number (max %1)").arg(blockCount),
        d_editor->textCursor().blockNumber() + 1,
        1,
        blockCount,
        1,
        &ok);

    if (ok) {
        d_editor->goToBlock(line - 1, 0);
    }
}

void MainWindow::jumpToPreview()
{
    QTextCursor cursor = d_editor->textCursor();
    d_driver->forwardSearch(cursor.blockNumber(), cursor.positionInBlock(), d_previewer->currentPage());
}

void MainWindow::goToDefinition()
{
    QTextCursor cursor = d_editor->textCursor();
    d_driver->searchDefinition(cursor.blockNumber(), cursor.positionInBlock());
}

void MainWindow::showTypstDocs()
{
    QDesktopServices::openUrl(QUrl("https://typst.app/docs/"));
}

void MainWindow::showAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::setIconTheme()
{
    QStringList paths;
    if (EditorTheme::isAppInDarkMode()) {
        paths.append(":/icons-dark");
    }
    paths.append(":/icons");

    QIcon::setFallbackSearchPaths(paths);
}

void MainWindow::restoreSpellingDictionary(const QSettings& settings)
{
    QString dictName = settings.value(SETTING_SPELLING_DICT, QString()).toString();
    QString dictPath;

    SpellChecker* checker = SpellChecker::instance();
    if (!dictName.isEmpty()) {
        QMap<QString, QString> allDicts = checker->findDictionaries();
        if (!allDicts.contains(dictName)) {
            dictName.clear();
        }
        else {
            dictPath = allDicts[dictName];
        }
    }

    checker->setCurrentDictionary(dictName, dictPath);
    d_spellingButton->setText(dictName.isEmpty() ? tr("None") : dictName);
}

void MainWindow::changeSpellCheckingDictionary()
{
    SpellChecker* checker = SpellChecker::instance();
    QMap<QString, QString> dicts = checker->findDictionaries();

    QStringList dictNames = { "" };
    QStringList dictLabels = { tr("None") };

    for (auto kit = dicts.keyBegin(); kit != dicts.keyEnd(); ++kit) {
        dictNames.append(*kit);
        dictLabels.append(QString("%1 - %2").arg(
            *kit,
            checker->dictionaryDisplayName(*kit)));
    }

    QString currentDict = checker->currentDictionaryName();
    int index = dictNames.indexOf(currentDict);
    if (index < 0) {
        index = 0;
    }

    bool ok;
    QString result = QInputDialog::getItem(this,
        tr("Spell Checking"),
        tr("Select dictionary to use for spell checking"),
        dictLabels,
        index,
        false,
        &ok);

    if (!ok) {
        return;
    }

    QString selectedDictName = dictNames[dictLabels.indexOf(result)];
    checker->setCurrentDictionary(selectedDictName, dicts.value(selectedDictName));
    d_spellingButton->setText(selectedDictName.isEmpty() ? result : selectedDictName);

    QSettings settings;
    settings.setValue(SETTING_SPELLING_DICT, selectedDictName);
}

void MainWindow::cursorPositionChanged()
{
    QTextCursor cursor = d_editor->textCursor();
    int line = cursor.blockNumber();
    int column = cursor.positionInBlock();

    d_cursorPosButton->setText(tr("Line %1, Col %2")
        .arg(line + 1)
        .arg(column));

    if (d_previewer->shouldFollowEditorCursor()) {
        d_driver->forwardSearch(line, column, d_previewer->currentPage());
    }

    d_outlineView->currentLineChanged(line);
}

void MainWindow::editorFontZoomFactorChanged(qreal factor)
{
    int factorPercentage = qRound(factor * 100);
    if (factorPercentage == 100) {
        d_fontZoomFactorButton->hide();
    }
    else {
        d_fontZoomFactorButton->setText(QStringLiteral("%1%").arg(factorPercentage));
        d_fontZoomFactorButton->show();
    }
}

void MainWindow::toggleCursorMovementStyle()
{
    if (d_editor->document()->defaultCursorMoveStyle() == Qt::LogicalMoveStyle) {
        d_editor->document()->setDefaultCursorMoveStyle(Qt::VisualMoveStyle);
        d_cursorStyleButton->setText(tr("Visual"));
    }
    else {
        d_editor->document()->setDefaultCursorMoveStyle(Qt::LogicalMoveStyle);
        d_cursorStyleButton->setText(tr("Logical"));
    }
}

void MainWindow::showSettingsDialog()
{
    if (!d_settingsDialog) {
        d_settingsDialog = new SettingsDialog(this);
        connect(d_settingsDialog, &QDialog::accepted, this, &MainWindow::settingsDialogAccepted);
    }

    QSettings settings;

    QString mode = settings.value(SETTING_EDITOR_MODE).toString();
    EditorSettings editorSettings { mode, EditorSettings::ModeSource::SETTINGS };

    typstdriver::TypstCompilerSettings compilerSettings { settings };

    d_settingsDialog->setEditorSettings(editorSettings);
    d_settingsDialog->setCompilerSettings(compilerSettings);
    d_settingsDialog->open();
}

void MainWindow::settingsDialogAccepted()
{
    EditorSettings editorSettings = d_settingsDialog->editorSettings();
    typstdriver::TypstCompilerSettings compilerSettings = d_settingsDialog->compilerSettings();

    d_editor->applySettings(editorSettings);
    d_backupHandler->setBackupInterval(editorSettings.autoBackupInterval());

    d_driver->setCompilerSettings(compilerSettings);
    if (!compilerSettings.allowPreviewPackages()) {
        d_driver->discardLookupCaches();
    }
    d_driver->updatePreview();

    QSettings settings;
    settings.setValue(SETTING_EDITOR_MODE, editorSettings.toModeLine());
    compilerSettings.save(settings);
}

void MainWindow::showSymbolPicker()
{
    if (!d_symbolPickerDialog) {
        d_symbolPickerDialog = new SymbolPicker(d_driver, this);
        connect(d_symbolPickerDialog, &QDialog::accepted, this, [this]() {
            d_editor->insertSymbol(d_symbolPickerDialog->selectedSymbolName());
        });
    }

    d_symbolPickerDialog->open();
}

void MainWindow::showColorPicker()
{
    QColor result = QColorDialog::getColor(
        Qt::black,
        this,
        tr("Color Picker"),
        QColorDialog::ShowAlphaChannel);

    if (result.isValid()) {
        d_editor->insertColor(result);
    }
}

void MainWindow::previewReady()
{
    if (d_pendingExport == ExportType::FULL) {
        QTimer::singleShot(0, this, &MainWindow::exportAs);
    }
    else if (d_pendingExport == ExportType::PDF) {
        QTimer::singleShot(0, this, &MainWindow::exportPdf);
    }
    d_pendingExport = ExportType::NONE;
}

void MainWindow::compilationStatusChanged()
{
    TypstDriverWrapper::Status status = d_driver->status();

    if (status != TypstDriverWrapper::Status::PROCESSING) {
        d_compilingMovie->stop();
    }

    d_compilerOutput->adjustColumnWidths();
    d_editor->setSourceDiagnostics(d_driver->diagnosticsModel()->sourceDiagnostics());

    if (status == TypstDriverWrapper::Status::PROCESSING) {
        d_compilationStatusButton->setText(tr("Compiling..."));
        d_compilingMovie->start();
    }
    else if (status == TypstDriverWrapper::Status::SUCCESS) {
        d_compilationStatusButton->setText(tr("No Errors"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-success.svg"));
    }
    else if (status == TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS) {
        d_compilationStatusButton->setText(tr("Warnings"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-warning.svg"));
    }
    else if (status == TypstDriverWrapper::Status::FAILED) {
        d_compilerOutputDock->show();
        d_compilationStatusButton->setText(tr("Errors"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-error.svg"));
    }
    else {
        d_compilationStatusButton->setText(QString());
        d_compilationStatusButton->setIcon(QIcon());
    }
}

void MainWindow::exportComplete(bool ok)
{
    d_compilerOutput->adjustColumnWidths();
    qApp->restoreOverrideCursor();
    if (!ok) {
        d_compilerOutput->show();
    }
}

void MainWindow::updateWordCount(size_t wordCount)
{
    d_wordCountButton->setText(tr("%n Word(s)", "", static_cast<int>(wordCount)));
}

}

#include "moc_katvan_mainwindow.cpp"
