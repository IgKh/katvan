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
#include "katvan_codemodel.h"
#include "katvan_document.h"

#include <QScopedValueRollback>
#include <QTimer>

namespace katvan {

Document::Document(QObject* parent)
    : QTextDocument(parent)
    , d_suppressContentChangeHandling(false)
{
    d_codeModel = new CodeModel(this);

    connect(this, &QTextDocument::contentsChange, this, &Document::propagateDocumentEdit);

    d_debounceTimer = new QTimer(this);
    d_debounceTimer->setSingleShot(true);
    d_debounceTimer->setInterval(500);
    d_debounceTimer->callOnTimeout(this, &Document::contentModified);
}

QString Document::textForPreview() const
{
    return toPlainText() + QChar::LineFeed;
}

void Document::setDocumentText(const QString& text)
{
    QScopedValueRollback guard { d_suppressContentChangeHandling, true };
    setLayoutEnabled(false);
    setPlainText(text);
    setLayoutEnabled(true);

    Q_EMIT contentReset();
}

void Document::propagateDocumentEdit(int from, int charsRemoved, int charsAdded)
{
    if (d_suppressContentChangeHandling) {
        return;
    }

    QTextBlock block = findBlock(from);

    // In Qt's internal reckoning all blocks always end with ParagraphSeparator
    // or other such marker. Global character positions (like the from parameter
    // here) take this into account, however QTextBlock::text() strips them away.
    // To make sure that our and the Typst compiler's character positions are
    // always in sync, we explicitly add a line feed in each place where a
    // separator character would be in the document's internal buffer.
    QString blockText = block.text().sliced(from - block.position()) + QChar::LineFeed;

    QString editText;
    while (block.isValid() && charsAdded > 0) {
        if (blockText.size() > charsAdded) {
            editText += blockText.left(charsAdded);
            break;
        }
        else {
            editText += blockText;
            charsAdded -= blockText.size();
            block = block.next();
            blockText = block.text() + QChar::LineFeed;
        }
    }

    Q_EMIT contentEdited(from, from + charsRemoved, editText);
    d_debounceTimer->start();
}

}

#include "moc_katvan_document.cpp"
