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

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QLabel;
class QPdfDocument;
class QPdfView;
class QPlainTextEdit;
class QToolButton;
QT_END_NAMESPACE

namespace katvan
{

class Editor;
class TypstDriver;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

    void loadFile(const QString& fileName);

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void exportPdf();
    void changeEditorFont();
    void showTypstDocs();
    void showAbout();

    void cursorPositionChanged();
    void toggleCursorMovementStyle();
    void updatePreview(const QString& pdfFile);
    void compilationFailed(const QString& output);

private:
    void setupUI();
    void setupActions();
    void setupStatusBar();
    void readSettings();
    void saveSettings();

    bool maybeSave();
    void setCurrentFile(const QString& fileName);

    void closeEvent(QCloseEvent* event) override;

    QString d_currentFileName;
    QString d_currentFileShortName;

    TypstDriver* d_driver;
    QPdfDocument* d_previewDocument;

    Editor* d_editor;
    QPdfView* d_pdfPreview;
    QPlainTextEdit* d_compilerOutput;

    QLabel* d_cursorPosLabel;
    QToolButton* d_cursorStyleButton;

    QDockWidget* d_previewDock;
    QDockWidget* d_compilerOutputDock;
};

}
