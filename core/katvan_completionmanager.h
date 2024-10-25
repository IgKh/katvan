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
#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>
#include <QTextCursor>

QT_BEGIN_NAMESPACE
class QCompleter;
class QJsonObject;
class QTextEdit;
QT_END_NAMESPACE

namespace katvan {

class Editor;

class CompletionListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    CompletionListModel(const QList<QJsonObject>& suggestions, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<QJsonObject> d_suggestions;
};

class CompletionManager : public QObject
{
    Q_OBJECT

public:
    CompletionManager(QTextEdit* editor);

    bool isActive() const;

public slots:
    void startCompletion();
    void completionsReady(int line, int column, QByteArray completionsJson);
    void updateCompletionPrefix();

signals:
    void completionsRequested(int blockNumber, int charOffset);

private slots:
    void suggestionSelected(const QModelIndex& index);

private:
    QTextEdit* d_editor;
    QCompleter* d_completer;

    bool d_completionsRequested;
    QTextCursor d_suggestionsStartCursor;
};

}
