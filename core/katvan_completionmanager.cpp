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
#include "katvan_completionmanager.h"
#include "katvan_text_utils.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>
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
 * {"apply":"@preview/wordometer:0.1.4","detail":"Word counts and document statistics.","kind":"package","label":"@preview/wordometer:0.1.4"}
 */

namespace katvan {

Q_GLOBAL_STATIC(QRegularExpression, APPLY_PLACEHOLDER_REGEX, QStringLiteral("\\${(\\S*?)}"))

enum CompletionModelRoles {
    COMPLETION_DETAIL_ROLE = Qt::UserRole + 1,
    COMPLETION_APPLY_ROLE,
};

constexpr int ICON_MARGIN_PX = 3;

CompletionListModel::CompletionListModel(QObject* parent)
    : QAbstractListModel(parent)
    , d_symbolFont(utils::SYMBOL_FONT_FAMILY)
{
}

void CompletionListModel::setSuggestions(const QList<QJsonObject>& suggestions)
{
    beginResetModel();
    d_suggestions = suggestions;
    endResetModel();
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
        return obj["label"].toString();
    }
    else if (role == Qt::DecorationRole) {
        if (obj["kind"].isString()) {
            QString kind = obj["kind"].toString();
            if (kind == "syntax") {
                return QIcon::fromTheme("code-context");
            }
            else if (kind == "func") {
                return QIcon::fromTheme("code-function");
            }
            else if (kind == "type") {
                return QIcon::fromTheme("code-typedef");
            }
            else if (kind == "param" || kind == "constant") {
                return QIcon::fromTheme("code-variable");
            }
            else if (kind == "package") {
                return QIcon::fromTheme("package");
            }
        }
        else if (obj["kind"].isObject()) {
            QJsonObject kind = obj["kind"].toObject();
            QString symbol = kind["symbol"].toString();
            if (!symbol.isEmpty()) {
                return utils::fontIcon(utils::firstCodepointOf(symbol), d_symbolFont);
            }
        }
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

CompletionSuggestionDelegate::CompletionSuggestionDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
    , d_labelFont(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    d_labelFont.setWeight(QFont::DemiBold);
}

static void createLayout(QTextLayout& layout, const QString& text, const QFont& font, bool forPainting)
{
    static constexpr int MAX_WIDTH = 500;

    layout.setText(text);
    layout.setFont(font);
    if (forPainting) {
        layout.setCacheEnabled(true);
    }

    qreal height = 0;

    layout.beginLayout();
    while (true) {
        QTextLine tl = layout.createLine();
        if (!tl.isValid()) {
            break;
        }
        tl.setLineWidth(MAX_WIDTH);
        tl.setPosition(QPointF(0, height));
        height += tl.height();
    }
    layout.endLayout();
}

void CompletionSuggestionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QString label = index.data(Qt::DisplayRole).toString();
    QString detail = index.data(COMPLETION_DETAIL_ROLE).toString();
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));

    QTextLayout detailLayout;
    createLayout(detailLayout, detail, option.font, true);

    painter->save();
    painter->setClipRect(option.rect);

    if (option.state & QStyle::State_Selected) {
        QBrush bgBrush = option.palette.brush(QPalette::Normal, QPalette::Highlight);
        painter->fillRect(option.rect, bgBrush);
    }

    int xOffset = option.rect.x() + option.decorationSize.width() + ICON_MARGIN_PX;
    QFontMetrics labelMetrics { d_labelFont };

    QRect labelRect {
        QPoint { xOffset, option.rect.top() },
        QSize { option.rect.width(), labelMetrics.height() }
    };
    QRect detailRect {
        QPoint { xOffset, labelRect.bottom() },
        QSize { labelRect.width(), option.rect.height() - labelRect.height() }
    };

    painter->setPen(option.palette.text().color());

    if (!icon.isNull()) {
        QRect iconRect { option.rect.topLeft(), option.decorationSize };

        QPixmap pixmap = icon.pixmap(option.decorationSize, (option.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
        painter->drawPixmap(iconRect, pixmap);
    }

    painter->setFont(d_labelFont);
    painter->drawText(labelRect, label);

    detailLayout.draw(painter, detailRect.topLeft());

    painter->restore();
}

QSize CompletionSuggestionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QString label = index.data(Qt::DisplayRole).toString();
    QString detail = index.data(COMPLETION_DETAIL_ROLE).toString();

    QFontMetrics labelMetrics { d_labelFont };

    QTextLayout layout;
    createLayout(layout, detail, option.font, false);

    int width = labelMetrics.horizontalAdvance(label);
    int height = labelMetrics.height();

    for (int i = 0; i < layout.lineCount(); i++) {
        QTextLine tl = layout.lineAt(i);
        width = qMax(width, qCeil(tl.naturalTextWidth()));
        height += qCeil(tl.height());
    }

    width += (option.decorationSize.width() + ICON_MARGIN_PX);
    height = qMax(height, option.decorationSize.height());

    return QSize(width, height);
}

