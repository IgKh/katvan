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
#include "katvan_compileroutput.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QRegularExpression>

// MAIN:89:13: error: unknown variable: abc
Q_GLOBAL_STATIC(QRegularExpression, DIAGNOSTIC_MESSAGE_REGEX,
                QStringLiteral("^MAIN:(\\d+):(\\d+):\\s+(.+)$"))

namespace katvan {

CompilerOutput::CompilerOutput(QWidget* parent)
    : QPlainTextEdit(parent)
    , d_cursorOverLink(false)
{
    setReadOnly(true);
    setMouseTracking(true);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void CompilerOutput::setInputFileShortName(const QString& fileName)
{
    d_fileName = fileName;
}

void CompilerOutput::setOutputLines(const QStringList& output)
{
    clear();
    QTextCursor cursor(document());

    QTextCharFormat origFormat = cursor.charFormat();

    for (const auto& line : output) {
        QRegularExpressionMatch m = DIAGNOSTIC_MESSAGE_REGEX->match(line);
        if (!m.hasMatch()) {
            cursor.insertText(line);
        }
        else {
            QString row = m.captured(1);
            QString col = m.captured(2);

            QString locator = QStringLiteral("%1:%2:%3").arg(
                d_fileName.isEmpty() ? "Untitled" : d_fileName,
                row,
                col);

            QTextCharFormat linkFormat = origFormat;
            linkFormat.setAnchor(true);
            linkFormat.setAnchorHref(QStringLiteral("%1:%2").arg(row, col));
            linkFormat.setFontUnderline(true);

            cursor.insertText(locator, linkFormat);
            cursor.insertText(": " + m.captured(3), origFormat);
        }
        cursor.insertBlock();
    }
}

void CompilerOutput::mouseMoveEvent(QMouseEvent* event)
{
    QPlainTextEdit::mouseMoveEvent(event);

    QString anchor = anchorAt(event->pos());
    if (!anchor.isEmpty() && !d_cursorOverLink) {
        d_cursorOverLink = true;
        qGuiApp->setOverrideCursor(Qt::PointingHandCursor);
    }
    else if (anchor.isEmpty() && d_cursorOverLink) {
        d_cursorOverLink = false;
        qGuiApp->restoreOverrideCursor();
    }
}

void CompilerOutput::mouseReleaseEvent(QMouseEvent* event)
{
    QString anchor = anchorAt(event->pos());
    if ((event->button() & Qt::LeftButton) && !anchor.isEmpty()) {
        QStringList parts = anchor.split(QLatin1Char(':'));
        Q_ASSERT(parts.size() == 2);

        int block = parts[0].toInt() - 1;
        int offset = parts[1].toInt();
        Q_EMIT goToPosition(block, offset);
    }

    QPlainTextEdit::mouseReleaseEvent(event);
}

}
