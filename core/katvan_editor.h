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

#include "katvan_editortheme.h"
#include "katvan_editorsettings.h"

#include "typstdriver_logger.h"

#include <QPointer>
#include <QTextEdit>

#include <optional>

QT_BEGIN_NAMESPACE
class QHelpEvent;
class QMenu;
QT_END_NAMESPACE

namespace katvan {

class CodeModel;
class CompletionManager;
class Document;
class Highlighter;

class Editor : public QTextEdit
{
    Q_OBJECT

    friend class LineNumberGutter;

public:
    Editor(Document* doc, QWidget* parent = nullptr);

    CompletionManager* completionManager() const { return d_completionManager; }

    void applySettings(const EditorSettings& settings);
    void updateEditorTheme();
    void setSourceDiagnostics(QList<typstdriver::Diagnostic> diagnostics);

    QMenu* createInsertMenu();

public slots:
    void toggleTextBlockDirection();
    void setTextBlockDirection(Qt::LayoutDirection dir);
    void goToBlock(int blockNum, int charOffset);
    void forceRehighlighting();
    void checkForModelines();
    void showToolTip(QPoint windowPos, const QString& text);

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

    QTextCursor cursorAt(int blockNum, int charOffset) const;
    QString predefinedTooltipAtPosition(int position) const;
    std::tuple<QTextBlock, QTextBlock, bool> selectedBlockRange() const;
    QString getIndentString(QTextCursor cursor) const;
    void handleNewLine();
    void handleClosingBracket(const QString& bracket);
    void unindentBlock(QTextCursor blockStartCursor, QTextCursor notAfter = QTextCursor());

    void handleToolTipEvent(QHelpEvent* event);

    void applyEffectiveSettings();

    QTextEdit::ExtraSelection makeBracketHighlight(int pos);

    int lineNumberGutterWidth();
    void lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event);

private slots:
    void popupInsertMenu();
    void spellingSuggestionsReady(const QString& word, int position, const QStringList& suggestions);

    void updateLineNumberGutterWidth();
    void updateLineNumberGutters();
    void updateExtraSelections();

signals:
    void toolTipRequested(int blockNumber, int charOffset, QPoint widgetPos);

private:
    QWidget* d_leftLineNumberGutter;
    QWidget* d_rightLineNumberGutter;

    Highlighter* d_highlighter;
    CodeModel* d_codeModel;
    CompletionManager* d_completionManager;

    EditorSettings d_appSettings;
    EditorSettings d_fileMode;
    EditorSettings d_effectiveSettings;
    EditorTheme d_theme;

    QList<typstdriver::Diagnostic> d_sourceDiagnostics;
    QPointer<QMenu> d_contextMenu;
    std::optional<Qt::LayoutDirection> d_pendingDirectionChange;
    QString d_pendingSuggestionsWord;
    int d_pendingSuggestionsPosition;
    std::optional<QPoint> d_pendingTooltipPos;
};

}