CompletionManager::CompletionManager(QTextEdit* editor)
    : QObject(editor)
    , d_editor(editor)
    , d_implictCompletionAllowed(false)
    , d_completionsRequested(false)
{
    d_model = new CompletionListModel(this);

    d_completer = new QCompleter(this);
    d_completer->setWidget(d_editor);
    d_completer->setCompletionMode(QCompleter::PopupCompletion);
    d_completer->setCaseSensitivity(Qt::CaseInsensitive);

    d_completer->setModel(d_model);
    d_completer->popup()->setItemDelegate(new CompletionSuggestionDelegate(this));

    connect(d_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, &CompletionManager::suggestionSelected);
}

bool CompletionManager::isActive() const
{
    return d_completer->popup()->isVisible();
}

void CompletionManager::startExplicitCompletion()
{
    startCompletion(false);
}

void CompletionManager::startImplicitCompletion()
{
    startCompletion(true);
}

void CompletionManager::startCompletion(bool implicit)
{
    if (implicit && !isImplictCompletionAllowed()) {
        return;
    }

    if (isActive() || d_completionsRequested) {
        return;
    }

    d_completionsRequested = true;
    QTextCursor cursor = d_editor->textCursor();
    Q_EMIT completionsRequested(cursor.blockNumber(), cursor.positionInBlock(), implicit);
}

void CompletionManager::close()
{
    d_completer->popup()->hide();
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

    const QJsonArray completionsArray = doc.array();

    QList<QJsonObject> suggestions;
    for (const QJsonValue& value : completionsArray) {
        suggestions.append(value.toObject());
    }
    d_model->setSuggestions(suggestions);

    QTextBlock block = d_editor->document()->findBlockByNumber(line);
    d_suggestionsStartPosition = block.position() + column;

    updateCompletionPrefix(true);
}

void CompletionManager::updateCompletionPrefix(bool force)
{
    QTextCursor currentCursor = d_editor->textCursor();
    if (currentCursor.position() < d_suggestionsStartPosition) {
        d_completer->popup()->hide();
        return;
    }

    QTextCursor cursor { d_editor->document() };
    cursor.setPosition(d_suggestionsStartPosition);
    cursor.setPosition(currentCursor.position(), QTextCursor::KeepAnchor);

    QString prefix = cursor.selectedText();
    if (d_completer->completionPrefix() != prefix || force) {
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
    if (snippet.isEmpty()) {
        return;
    }

    QTextCursor cursor { d_editor->document() };
    cursor.setPosition(d_suggestionsStartPosition);
    cursor.setPosition(d_editor->textCursor().position(), QTextCursor::KeepAnchor);

    int lastMatchEnd = 0;
    int selectionStart = -1, selectionEnd = -1;

    cursor.beginEditBlock();

    auto it = APPLY_PLACEHOLDER_REGEX->globalMatch(snippet);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();

        // First fill all non-placeholder text after the previous placeholder
        cursor.insertText(snippet.sliced(lastMatchEnd, m.capturedStart(0) - lastMatchEnd));

        if (selectionStart == -1) {
            selectionStart = cursor.position();
        }

        QString placeholderName = m.captured(1);
        if (!placeholderName.isEmpty()) {
            cursor.insertText(placeholderName);
        }

        if (selectionEnd == -1) {
            selectionEnd = cursor.position();
        }
        lastMatchEnd = m.capturedEnd(0);
    }

    cursor.insertText(snippet.sliced(lastMatchEnd));
    cursor.endEditBlock();

    if (selectionStart >= 0) {
        cursor.setPosition(selectionStart);
        cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    }
    d_editor->setTextCursor(cursor);

    // Making a completion can trigger a new completion
    if (selectionStart < 0 || selectionStart == selectionEnd) {
        startImplicitCompletion();
    }
}

}

#include "moc_katvan_completionmanager.cpp"
