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
class SpellChecker;

class Editor : public QTextEdit
{
    Q_OBJECT

    friend class LineNumberGutter;

public:
    Editor(QWidget* parent = nullptr);

    SpellChecker* spellChecker() const { return d_spellChecker; }

    void applySettings(const EditorSettings& settings);

    QMenu* createInsertMenu();

public slots:
    void toggleTextBlockDirection();
    void setTextBlockDirection(Qt::LayoutDirection dir);
    void goToBlock(int blockNum);
    void forceRehighlighting();
    void checkForModelines();

protected:
    bool event(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QString misspelledWordAtCursor(QTextCursor cursor);

    void changeWordAtPosition(int position, const QString& into);
    void insertMark(QChar mark);
    void insertSurroundingMarks(QString before, QString after);

    std::tuple<QTextBlock, QTextBlock, bool> selectedBlockRange() const;
    QString getIndentString(QTextCursor cursor) const;
    void unindentBlock(QTextCursor blockStartCursor, QTextCursor notAfter = QTextCursor());

    void applyEffectiveSettings();

    int lineNumberGutterWidth();
    QTextBlock getFirstVisibleBlock();
    void lineNumberGutterPaintEvent(QWidget* gutter, QPaintEvent* event);

private slots:
    void popupInsertMenu();
    void spellingSuggestionsReady(const QString& word, int position, const QStringList& suggestions);

    void updateLineNumberGutterWidth();
    void updateLineNumberGutters();

signals:
    void contentModified(const QString& text);

private:
    QWidget* d_leftLineNumberGutter;
    QWidget* d_rightLineNumberGutter;

    QTimer* d_debounceTimer;
    Highlighter* d_highlighter;
    SpellChecker* d_spellChecker;

    EditorSettings d_appSettings;
    EditorSettings d_fileMode;
    EditorSettings d_effectiveSettings;

    QPointer<QMenu> d_contextMenu;
    std::optional<Qt::LayoutDirection> d_pendingDirectionChange;
    QString d_pendingSuggestionsWord;
    int d_pendingSUggestionsPosition;
};

}
