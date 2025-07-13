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
#include "katvan_editorsettings.h"

#include <QApplication>

namespace katvan {

static std::optional<bool> parseModeLineBool(const QString& value)
{
    if (value.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    else if (value.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("0"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return std::nullopt;
}

void EditorSettings::parseModeLine(QString mode, EditorSettings::ModeSource source)
{
    // E.g. "katvan: font DejaVu Sans; font-size 12; replace-tabs on; indent-width 4;"

    qsizetype pos = mode.indexOf(QLatin1Char(':'));
    if (pos > 0) {
        mode = mode.sliced(pos + 1, mode.size() - pos - 1);
    }

    auto parts = mode.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : std::as_const(parts)) {
        QString s = part.trimmed();
        pos = s.indexOf(QLatin1Char(' '));
        if (pos < 0) {
            continue;
        }

        QString variable = s.sliced(0, pos);
        QString rest = s.sliced(pos + 1, s.size() - pos - 1).trimmed();

        if (variable == QStringLiteral("font")) {
            d_fontFamily = rest;
        }
        else if (variable == QStringLiteral("font-size")) {
            bool ok = false;
            int size = rest.toInt(&ok);
            if (ok && size > 0) {
                d_fontSize = size;
            }
        }
        else if (variable == QStringLiteral("indent-mode")) {
            if (rest == QStringLiteral("none")) {
                d_indentMode = EditorSettings::IndentMode::NONE;
            }
            else if (rest == QStringLiteral("normal")) {
                d_indentMode = EditorSettings::IndentMode::NORMAL;
            }
            else if (rest == QStringLiteral("typst")) {
                d_indentMode = EditorSettings::IndentMode::SMART;
            }
        }
        else if (variable == QStringLiteral("replace-tabs")) {
            auto ob = parseModeLineBool(rest);
            if (ob) {
                if (ob.value()) {
                    d_indentStyle = EditorSettings::IndentStyle::SPACES;
                }
                else {
                    d_indentStyle = EditorSettings::IndentStyle::TABS;
                }
            }
        }
        else if (variable == QStringLiteral("indent-width")) {
            bool ok = false;
            int width = rest.toInt(&ok);
            if (ok && width >= 0) {
                d_indentWidth = width;
            }
        }
        else if (variable == QStringLiteral("tab-width")) {
            bool ok = false;
            int width = rest.toInt(&ok);
            if (ok && width >= 0) {
                d_tabWidth = width;
            }
        }
        else if (variable == QStringLiteral("scheme")) {
            d_colorScheme = rest;
        }
        else if (variable == QStringLiteral("show-line-numbers")) {
            if (rest == QStringLiteral("both")) {
                d_lineNumberStyle = EditorSettings::LineNumberStyle::BOTH_SIDES;
            }
            else if (rest == QStringLiteral("primary")) {
                d_lineNumberStyle = EditorSettings::LineNumberStyle::PRIMARY_ONLY;
            }
            else if (rest == QStringLiteral("none")) {
                d_lineNumberStyle = EditorSettings::LineNumberStyle::NONE;
            }
        }
        else if (variable == QStringLiteral("show-control-chars")) {
            d_showControlChars = parseModeLineBool(rest);
        }
        else if (variable == QStringLiteral("auto-brackets")) {
            d_autoBrackets = parseModeLineBool(rest);
        }
        else if (variable == QStringLiteral("auto-trigger-completions")) {
            d_autoTriggerCompletions = parseModeLineBool(rest);
        }
        else if (variable == QStringLiteral("backup-interval") && source == ModeSource::SETTINGS) {
            bool ok = false;
            int interval = rest.toInt(&ok);
            if (ok && interval >= 0) {
                d_autoBackupInterval = interval;
            }
        }
    }
}

QString EditorSettings::toModeLine() const
{
    QString result;

    if (d_fontFamily) {
        result += QLatin1String("font %1; ").arg(d_fontFamily.value());
    }
    if (d_fontSize) {
        result += QLatin1String("font-size %1; ").arg(QString::number(d_fontSize.value()));
    }
    if (d_indentMode) {
        switch (d_indentMode.value()) {
            case EditorSettings::IndentMode::NONE:
                result += QLatin1String("indent-mode none; ");
                break;
            case EditorSettings::IndentMode::NORMAL:
                result += QLatin1String("indent-mode normal; ");
                break;
            case EditorSettings::IndentMode::SMART:
                result += QLatin1String("indent-mode typst; ");
                break;
        }
    }
    if (d_indentStyle) {
        if (d_indentStyle.value() == EditorSettings::IndentStyle::SPACES) {
            result += QStringLiteral("replace-tabs on; ");
        }
        else {
            result += QStringLiteral("replace-tabs off; ");
        }
    }
    if (d_indentWidth) {
        result += QLatin1String("indent-width %1; ").arg(QString::number(d_indentWidth.value()));
    }
    if (d_tabWidth) {
        result += QLatin1String("tab-width %1; ").arg(QString::number(d_tabWidth.value()));
    }
    if (d_colorScheme) {
        result += QLatin1String("scheme %1; ").arg(d_colorScheme.value());
    }
    if (d_lineNumberStyle) {
        switch (d_lineNumberStyle.value()) {
            case EditorSettings::LineNumberStyle::BOTH_SIDES:
                result += QLatin1String("show-line-numbers both; ");
                break;
            case EditorSettings::LineNumberStyle::PRIMARY_ONLY:
                result += QLatin1String("show-line-numbers primary; ");
                break;
            case EditorSettings::LineNumberStyle::NONE:
                result += QLatin1String("show-line-numbers none; ");
                break;
        }
    }
    if (d_showControlChars) {
        if (d_showControlChars.value()) {
            result += QLatin1String("show-control-chars on; ");
        }
        else {
            result += QLatin1String("show-control-chars off; ");
        }
    }
    if (d_autoBrackets) {
        if (d_autoBrackets.value()) {
            result += QLatin1String("auto-brackets on; ");
        }
        else {
            result += QLatin1String("auto-brackets off; ");
        }
    }
    if (d_autoTriggerCompletions) {
        if (d_autoTriggerCompletions.value()) {
            result += QLatin1String("auto-trigger-completions on; ");
        }
        else {
            result += QLatin1String("auto-trigger-completions off; ");
        }
    }
    if (d_autoBackupInterval) {
        result += QLatin1String("backup-interval %1; ").arg(QString::number(d_autoBackupInterval.value()));
    }

    return result.trimmed();
}

QString EditorSettings::fontFamily() const
{
    if (d_fontFamily) {
        return d_fontFamily.value();
    }
    return QApplication::font().family();
}

int EditorSettings::fontSize() const
{
    if (d_fontSize) {
        return d_fontSize.value();
    }
    return QApplication::font().pointSize();
}

QFont EditorSettings::font() const
{
    return QFont(fontFamily(), fontSize());
}

EditorSettings::IndentMode EditorSettings::indentMode() const
{
    return d_indentMode.value_or(EditorSettings::IndentMode::NONE);
}

EditorSettings::IndentStyle EditorSettings::indentStyle() const
{
    return d_indentStyle.value_or(EditorSettings::IndentStyle::SPACES);
}

int EditorSettings::indentWidth() const
{
    return d_indentWidth.value_or(4);
}

int EditorSettings::tabWidth() const
{
    return d_tabWidth.value_or(indentWidth());
}

QString EditorSettings::colorScheme() const
{
    return d_colorScheme.value_or("auto");
}

EditorSettings::LineNumberStyle EditorSettings::lineNumberStyle() const
{
    return d_lineNumberStyle.value_or(EditorSettings::LineNumberStyle::BOTH_SIDES);
}

bool EditorSettings::showControlChars() const
{
    return d_showControlChars.value_or(true);
}

bool EditorSettings::autoBrackets() const
{
    return d_autoBrackets.value_or(true);
}

bool EditorSettings::autoTriggerCompletions() const
{
    return d_autoTriggerCompletions.value_or(true);
}

int EditorSettings::autoBackupInterval() const
{
    return d_autoBackupInterval.value_or(15);
}

void EditorSettings::mergeSettings(const EditorSettings& other)
{
    if (other.d_fontFamily) {
        d_fontFamily = other.d_fontFamily;
    }
    if (other.d_fontSize) {
        d_fontSize = other.d_fontSize;
    }
    if (other.d_indentMode) {
        d_indentMode = other.d_indentMode;
    }
    if (other.d_indentStyle) {
        d_indentStyle = other.d_indentStyle;
    }
    if (other.d_indentWidth) {
        d_indentWidth = other.d_indentWidth;
    }
    if (other.d_tabWidth) {
        d_tabWidth = other.d_tabWidth;
    }
    if (other.d_colorScheme) {
        d_colorScheme = other.d_colorScheme;
    }
    if (other.d_lineNumberStyle) {
        d_lineNumberStyle = other.d_lineNumberStyle;
    }
    if (other.d_showControlChars) {
        d_showControlChars = other.d_showControlChars;
    }
    if (other.d_autoBrackets) {
        d_autoBrackets = other.d_autoBrackets;
    }
    if (other.d_autoTriggerCompletions) {
        d_autoTriggerCompletions = other.d_autoTriggerCompletions;
    }
}

}

#include "moc_katvan_editorsettings.cpp"
