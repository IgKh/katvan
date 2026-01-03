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
#pragma once

#include <QList>
#include <QTextCursor>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QRegularExpression;
QT_END_NAMESPACE

namespace katvan {

class Editor;

class SearchBar : public QWidget
{
    Q_OBJECT

public:
    SearchBar(Editor* editor, QWidget* parent = nullptr);

public slots:
    void ensureFindVisible();
    void ensureReplaceVisible();
    void resetSearchRange();

private slots:
    void updateTooltips();
    void checkTermIsValid();
    void findNext();
    void findPrevious();
    void replaceNext();
    void replaceAll();

private:
    void setupUI();
    void setReplaceLineVisible(bool visible);
    void ensureVisibleImpl();
    QTextCursor findFromCursor(QTextCursor cursor, const QRegularExpression& regex, bool forward);
    QTextCursor findImpl(const QString& searchTerm, bool forward);
    void find(bool forward);
    QString getReplacementText();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    Editor* d_editor;

    QLineEdit* d_searchTerm;
    QLineEdit* d_replaceWith;

    QAction* d_normalMatchType;
    QAction* d_regexMatchType;
    QAction* d_wholeWordsMatchType;
    QAction* d_matchCase;
    QAction* d_findInSelection;

    QTextCursor d_searchRangeStart;
    QTextCursor d_searchRangeEnd;
    bool d_resultSettingInProgress;

    QTextCursor d_lastMatch;
    QStringList d_lastMatchCaptures;
};

}
