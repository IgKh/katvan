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
#include "katvan_editor.h"
#include "katvan_mainwindow.h"
#include "katvan_recentfiles.h"
#include "katvan_searchbar.h"
#include "katvan_spellchecker.h"
#include "katvan_typstdriver.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfView>
#include <QPlainTextEdit>
#include <QScrollBar>
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
static constexpr QLatin1StringView SETTING_EDITOR_FONT = QLatin1StringView("editor/font");

MainWindow::MainWindow()
    : QMainWindow(nullptr)
{
    d_recentFiles = new RecentFiles(this);
    connect(d_recentFiles, &RecentFiles::fileSelected, this, &MainWindow::openNamedFile);

    d_driver = new TypstDriver(this);
    connect(d_driver, &TypstDriver::previewReady, this, &MainWindow::updatePreview);
    connect(d_driver, &TypstDriver::compilationFailed, this, &MainWindow::compilationFailed);

    d_previewDocument = new QPdfDocument();

    setupUI();
    setupActions();
    setupStatusBar();

    readSettings();
    setCurrentFile(QString());
    cursorPositionChanged();

    if (!d_driver->compilerFound()) {
        QTimer::singleShot(0, this, [this]() {
            QMessageBox::warning(
                this,
                QCoreApplication::applicationName(),
                tr("The typst compiler was not found, and therefore previews and export will not work.\nPlease make sure it is installed and in your system path."));
        });
    }
}

void MainWindow::setupUI()
{
    setWindowTitle(tr("Katvan"));
    setWindowIcon(QIcon(":/assets/katvan.svg"));

    d_editor = new Editor();
    connect(d_editor, &Editor::contentModified, d_driver, &TypstDriver::updatePreview);
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

    d_pdfPreview = new QPdfView();
    d_pdfPreview->setDocument(d_previewDocument);
    d_pdfPreview->setPageMode(QPdfView::PageMode::MultiPage);
    d_pdfPreview->setZoomMode(QPdfView::ZoomMode::FitToWidth);

    QFont monospaceFont { "Monospace" };
    monospaceFont.setStyleHint(QFont::Monospace);

    d_compilerOutput = new QPlainTextEdit();
    d_compilerOutput->setReadOnly(true);
    d_compilerOutput->setFont(monospaceFont);

    setDockOptions(QMainWindow::AnimatedDocks);

    d_previewDock = new QDockWidget(tr("Preview"));
    d_previewDock->setObjectName("previewDockPanel");
    d_previewDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    d_previewDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    d_previewDock->setWidget(d_pdfPreview);
    addDockWidget(Qt::RightDockWidgetArea, d_previewDock);

    d_compilerOutputDock = new QDockWidget(tr("Compiler Output"));
    d_compilerOutputDock->setObjectName("compilerOutputDockPanel");
    d_compilerOutputDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
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
    exportPdfAction->setEnabled(d_driver->compilerFound());

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

    QAction* findAction = editMenu->addAction(tr("&Find..."), d_searchBar, &SearchBar::ensureVisible);
    findAction->setIcon(QIcon::fromTheme("edit-find", QIcon(":/icons/edit-find.svg")));
    findAction->setShortcut(QKeySequence::Find);

    QAction* gotoLineAction = editMenu->addAction(tr("&Go to line..."), this, &MainWindow::goToLine);
    gotoLineAction->setShortcut(Qt::CTRL | Qt::Key_G);

    /*
     * View Menu
     */
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    viewMenu->addAction(tr("Editor &Font..."), this, &MainWindow::changeEditorFont);

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

    d_cursorPosButton = buildStatusBarButton();
    connect(d_cursorPosButton, &QToolButton::clicked, this, &MainWindow::goToLine);

    statusBar()->addPermanentWidget(d_cursorPosButton);

    d_spellingButton = buildStatusBarButton();
    d_spellingButton->setToolTip(tr("Spell checking dictionary"));
    d_spellingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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

    if (settings.contains(SETTING_EDITOR_FONT)) {
        QFont editorFont = settings.value(SETTING_EDITOR_FONT).value<QFont>();
        d_editor->setFont(editorFont);
    }

    d_recentFiles->restoreRecents(settings);
    restoreSpellingDictionary(settings);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(SETTING_MAIN_WINDOW_STATE, saveState());
    settings.setValue(SETTING_MAIN_WINDOW_GEOMETRY, saveGeometry());
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
    d_previewDocument->close();

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
    }
    else {
        QFileInfo info(fileName);
        d_currentFileName = info.exists() ? info.canonicalFilePath() : fileName;
        d_currentFileShortName = info.fileName();

        d_recentFiles->addRecent(d_currentFileName);
    }

    d_editor->document()->setModified(false);
    setWindowModified(false);

    setWindowTitle(QString("%1[*] - %2").arg(
        QCoreApplication::applicationName(),
        d_currentFileShortName));

    d_driver->resetInputFile(fileName);
    d_searchBar->hide();
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

void MainWindow::newFile()
{
    if (!maybeSave()) {
        return;
    }
    d_editor->clear();
    d_previewDocument->close();
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
    if (d_driver->status() == TypstDriver::Status::FAILED) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("The document %1 has errors.\nTo export the document, please correct them.").arg(d_currentFileShortName));

        return;
    }
    else if (d_driver->status() != TypstDriver::Status::SUCCESS) {
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
    if (QFile::exists(targetFileName)) {
        QFile::remove(targetFileName);
    }

    QFile sourceFile(d_driver->pdfFilePath());
    bool ok = sourceFile.copy(targetFileName);
    if (!ok) {
        QMessageBox::critical(
            this,
            QCoreApplication::applicationName(),
            tr("Failing writing file %1: %2").arg(targetFileName, sourceFile.errorString()));
    }
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
        d_editor->goToBlock(line - 1);
    }
}

void MainWindow::changeEditorFont()
{
    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, d_editor->font(), this);

    if (!ok) {
        return;
    }

    d_editor->setFont(font);

    QSettings settings;
    settings.setValue(SETTING_EDITOR_FONT, font);
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
    if (selectedDictName != currentDict) {
        d_editor->forceRehighlighting();
    }

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

void MainWindow::updatePreview(const QString& pdfFile)
{
    int origY = d_pdfPreview->verticalScrollBar()->value();

    QPdfDocument::Error rc = d_previewDocument->load(pdfFile);
    if (rc != QPdfDocument::Error::None) {
        QString err = QVariant::fromValue(rc).toString();
        QMessageBox::warning(
            this,
            QCoreApplication::applicationName(),
            tr("Failed loading preview: %1").arg(err));

        return;
    }

    d_compilerOutput->clear();
    d_pdfPreview->verticalScrollBar()->setValue(origY);
}

void MainWindow::compilationFailed(const QString& output)
{
    d_compilerOutput->setPlainText(output);
    d_compilerOutputDock->show();
}

}
