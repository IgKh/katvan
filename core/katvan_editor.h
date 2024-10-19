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

#include "katvan_editortheme.h"
#include "katvan_editorsettings.h"

#include <QPointer>
#include <QTextEdit>

#include <optional>

QT_BEGIN_NAMESPACE
class QMenu;
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class Highlighter;
class CodeModel;

class Editor : public QTextEdit
{
    Q_OBJECT

    friend class LineNumberGutter;

public:
    Editor(QWidget* parent = nullptr);

    QString documentText() const;

    void applySettings(const EditorSettings& settings);
    void updateEditorTheme();

    QMenu* createInsertMenu();

public slots:
    void setDocumentText(const QString& text);
    void toggleTextBlockDirection();
    void setTextBlockDirection(Qt::LayoutDirection dir);
    void goToBlock(int blockNum, int charOffset);
    void forceRehighlighting();
    void checkForModelines();

protected:
    bool event(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    std::tuple<int, int> misspelledRangeAtCursor(QTextCursor cursor);
    QString misspelledWordAtCursor(QTextCursor cursor);

    void changeMisspelledWordAtPosition(int position, const QString& into);
    void insertMark(QChar mark);
    void insertSurroundingMarks(QString before, QString after);

    std::tuple<QTextBlock, QTextBlock, bool> selectedBlockRange() const;
    QString getIndentString(QTextCursor cursor) const;
    void handleNewLine();
    void handleClosingBracket(const QString& bracket);
    void unindentBlock(QTextCursor blockStartCursor, QTextCursor notAfter = QTextCursor());

    void applyEffectiveSettings();

    QTextEdit::ExtraSelection makeBracketHighlight(int pos);

    int lineNumberGutterWidth();
    void lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event);

private slots:
    void popupInsertMenu();
    void spellingSuggestionsReady(const QString& word, int position, const QStringList& suggestions);

    void propagateDocumentEdit(int from, int charsRemoved, int charsAdded);

    void updateLineNumberGutterWidth();
    void updateLineNumberGutters();
    void updateExtraSelections();

signals:
    void contentModified();
    void contentEdited(int from, int to, QString text);

private:
    QWidget* d_leftLineNumberGutter;
    QWidget* d_rightLineNumberGutter;

    QTimer* d_debounceTimer;
    Highlighter* d_highlighter;
    CodeModel* d_codeModel;

    EditorSettings d_appSettings;
    EditorSettings d_fileMode;
    EditorSettings d_effectiveSettings;
    EditorTheme d_theme;

    QPointer<QMenu> d_contextMenu;
    bool d_suppressContentChangeHandling;
    std::optional<Qt::LayoutDirection> d_pendingDirectionChange;
    QString d_pendingSuggestionsWord;
    int d_pendingSuggestionsPosition;
};

}
