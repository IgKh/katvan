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
#include "katvan_completionmanager.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>

/*
 * Samples:
 *
 * {"apply":"fallback: ${}","detail":"Whether to allow last resort font fallback when the primary font list\ncontains no match.","kind":"param","label":"fallback"}
 * {"apply":"(${params}) => ${output}","detail":"A custom function.","kind":"syntax","label":"function"}
 * {"apply":"highlight(${})","detail":"Highlights text with a background color.","kind":"func","label":"highlight"}
 * {"apply":null,"detail":null,"kind":{"symbol":"/"},"label":"slash"}
 * {"apply":null,"detail":"op(text: [dim], limits: false)","kind":"constant","label":"dim"}
 * {"apply":"${x}^${2:2}","detail":"Sets something in superscript.","kind":"syntax","label":"superscript"}
 */

namespace katvan {

enum CompletionModelRoles {
    COMPLETION_DETAIL_ROLE = Qt::UserRole + 1,
    COMPLETION_APPLY_ROLE,
};

CompletionListModel::CompletionListModel(const QList<QJsonObject>& suggestions, QObject* parent)
    : QAbstractListModel(parent)
    , d_suggestions(suggestions)
{
}

int CompletionListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return d_suggestions.size();
}

QVariant CompletionListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= d_suggestions.size()) {
        return QVariant();
    }

    const QJsonObject& obj = d_suggestions[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // TODO Should the EditRole should be something more complex?
        return obj["label"].toString();
    }
    else if (role == Qt::DecorationRole) {
        // TODO find a icon for each completion kind, and for symbols use a
        // font icon
        return QVariant();
    }
    else if (role == COMPLETION_DETAIL_ROLE) {
        return obj["detail"].toString();
    }
    else if (role == COMPLETION_APPLY_ROLE) {
        auto applyPattern = obj["apply"];
        if (applyPattern.isNull()) {
            return obj["label"].toString();
        }
        return applyPattern.toString();
    }
    return QVariant();
}

CompletionManager::CompletionManager(QTextEdit* editor)
    : QObject(editor)
    , d_editor(editor)
    , d_completionsRequested(false)
{
    d_completer = new QCompleter(this);
    d_completer->setWidget(d_editor);
    d_completer->setWrapAround(false);
    d_completer->setCompletionMode(QCompleter::PopupCompletion);
    d_completer->setCaseSensitivity(Qt::CaseInsensitive);

    // TODO need custom delegate to render detail string

    connect(d_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, &CompletionManager::suggestionSelected);
}

bool CompletionManager::isActive() const
{
    return d_completer->popup()->isVisible();
}

void CompletionManager::startCompletion()
{
    if (isActive() || d_completionsRequested) {
        return;
    }

    d_completionsRequested = true;
    QTextCursor cursor = d_editor->textCursor();
    Q_EMIT completionsRequested(cursor.blockNumber(), cursor.positionInBlock());
}

void CompletionManager::completionsReady(int line, int column, QByteArray completionsJson)
{
    d_completionsRequested = false;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(completionsJson, &error);

    if (doc.isNull()) {
        qWarning() << "Failed parsing completions:" << error.errorString();
        return;
    }

    QList<QJsonObject> suggestions;
    for (const QJsonValue& value : doc.array()) {
        suggestions.append(value.toObject());
    }
    d_completer->setModel(new CompletionListModel(suggestions));

    QTextBlock block = d_editor->document()->findBlockByNumber(line);

    d_suggestionsStartCursor = QTextCursor(d_editor->document());
    d_suggestionsStartCursor.setPosition(block.position() + column);

    updateCompletionPrefix();
}

void CompletionManager::updateCompletionPrefix()
{
    QTextCursor currentCursor = d_editor->textCursor();
    if (currentCursor.position() < d_suggestionsStartCursor.position()) {
        d_completer->popup()->hide();
        return;
    }

    QTextCursor cursor = d_suggestionsStartCursor;
    cursor.setPosition(currentCursor.position(), QTextCursor::KeepAnchor);

    QString prefix = cursor.selectedText();
    if (d_completer->completionPrefix() != prefix) {
        d_completer->setCompletionPrefix(prefix);
        d_completer->popup()->setCurrentIndex(d_completer->completionModel()->index(0, 0));
    }

    QRect popupRect = d_editor->cursorRect();
    popupRect.setWidth(
        d_completer->popup()->sizeHintForColumn(0) +
        d_completer->popup()->verticalScrollBar()->sizeHint().width());

    d_completer->complete(popupRect);
}

void CompletionManager::suggestionSelected(const QModelIndex& index)
{
    QString snippet = index.data(COMPLETION_APPLY_ROLE).toString();

    // TODO snippet can have placeholders (named and unamed). Need to decide
    // what to do with them, especially when there is more than one... Preedit
    // area won't work for this.

    d_suggestionsStartCursor.select(QTextCursor::WordUnderCursor);
    d_suggestionsStartCursor.insertText(snippet);
}

}
