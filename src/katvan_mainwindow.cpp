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
#include "katvan_backuphandler.h"
#include "katvan_compileroutput.h"
#include "katvan_editor.h"
#include "katvan_editorsettings.h"
#include "katvan_mainwindow.h"
#include "katvan_previewer.h"
#include "katvan_recentfiles.h"
#include "katvan_searchbar.h"
#include "katvan_spellchecker.h"
#include "katvan_typstdriverwrapper.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMovie>
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
    , d_exportPdfPending(false)
{
    setObjectName("katvanMainWindow");

    d_recentFiles = new RecentFiles(this);
    connect(d_recentFiles, &RecentFiles::fileSelected, this, &MainWindow::openNamedFile);

    d_driver = new TypstDriverWrapper(this);
    d_backupHandler = new BackupHandler(this);

    setupUI();
    setupActions();
    setupStatusBar();

    connect(d_driver, &TypstDriverWrapper::previewReady, this, &MainWindow::updatePreview);
    connect(d_driver, &TypstDriverWrapper::compilationStatusChanged, this, &MainWindow::compilationStatusChanged);
    connect(d_driver, &TypstDriverWrapper::outputReady, d_compilerOutput, &CompilerOutput::setOutputLines);

    readSettings();
    cursorPositionChanged();
}

void MainWindow::setupUI()
{
    setWindowTitle(QString("%1[*]").arg(QCoreApplication::applicationName()));
    setWindowIcon(QIcon(":/assets/katvan.svg"));

    d_editor = new Editor();
    connect(d_editor, &Editor::contentModified, d_driver, &TypstDriverWrapper::updatePreview);
    connect(d_editor, &Editor::contentModified, d_backupHandler, &BackupHandler::editorContentChanged);
    connect(d_editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::cursorPositionChanged);
    connect(d_editor->document(), &QTextDocument::modificationChanged, this, &QMainWindow::setWindowModified);

    d_searchBar = new SearchBar(d_editor);
    d_searchBar->setVisible(false);

    QWidget* centralWidget = new QWidget();
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->addWidget(d_editor, 1);
    centralLayout->addWidget(d_searchBar, 0);

    setCentralWidget(centralWidget);

    d_editorSettingsDialog = new EditorSettingsDialog(this);
    connect(d_editorSettingsDialog, &QDialog::accepted, this, &MainWindow::editorSettingsDialogAccepted);

    d_previewer = new Previewer();

    d_compilerOutput = new CompilerOutput();
    connect(d_compilerOutput, &CompilerOutput::goToPosition, d_editor, &Editor::goToBlock);

    setDockOptions(QMainWindow::AnimatedDocks);

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
}

