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

#include <QActionGroup>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextEdit>
#include <QToolButton>
#include <QValidator>

namespace katvan {

class RegexFormatValidator : public QValidator
{
public:
    RegexFormatValidator(QAction* regexModeAction, QObject* parent = nullptr)
        : QValidator(parent)
        , d_regexModeAction(regexModeAction)
    {
    }

    QValidator::State validate(QString& input, int&) const override
    {
        if (!d_regexModeAction->isChecked()) {
            return QValidator::Acceptable;
        }

        QRegularExpression regex(input, QRegularExpression::UseUnicodePropertiesOption);
        if (regex.isValid()) {
            return QValidator::Acceptable;
        }
        return QValidator::Intermediate;
    }

private:
    QAction* d_regexModeAction;
};

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
    QMenu* settingsMenu = new QMenu();
    settingsMenu->addSection(tr("Find Type"));

    QActionGroup* matchTypeGroup = new QActionGroup(this);
    connect(matchTypeGroup, &QActionGroup::triggered, this, &SearchBar::checkTermIsValid);

    d_normalMatchType = settingsMenu->addAction(tr("Normal"));
    d_normalMatchType->setActionGroup(matchTypeGroup);
    d_normalMatchType->setCheckable(true);
    d_normalMatchType->setChecked(true);

    d_regexMatchType = settingsMenu->addAction(tr("Regular Expression"));
    d_regexMatchType->setActionGroup(matchTypeGroup);
    d_regexMatchType->setCheckable(true);

    d_wholeWordsMatchType = settingsMenu->addAction(tr("Whole Words"));
    d_wholeWordsMatchType->setActionGroup(matchTypeGroup);
    d_wholeWordsMatchType->setCheckable(true);

    settingsMenu->addSeparator();

    d_matchCase = settingsMenu->addAction(tr("Match Case"));
    d_matchCase->setCheckable(true);

    d_searchTerm = new QLineEdit();
    d_searchTerm->setValidator(new RegexFormatValidator(d_regexMatchType, this));
    connect(d_searchTerm, &QLineEdit::returnPressed, this, &SearchBar::findNext);
    connect(d_searchTerm, &QLineEdit::textEdited, this, &SearchBar::checkTermIsValid);

    QToolButton* findNextButton = new QToolButton();
    findNextButton->setIcon(QIcon::fromTheme("go-down", QIcon(":/icons/go-down.svg")));
    findNextButton->setShortcut(QKeySequence::FindNext);
    findNextButton->setToolTip(processToolTip(tr("Go to next match (%1)"), QKeySequence::FindNext));
    connect(findNextButton, &QToolButton::clicked, this, &SearchBar::findNext);

    QToolButton* findPrevButton = new QToolButton();
    findPrevButton->setIcon(QIcon::fromTheme("go-up", QIcon(":/icons/go-up.svg")));
    findPrevButton->setShortcut(QKeySequence::FindPrevious);
    findPrevButton->setToolTip(processToolTip(tr("Go to previous match (%1)"), QKeySequence::FindPrevious));
    connect(findPrevButton, &QToolButton::clicked, this, &SearchBar::findPrevious);

    QToolButton* settingsButton = new QToolButton();
    settingsButton->setMenu(settingsMenu);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    settingsButton->setIcon(QIcon::fromTheme("settings-configure", QIcon(":/icons/settings-configure.svg")));
    settingsButton->setToolTip(tr("Find settings"));

    QToolButton* closeButton = new QToolButton();
    closeButton->setIcon(QIcon::fromTheme("window-close", QIcon(":/icons/window-close.svg")));
    closeButton->setToolTip(tr("Close search bar"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::hide);

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
    layout->addWidget(settingsButton);
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

void SearchBar::checkTermIsValid()
{
    if (!d_searchTerm->hasAcceptableInput()) {
        d_searchTerm->setStyleSheet("background-color: #e59596");
        d_searchTerm->setToolTip(tr("Invalid regular expression entered"));
    }
    else {
        d_searchTerm->setStyleSheet(QString());
        d_searchTerm->setToolTip(QString());
    }
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

    QRegularExpression regex;
    if (d_regexMatchType->isChecked()) {
        regex.setPattern(searchTerm);
        regex.setPatternOptions(QRegularExpression::UseUnicodePropertiesOption);
    }
    else {
        regex.setPattern(QRegularExpression::escape(searchTerm));
    }

    QTextDocument::FindFlags flags;
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }
    if (d_wholeWordsMatchType->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    if (d_matchCase->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }

    QTextDocument* document = d_editor->document();
    QTextCursor cursor = d_editor->textCursor();

    QTextCursor found = document->find(regex, cursor, flags);
    if (!found.isNull()) {
        d_editor->setTextCursor(found);
        return;
    }

    // Maybe there is a match before the cursor
    QTextCursor edgeCursor(document);
    if (!forward) {
        edgeCursor.movePosition(QTextCursor::End);
    }

    found = document->find(regex, edgeCursor, flags);
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
