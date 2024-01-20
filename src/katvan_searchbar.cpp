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
#include "katvan_searchbar.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>

namespace katvan {

static void setOptionalIcon(QPushButton* button, const QString& iconName, const QString& fallbackText)
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        button->setIcon(icon);
    }
    else {
        button->setText(fallbackText);
    }
}

static QString processToolTip(const QString& toolTipTemplate, const QKeySequence& shortcut)
{
    QString shortcutStr = shortcut.toString(QKeySequence::NativeText);
    if (!shortcutStr.isEmpty()) {
        return toolTipTemplate.arg(shortcutStr);
    }

    // Strip any "(%1)" from the template
    QString result = toolTipTemplate;
    result.remove(QLatin1StringView("(%1)"));
    return result.trimmed();

}

SearchBar::SearchBar(QTextEdit* editor, QWidget* parent)
    : QWidget(parent)
    , d_editor(editor)
{
    setupUI();
}

void SearchBar::setupUI()
{
    d_searchTerm = new QLineEdit();
    connect(d_searchTerm, &QLineEdit::returnPressed, this, &SearchBar::findNext);

    QPushButton* findNextButton = new QPushButton();
    setOptionalIcon(findNextButton, QStringLiteral("go-down"), tr("Next"));
    findNextButton->setShortcut(QKeySequence::FindNext);
    findNextButton->setToolTip(processToolTip(tr("Go to next match (%1)"), QKeySequence::FindNext));
    connect(findNextButton, &QPushButton::clicked, this, &SearchBar::findNext);

    QPushButton* findPrevButton = new QPushButton();
    setOptionalIcon(findPrevButton, QStringLiteral("go-up"), tr("Prev"));
    findPrevButton->setShortcut(QKeySequence::FindPrevious);
    findPrevButton->setToolTip(processToolTip(tr("Go to previous match (%1)"), QKeySequence::FindPrevious));
    connect(findPrevButton, &QPushButton::clicked, this, &SearchBar::findPrevious);

    QPushButton* closeButton = new QPushButton();
    setOptionalIcon(closeButton, QStringLiteral("window-close"), tr("Close"));
    closeButton->setToolTip(tr("Close search bar"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(
        layout->contentsMargins().left(),
        0,
        layout->contentsMargins().right(),
        0);

    layout->addWidget(new QLabel(tr("Find:")));
    layout->addWidget(d_searchTerm, 1);
    layout->addWidget(findNextButton);
    layout->addWidget(findPrevButton);
    layout->addWidget(closeButton);
}

void SearchBar::ensureVisible()
{
    show();

    QTextCursor cursor = d_editor->textCursor();
    if (cursor.hasSelection()) {
        d_searchTerm->setText(cursor.selectedText());
    }

    d_searchTerm->setFocus();
    d_searchTerm->selectAll();
}

void SearchBar::findNext()
{
    find(true);
}

void SearchBar::findPrevious()
{
    find(false);
}

void SearchBar::find(bool forward)
{
    QString searchTerm = d_searchTerm->text();
    if (searchTerm.isEmpty()) {
        return;
    }

    QTextDocument::FindFlags flags;
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }

    QTextDocument* document = d_editor->document();
    QTextCursor cursor = d_editor->textCursor();

    QTextCursor found = document->find(searchTerm, cursor, flags);
    if (!found.isNull()) {
        d_editor->setTextCursor(found);
        return;
    }

    // Maybe there is a match before the cursor
    QTextCursor edgeCursor(document);
    if (!forward) {
        edgeCursor.movePosition(QTextCursor::End);
    }

    found = document->find(searchTerm, edgeCursor, flags);
    if (!found.isNull()) {
        d_editor->setTextCursor(found);
        return;
    }

    // There really are no matches
    QMessageBox::warning(window(), QCoreApplication::applicationName(), tr("No matches found"));
}

void SearchBar::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Cancel)) {
        hide();
    }
    QWidget::keyPressEvent(event);
}

}