void MainWindow::setupActions()
{
    /*
     * File Menu
     */
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newFileAction = fileMenu->addAction(tr("&New"), this, &MainWindow::newFile);
    newFileAction->setIcon(QIcon::fromTheme("document-new", QIcon(":/icons/document-new.svg")));
    newFileAction->setShortcut(QKeySequence::New);

    fileMenu->addSeparator();

    QAction* openFileAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::openFile);
    openFileAction->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/document-open.svg")));
    openFileAction->setShortcut(QKeySequence::Open);

    d_recentFiles->setMenu(fileMenu->addMenu(tr("&Recent Files")));

    fileMenu->addSeparator();

    QAction* saveFileAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::saveFile);
    saveFileAction->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/document-save.svg")));
    saveFileAction->setShortcut(QKeySequence::Save);
    connect(d_editor->document(), &QTextDocument::modificationChanged, saveFileAction, &QAction::setEnabled);

    QAction* saveFileAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveFileAs);
    saveFileAsAction->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/icons/document-save-as.svg")));
    saveFileAsAction->setShortcut(QKeySequence::SaveAs);

    QAction* exportPdfAction = fileMenu->addAction(tr("&Export PDF..."), this, &MainWindow::exportPdf);
    exportPdfAction->setIcon(QIcon::fromTheme("document-send", QIcon(":/icons/document-send.svg")));

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("&Quit"), qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    quitAction->setIcon(QIcon::fromTheme("application-exit", QIcon(":/icons/application-exit.svg")));
    quitAction->setShortcut(QKeySequence::Quit);

    /*
     * Edit Menu
     */
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    QAction* undoAction = editMenu->addAction(tr("&Undo"), d_editor, &QTextEdit::undo);
    undoAction->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/icons/edit-undo.svg")));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);
    connect(d_editor, &QTextEdit::undoAvailable, undoAction, &QAction::setEnabled);

    QAction* redoAction = editMenu->addAction(tr("&Redo"), d_editor, &QTextEdit::redo);
    redoAction->setIcon(QIcon::fromTheme("edit-redo",  QIcon(":/icons/edit-redo.svg")));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);
    connect(d_editor, &QTextEdit::redoAvailable, redoAction, &QAction::setEnabled);

    editMenu->addSeparator();

    QAction* cutAction = editMenu->addAction(tr("Cu&t"), d_editor, &QTextEdit::cut);
    cutAction->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/icons/edit-cut.svg")));
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setEnabled(false);
    connect(d_editor, &QTextEdit::copyAvailable, cutAction, &QAction::setEnabled);

    QAction* copyAction = editMenu->addAction(tr("&Copy"), d_editor, &QTextEdit::copy);
    copyAction->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.svg")));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(false);
    connect(d_editor, &QTextEdit::copyAvailable, copyAction, &QAction::setEnabled);

    QAction* pasteAction = editMenu->addAction(tr("&Paste"), d_editor, &QTextEdit::paste);
    pasteAction->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/icons/edit-paste.svg")));
    pasteAction->setShortcut(QKeySequence::Paste);
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
    findAction->setIcon(QIcon::fromTheme("edit-find", QIcon(":/icons/edit-find.svg")));
    findAction->setShortcut(QKeySequence::Find);

    QAction* replaceAction = editMenu->addAction(tr("&Replace..."), d_searchBar, &SearchBar::ensureReplaceVisible);
    replaceAction->setIcon(QIcon::fromTheme("edit-find-replace", QIcon(":/icons/edit-find-replace.svg")));
    replaceAction->setShortcut(QKeySequence::Replace);

    QAction* gotoLineAction = editMenu->addAction(tr("&Go to line..."), this, &MainWindow::goToLine);
    gotoLineAction->setShortcut(Qt::CTRL | Qt::Key_G);

    /*
     * View Menu
     */
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addAction(tr("&Editor Settings..."), d_editorSettingsDialog, &QDialog::show);

    viewMenu->addSeparator();

    viewMenu->addAction(d_previewDock->toggleViewAction());
    viewMenu->addAction(d_compilerOutputDock->toggleViewAction());

    /*
     * Tools Menu
     */
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));

    QAction* spellingAction = toolsMenu->addAction(tr("&Spell Checking..."), this, &MainWindow::changeSpellCheckingDictionary);
    spellingAction->setIcon(QIcon::fromTheme("tools-check-spelling", QIcon(":/icons/tools-check-spelling.svg")));

    /*
     * Help Menu
     */
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction* docsAction = helpMenu->addAction(tr("Typst &Documentation..."), this, &MainWindow::showTypstDocs);
    docsAction->setIcon(QIcon::fromTheme("help-contents", QIcon(":/icons/help-contents.svg")));

    helpMenu->addSeparator();

    QAction* aboutAction = helpMenu->addAction(tr("&About..."), this, &MainWindow::showAbout);
    aboutAction->setIcon(QIcon::fromTheme("help-about", QIcon(":/icons/help-about.svg")));
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

    d_cursorPosButton = buildStatusBarButton();
    connect(d_cursorPosButton, &QToolButton::clicked, this, &MainWindow::goToLine);

    statusBar()->addPermanentWidget(d_cursorPosButton);

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
        editorSettings = EditorSettings(settings.value(SETTING_EDITOR_MODE).toString());
    }
    d_editorSettingsDialog->setSettings(editorSettings);
    d_editor->applySettings(editorSettings);

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
            tr("Loading file %1 failed: %2").arg(fileName, file.errorString()));

        return;
    }

    QTextStream stream(&file);
    d_editor->setPlainText(stream.readAll());
    d_previewer->reset();

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("Loaded %1").arg(d_currentFileName));
}

