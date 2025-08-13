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
#pragma once

#include <QAbstractListModel>
#include <QAbstractItemDelegate>
#include <QFont>
#include <QJsonObject>
#include <QList>

QT_BEGIN_NAMESPACE
class QCompleter;
class QJsonObject;
QT_END_NAMESPACE

namespace katvan {

class Editor;

class CompletionListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    CompletionListModel(QObject* parent = nullptr);

    void setSuggestions(const QList<QJsonObject>& suggestions);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<QJsonObject> d_suggestions;
    QFont d_symbolFont;
};

class CompletionSuggestionDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    CompletionSuggestionDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QFont d_labelFont;
};

class CompletionManager : public QObject
{
    Q_OBJECT

public:
    CompletionManager(Editor* editor);

    bool isActive() const;

    bool isImplicitCompletionAllowed() const { return d_implicitCompletionAllowed; }
    void setImplicitCompletionAllowed(bool allow) { d_implicitCompletionAllowed = allow; }

public slots:
    void startExplicitCompletion();
    void startImplicitCompletion();
    void startCompletion(bool implicit);
    void close();
    void completionsReady(int line, int column, QByteArray completionsJson);
    void updateCompletionPrefix(bool force = false);

signals:
    void completionsRequested(int blockNumber, int charOffset, bool implicit);

private slots:
    void suggestionSelected(const QModelIndex& index);

private:
    Editor* d_editor;
    QCompleter* d_completer;
    CompletionListModel* d_model;

    bool d_implicitCompletionAllowed;
    bool d_completionsRequested;
    int d_suggestionsStartPosition;
};

}
