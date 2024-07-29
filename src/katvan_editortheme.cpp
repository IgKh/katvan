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
#include "katvan_editortheme.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

namespace katvan {

using HiglightingMarkerKind = parsing::HiglightingMarker::Kind;

bool EditorTheme::isAppInDarkMode()
{
    QApplication* app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app) {
        // Happens in test suite
        return false;
    }

    if (app->styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        // Qt Vista style correctly reports dark mode setting on Windows 10,
        // but doesn't actually respect it - the default palette is light.
        if (app->style()->name() == "windowsvista") {
            return false;
        }
        return true;
    }

    // If the Qt platform integration doesn't support signaling a color scheme,
    // sniff it from the global palette.
    QPalette palette = app->palette();
    return palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

EditorTheme& EditorTheme::defaultTheme()
{
    if (isAppInDarkMode()) {
        static EditorTheme darkTheme(":/themes/default-dark.json");
        return darkTheme;
    }
    else {
        static EditorTheme lightTheme(":/themes/default-light.json");
        return lightTheme;
    }
}

static QColor readColor(const QJsonValue& val)
{
    if (!val.isString()) {
        return QColor();
    }
    return QColor::fromString(val.toString());
}

static QTextCharFormat readTextFormat(const QJsonValue& val)
{
    QTextCharFormat format;

    if (val.isString()) {
        format.setForeground(QColor::fromString(val.toString()));
    }
    else if (val.isObject()) {
        QJsonObject formatObj = val.toObject();

        if (formatObj.contains("font-weight") && formatObj.value("font-weight").isDouble()) {
            format.setFontWeight(static_cast<int>(formatObj.value("font-weight").toDouble()));
        }
        if (formatObj.contains("font-underline") && formatObj.value("font-underline").isBool()) {
            format.setFontUnderline(formatObj.value("font-underline").toBool());
        }
        if (formatObj.contains("font-italic") && formatObj.value("font-italic").isBool()) {
            format.setFontItalic(formatObj.value("font-italic").toBool());
        }
    }
    return format;
}

static void readHighlightingFormats(const QJsonObject& obj,
                                    QHash<HiglightingMarkerKind, QTextCharFormat>& highlightingFormats)
{
    highlightingFormats[HiglightingMarkerKind::COMMENT] = readTextFormat(obj.value("comment"));
    highlightingFormats[HiglightingMarkerKind::STRING_LITERAL] = readTextFormat(obj.value("string-literal"));
    highlightingFormats[HiglightingMarkerKind::NUMBER_LITERAL] = readTextFormat(obj.value("number-literal"));
    highlightingFormats[HiglightingMarkerKind::ESCAPE] = readTextFormat(obj.value("escape"));
    highlightingFormats[HiglightingMarkerKind::MATH_OPERATOR] = readTextFormat(obj.value("math-operator"));
    highlightingFormats[HiglightingMarkerKind::MATH_DELIMITER] = readTextFormat(obj.value("math-delimiter"));
    highlightingFormats[HiglightingMarkerKind::HEADING] = readTextFormat(obj.value("heading"));
    highlightingFormats[HiglightingMarkerKind::EMPHASIS] = readTextFormat(obj.value("emphasis"));
    highlightingFormats[HiglightingMarkerKind::STRONG_EMPHASIS] = readTextFormat(obj.value("strong-emphasis"));
    highlightingFormats[HiglightingMarkerKind::URL] = readTextFormat(obj.value("url"));
    highlightingFormats[HiglightingMarkerKind::RAW] = readTextFormat(obj.value("raw"));
    highlightingFormats[HiglightingMarkerKind::LABEL] = readTextFormat(obj.value("label"));
    highlightingFormats[HiglightingMarkerKind::REFERENCE] = readTextFormat(obj.value("reference"));
    highlightingFormats[HiglightingMarkerKind::LIST_ENTRY] = readTextFormat(obj.value("list-entry"));
    highlightingFormats[HiglightingMarkerKind::TERM] = readTextFormat(obj.value("list-term"));
    highlightingFormats[HiglightingMarkerKind::VARIABLE_NAME] = readTextFormat(obj.value("variable"));
    highlightingFormats[HiglightingMarkerKind::FUNCTION_NAME] = readTextFormat(obj.value("function"));
    highlightingFormats[HiglightingMarkerKind::KEYWORD] = readTextFormat(obj.value("keyword"));
}

static void readEditorColors(const QJsonObject& obj,
                             QHash<EditorTheme::EditorColor, QColor>& editorColors)
{
    editorColors[EditorTheme::EditorColor::SPELLING_ERROR] = readColor(obj.value("spelling-error"));
    editorColors[EditorTheme::EditorColor::MATCHING_BRACKET] = readColor(obj.value("matching-bracket"));
}

EditorTheme::EditorTheme(const QString& themeJsonFile)
{
    QFile file(themeJsonFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Couldn't open theme file!" << file.errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();

    readHighlightingFormats(obj.value("highlighting-formats").toObject(), d_highlightingFormats);
    readEditorColors(obj.value("editor-colors").toObject(), d_editorColors);
}

}