bool MainWindow::maybeSave()
{
    if (!d_editor->document()->isModified()) {
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
    if (fileName.isEmpty()) {
        d_currentFileName.clear();
        d_currentFileShortName = tr("Untitled");

        d_compilerOutput->setInputFileShortName(QString());
    }
    else {
        QFileInfo info(fileName);
        d_currentFileName = info.exists() ? info.canonicalFilePath() : fileName;
        d_currentFileShortName = info.fileName();

        d_recentFiles->addRecent(d_currentFileName);
        d_compilerOutput->setInputFileShortName(d_currentFileShortName);
    }

    d_editor->checkForModelines();

    setWindowTitle(QString("%1[*] - %2").arg(
        QCoreApplication::applicationName(),
        d_currentFileShortName));

    d_editor->document()->setModified(false);
    setWindowModified(false);

    d_driver->resetInputFile(fileName);

    QString previousTmpFile = d_backupHandler->resetSourceFile(fileName);
    if (!previousTmpFile.isEmpty()) {
        tryRecover(fileName, previousTmpFile);
    }

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

        QTextCursor cursor(d_editor->document());
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

void MainWindow::commitSession(QSessionManager& manager)
{
    if (!d_editor->document()->isModified()) {
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

void MainWindow::newFile()
{
    if (!maybeSave()) {
        return;
    }
    d_editor->clear();
    d_previewer->reset();
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
            tr("Opening file %1 for saving failed: %2").arg(d_currentFileName, file.errorString()));

        return false;
    }

    QTextStream stream(&file);
    stream << d_editor->toPlainText();
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Saving file %1 failed: %2").arg(d_currentFileName, file.errorString()));

        return false;
    }

    d_editor->document()->setModified(false);
    d_editor->checkForModelines();
    statusBar()->showMessage(tr("Saved %1").arg(d_currentFileName));

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

void MainWindow::exportPdf()
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
            d_driver->updatePreview(d_editor->toPlainText());
            d_exportPdfPending = true;
        }
        return;
    }

    QFileDialog dialog(this, tr("Export to PDF"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("PDF files (*.pdf)"));
    dialog.setDefaultSuffix("pdf");

    if (!d_currentFileName.isEmpty()) {
        QFileInfo info(d_currentFileName);
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.baseName() + ".pdf");
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString targetFileName = dialog.selectedFiles().at(0);

    QFile file(targetFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Opening write %1 for writing failed: %2").arg(targetFileName, file.errorString()));

        return;
    }

    file.write(d_driver->pdfBuffer());
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

void MainWindow::showTypstDocs()
{
    QDesktopServices::openUrl(QUrl("https://typst.app/docs/"));
}

void MainWindow::showAbout()
{
    QString mainText = tr(
        "<h3>Katvan</h3>"
        "<a href=\"%1\">%1</a>"
        "<p>A bare-bones editor for <i>Typst</i> files, with a bias for RTL</p>"
        "<p>Version %2 (Qt %3)"
    )
    .arg(
        QLatin1String("https://github.com/IgKh/katvan"),
        QCoreApplication::applicationVersion(),
        QLatin1String(qVersion()));

    QString informativeText = tr(
        "<p>Katvan is offered under the terms of the <a href=\"%1\">GNU General Public License Version 3</a>. "
        "Contains icons taken from the <a href=\"%2\">Breeze</a> icon theme.</p>"
    )
    .arg(
        QStringLiteral("https://www.gnu.org/licenses/gpl-3.0.en.html"),
        QStringLiteral("https://invent.kde.org/frameworks/breeze-icons"));

    QMessageBox dlg(QMessageBox::NoIcon, tr("About Katvan"), mainText, QMessageBox::Ok, this);
    dlg.setIconPixmap(windowIcon().pixmap(QSize(128, 128)));
    dlg.setInformativeText(informativeText);
    dlg.exec();
}

void MainWindow::restoreSpellingDictionary(const QSettings& settings)
{
    QString dictName = settings.value(SETTING_SPELLING_DICT, QString()).toString();
    QString dictPath;

    if (!dictName.isEmpty()) {
        QMap<QString, QString> allDicts = SpellChecker::findDictionaries();
        if (!allDicts.contains(dictName)) {
            dictName.clear();
        }
        else {
            dictPath = allDicts[dictName];
        }
    }

    d_editor->spellChecker()->setCurrentDictionary(dictName, dictPath);
    d_spellingButton->setText(dictName.isEmpty() ? tr("None") : dictName);
}

void MainWindow::changeSpellCheckingDictionary()
{
    QMap<QString, QString> dicts = SpellChecker::findDictionaries();

    QStringList dictNames = { "" };
    QStringList dictLabels = { tr("None") };

    for (auto kit = dicts.keyBegin(); kit != dicts.keyEnd(); ++kit) {
        dictNames.append(*kit);
        dictLabels.append(QString("%1 - %2").arg(
            *kit,
            SpellChecker::dictionaryDisplayName(*kit)));
    }

    QString currentDict = d_editor->spellChecker()->currentDictionaryName();
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
    d_editor->spellChecker()->setCurrentDictionary(selectedDictName, dicts.value(selectedDictName));
    d_spellingButton->setText(selectedDictName.isEmpty() ? result : selectedDictName);

    QSettings settings;
    settings.setValue(SETTING_SPELLING_DICT, selectedDictName);
}

void MainWindow::cursorPositionChanged()
{
    QTextCursor cursor = d_editor->textCursor();
    d_cursorPosButton->setText(tr("Line %1, Col %2")
        .arg(cursor.blockNumber() + 1)
        .arg(cursor.positionInBlock()));
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

void MainWindow::editorSettingsDialogAccepted()
{
    EditorSettings editorSettings = d_editorSettingsDialog->settings();

    d_editor->applySettings(editorSettings);

    QSettings settings;
    settings.setValue(SETTING_EDITOR_MODE, editorSettings.toModeLine());
}

void MainWindow::updatePreview(QByteArray pdfBuffer)
{
    if (!d_previewer->updatePreview(pdfBuffer)) {
        return;
    }

    if (d_exportPdfPending) {
        QTimer::singleShot(0, this, &MainWindow::exportPdf);
        d_exportPdfPending = false;
    }
}

void MainWindow::compilationStatusChanged()
{
    TypstDriverWrapper::Status status = d_driver->status();

    if (status != TypstDriverWrapper::Status::PROCESSING) {
        d_compilingMovie->stop();
    }

    if (status == TypstDriverWrapper::Status::PROCESSING) {
        d_compilationStatusButton->setText(tr("Compiling..."));
        d_compilingMovie->start();
    }
    else if (status == TypstDriverWrapper::Status::SUCCESS) {
        d_compilationStatusButton->setText(tr("Success"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-success.svg"));
    }
    else if (status == TypstDriverWrapper::Status::SUCCESS_WITH_WARNINGS) {
        d_compilationStatusButton->setText(tr("Success"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-warning.svg"));
    }
    else if (status == TypstDriverWrapper::Status::FAILED) {
        d_compilerOutputDock->show();
        d_compilationStatusButton->setText(tr("Failed"));
        d_compilationStatusButton->setIcon(QIcon(":/icons/data-error.svg"));
    }
    else {
        d_compilationStatusButton->setText(QString());
        d_compilationStatusButton->setIcon(QIcon());
    }
}

}
