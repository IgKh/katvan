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
#include "katvan_utils.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextBlock>
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
    , d_searchRangeStart(editor->document())
    , d_searchRangeEnd(editor->document())
    , d_resultSettingInProgress(false)
{
    setupUI();

    connect(editor, &QTextEdit::selectionChanged, this, &SearchBar::resetSearchRange);
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

    d_findInSelection = settingsMenu->addAction(tr("Find in Selection"));
    d_findInSelection->setCheckable(true);
    connect(d_findInSelection, &QAction::toggled, this, &SearchBar::resetSearchRange);

    d_searchTerm = new QLineEdit();
    d_searchTerm->setValidator(new RegexFormatValidator(d_regexMatchType, this));
    connect(d_searchTerm, &QLineEdit::returnPressed, this, &SearchBar::findNext);
    connect(d_searchTerm, &QLineEdit::textEdited, this, &SearchBar::checkTermIsValid);

    d_replaceWith = new QLineEdit();
    connect(d_replaceWith, &QLineEdit::returnPressed, this, &SearchBar::replaceNext);

    QToolButton* findNextButton = new QToolButton();
    findNextButton->setIcon(utils::themeIcon("go-down", "chevron.right"));
    findNextButton->setShortcut(QKeySequence::FindNext);
    findNextButton->setToolTip(processToolTip(tr("Go to next match (%1)"), QKeySequence::FindNext));
    connect(findNextButton, &QToolButton::clicked, this, &SearchBar::findNext);

    QToolButton* findPrevButton = new QToolButton();
    findPrevButton->setIcon(utils::themeIcon("go-up", "chevron.left"));
    findPrevButton->setShortcut(QKeySequence::FindPrevious);
    findPrevButton->setToolTip(processToolTip(tr("Go to previous match (%1)"), QKeySequence::FindPrevious));
    connect(findPrevButton, &QToolButton::clicked, this, &SearchBar::findPrevious);

    QToolButton* settingsButton = new QToolButton();
    settingsButton->setMenu(settingsMenu);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    settingsButton->setIcon(utils::themeIcon("settings-configure", "gear"));
    settingsButton->setToolTip(tr("Find settings"));

    QToolButton* replaceButton = new QToolButton();
    replaceButton->setText(tr("Replace"));
    replaceButton->setToolTip(tr("Replace next match"));
    connect(replaceButton, &QToolButton::clicked, this, &SearchBar::replaceNext);

    QToolButton* replaceAllButton = new QToolButton();
    replaceAllButton->setText(tr("Replace All"));
    replaceAllButton->setToolTip(tr("Replace all matches"));
    connect(replaceAllButton, &QToolButton::clicked, this, &SearchBar::replaceAll);

    QToolButton* closeButton = new QToolButton();
    closeButton->setIcon(utils::themeIcon("window-close", "xmark.circle"));
    closeButton->setToolTip(tr("Close search bar"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::hide);

    QHBoxLayout* findLayout = new QHBoxLayout();
    findLayout->addWidget(d_searchTerm, 1);
#if defined(Q_OS_MACOS)
    findLayout->addWidget(findPrevButton);
    findLayout->addWidget(findNextButton);
#else
    findLayout->addWidget(findNextButton);
    findLayout->addWidget(findPrevButton);
#endif
    findLayout->addWidget(settingsButton);

    QHBoxLayout* replaceLayout = new QHBoxLayout();
    replaceLayout->addWidget(d_replaceWith, 1);
    replaceLayout->addWidget(replaceButton);
    replaceLayout->addWidget(replaceAllButton);

    QGridLayout* layout = new QGridLayout(this);

    QMargins margins = layout->contentsMargins();
    margins.setTop(0);
    layout->setContentsMargins(margins);

    layout->addWidget(new QLabel(tr("Find:")), 0, 0);
    layout->addLayout(findLayout, 0, 1);
    layout->addWidget(closeButton, 0, 2);
    layout->addWidget(new QLabel(tr("Replace:")), 1, 0);
    layout->addLayout(replaceLayout, 1, 1);
    layout->setColumnStretch(1, 1);
}

static void setLayoutItemVisible(QLayoutItem* item, bool visible)
{
    if (item == nullptr) {
        return;
    }

    if (item->widget() != nullptr) {
        item->widget()->setVisible(visible);
    }
    else if (item->layout() != nullptr) {
        QLayout* layout = item->layout();
        for (int i = 0; i < layout->count(); i++) {
            setLayoutItemVisible(layout->itemAt(i), visible);
        }
    }
}

void SearchBar::setReplaceLineVisible(bool visible)
{
    QGridLayout* topLayout = qobject_cast<QGridLayout *>(layout());
    for (int i = 0; i < topLayout->columnCount(); i++) {
        setLayoutItemVisible(topLayout->itemAtPosition(1, i), visible);
    }
}

void SearchBar::ensureVisibleImpl()
{
    show();

    QTextCursor cursor = d_editor->textCursor();
    if (cursor.hasSelection()) {
        QTextBlock selectionBeginBlock = d_editor->document()->findBlock(cursor.selectionStart());
        QTextBlock selectionEndBlock = d_editor->document()->findBlock(cursor.selectionEnd());

        if (selectionBeginBlock == selectionEndBlock) {
            d_searchTerm->setText(cursor.selectedText().trimmed());
        }
    }

    d_searchTerm->setFocus();
    d_searchTerm->selectAll();
    d_lastMatch = QTextCursor();
}

void SearchBar::ensureFindVisible()
{
    if (!isVisible()) {
        setReplaceLineVisible(false);
    }
    ensureVisibleImpl();
}

void SearchBar::ensureReplaceVisible()
{
    ensureVisibleImpl();
    setReplaceLineVisible(true);
}

void SearchBar::resetSearchRange()
{
    if (d_resultSettingInProgress) {
        return;
    }

    QTextCursor cursor = d_editor->textCursor();
    if (cursor.hasSelection() && d_findInSelection->isChecked()) {
        d_searchRangeStart.setPosition(cursor.selectionStart());
        d_searchRangeEnd.setPosition(cursor.selectionEnd());
    }
    else {
        d_searchRangeStart.movePosition(QTextCursor::Start);
        d_searchRangeEnd.movePosition(QTextCursor::End);
    }
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
    d_lastMatch = QTextCursor();
}

void SearchBar::findNext()
{
    find(true);
}

void SearchBar::findPrevious()
{
    find(false);
}

QTextCursor SearchBar::findImpl(const QString& searchTerm, bool forward)
{
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

    auto isValidResult = [this](const QTextCursor& result) {
        return !result.isNull() && result >= d_searchRangeStart && result <= d_searchRangeEnd;
    };

    QTextCursor found = document->find(regex, cursor, flags);
    if (isValidResult(found)) {
        return found;
    }

    // Maybe there is a match before the cursor
    QTextCursor edgeCursor = forward ? d_searchRangeStart : d_searchRangeEnd;

    found = document->find(regex, edgeCursor, flags);
    if (isValidResult(found)) {
        return found;
    }

    // There really are no matches
    return QTextCursor();
}

void SearchBar::find(bool forward)
{
    d_lastMatch = QTextCursor();

    QString searchTerm = d_searchTerm->text();
    if (searchTerm.isEmpty()) {
        return;
    }

    QTextCursor found = findImpl(searchTerm, forward);
    if (!found.isNull()) {
        d_resultSettingInProgress = true;
        d_lastMatch = found;
        d_editor->setTextCursor(found);
        d_resultSettingInProgress = false;
    }
    else {
        QMessageBox::warning(window(), QCoreApplication::applicationName(), tr("No matches found"));
    }
}

void SearchBar::replaceNext()
{
    if (!d_lastMatch.isNull()) {
        d_lastMatch.beginEditBlock();
        d_lastMatch.removeSelectedText();
        d_lastMatch.insertText(d_replaceWith->text());
        d_lastMatch.endEditBlock();
    }
    findNext();
}

void SearchBar::replaceAll()
{
    QString searchTerm = d_searchTerm->text();
    if (searchTerm.isEmpty()) {
        return;
    }

    qsizetype replacementCount = 0;

    QTextCursor cursor(d_editor->document());
    cursor.beginEditBlock();

    QTextCursor result = findImpl(searchTerm, true);
    while (!result.isNull()) {
        cursor.setPosition(result.selectionStart());
        cursor.setPosition(result.selectionEnd(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(d_replaceWith->text());

        replacementCount++;
        result = findImpl(searchTerm, true);
    }

    cursor.endEditBlock();

    QMessageBox::information(
        window(),
        QCoreApplication::applicationName(),
        tr("%n replacements were performed", "", replacementCount));
}

void SearchBar::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Cancel)) {
        hide();
    }
    QWidget::keyPressEvent(event);
}

}
