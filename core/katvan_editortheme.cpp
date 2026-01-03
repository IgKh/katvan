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
#include "katvan_editortheme.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

QT_BEGIN_NAMESPACE
extern int qInitResources_themes();
QT_END_NAMESPACE

namespace katvan {

using HighlightingMarkerKind = parsing::HighlightingMarker::Kind;

bool EditorTheme::isAppInDarkMode()
{
    if (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        // Qt Vista style correctly reports dark mode setting on Windows 10,
        // but doesn't actually respect it - the default palette is light.
        if (qApp->style()->name() == "windowsvista") {
            return false;
        }
        return true;
    }

    // If the Qt platform integration doesn't support signaling a color scheme,
    // sniff it from the global palette.
    QPalette palette = qApp->palette();
    return palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

EditorTheme& EditorTheme::defaultTheme()
{
    if (isAppInDarkMode()) {
        return darkTheme();
    }
    else {
        return lightTheme();
    }
}

EditorTheme& EditorTheme::lightTheme()
{
    static EditorTheme lightTheme(":/themes/default-light.json");
    return lightTheme;
}

EditorTheme& EditorTheme::darkTheme()
{
    static EditorTheme darkTheme(":/themes/default-dark.json");
    return darkTheme;
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
                                    QHash<HighlightingMarkerKind, QTextCharFormat>& highlightingFormats)
{
    highlightingFormats[HighlightingMarkerKind::COMMENT] = readTextFormat(obj.value("comment"));
    highlightingFormats[HighlightingMarkerKind::STRING_LITERAL] = readTextFormat(obj.value("string-literal"));
    highlightingFormats[HighlightingMarkerKind::NUMBER_LITERAL] = readTextFormat(obj.value("number-literal"));
    highlightingFormats[HighlightingMarkerKind::ESCAPE] = readTextFormat(obj.value("escape"));
    highlightingFormats[HighlightingMarkerKind::MATH_OPERATOR] = readTextFormat(obj.value("math-operator"));
    highlightingFormats[HighlightingMarkerKind::MATH_DELIMITER] = readTextFormat(obj.value("math-delimiter"));
    highlightingFormats[HighlightingMarkerKind::HEADING] = readTextFormat(obj.value("heading"));
    highlightingFormats[HighlightingMarkerKind::EMPHASIS] = readTextFormat(obj.value("emphasis"));
    highlightingFormats[HighlightingMarkerKind::STRONG_EMPHASIS] = readTextFormat(obj.value("strong-emphasis"));
    highlightingFormats[HighlightingMarkerKind::URL] = readTextFormat(obj.value("url"));
    highlightingFormats[HighlightingMarkerKind::RAW] = readTextFormat(obj.value("raw"));
    highlightingFormats[HighlightingMarkerKind::LABEL] = readTextFormat(obj.value("label"));
    highlightingFormats[HighlightingMarkerKind::REFERENCE] = readTextFormat(obj.value("reference"));
    highlightingFormats[HighlightingMarkerKind::LIST_ENTRY] = readTextFormat(obj.value("list-entry"));
    highlightingFormats[HighlightingMarkerKind::TERM] = readTextFormat(obj.value("list-term"));
    highlightingFormats[HighlightingMarkerKind::VARIABLE_NAME] = readTextFormat(obj.value("variable"));
    highlightingFormats[HighlightingMarkerKind::FUNCTION_NAME] = readTextFormat(obj.value("function"));
    highlightingFormats[HighlightingMarkerKind::KEYWORD] = readTextFormat(obj.value("keyword"));
}

static void readEditorColors(const QJsonObject& obj,
                             QHash<EditorTheme::EditorColor, QColor>& editorColors)
{
    editorColors[EditorTheme::EditorColor::BACKGROUND] = readColor(obj.value("background"));
    editorColors[EditorTheme::EditorColor::FOREGROUND] = readColor(obj.value("foreground"));
    editorColors[EditorTheme::EditorColor::GUTTER] = readColor(obj.value("gutter"));
    editorColors[EditorTheme::EditorColor::CURRENT_LINE] = readColor(obj.value("current-line"));
    editorColors[EditorTheme::EditorColor::ERROR] = readColor(obj.value("error"));
    editorColors[EditorTheme::EditorColor::WARNING] = readColor(obj.value("warning"));
    editorColors[EditorTheme::EditorColor::MATCHING_BRACKET] = readColor(obj.value("matching-bracket"));
}

EditorTheme::EditorTheme(const QString& themeJsonFile)
{
    qInitResources_themes();

    QFile file(themeJsonFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Couldn't open theme file!" << file.errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();

    readHighlightingFormats(obj.value("highlighting-formats").toObject(), d_highlightingFormats);
    readEditorColors(obj.value("editor-colors").toObject(), d_editorColors);

    QFileInfo info(themeJsonFile);
    d_name = info.baseName();
}

QPalette EditorTheme::adjustPalette(QPalette original) const
{
    original.setColor(QPalette::Base, editorColor(EditorColor::BACKGROUND));
    original.setColor(QPalette::Text, editorColor(EditorColor::FOREGROUND));
    return original;
}

}
