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
#pragma once

#include <QMap>
#include <QTextBlockUserData>
#include <QTextDocument>

#include <concepts>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace katvan {

class CodeModel;

enum class BlockDataKind
{
    STATE_SPANS = 0,
    SPELLING,
    ISOLATES,
    LAYOUT,
};

template <typename T>
concept BlockDataSection = requires {
    requires std::derived_from<T, QTextBlockUserData>;
    requires std::same_as<decltype(T::DATA_KIND), const BlockDataKind>;
};

class BlockData : public QTextBlockUserData
{
public:
    ~BlockData() {
        qDeleteAll(d_sections);
    }

    template <BlockDataSection T>
    static T* get(const QTextBlock& block) {
        BlockData* data = dynamic_cast<BlockData*>(block.userData());
        if (data == nullptr) {
            return nullptr;
        }

        if (!data->d_sections.contains(T::DATA_KIND)) {
            return nullptr;
        }
        return dynamic_cast<T*>(data->d_sections[T::DATA_KIND]);
    }

    template <BlockDataSection T>
    static void set(QTextBlock block, T* section) {
        BlockData* data = dynamic_cast<BlockData*>(block.userData());
        if (data == nullptr) {
            data = new BlockData();
            block.setUserData(data);
        }

        if (data->d_sections.contains(T::DATA_KIND)) {
            delete data->d_sections[T::DATA_KIND];
        }
        data->d_sections[T::DATA_KIND] = section;
    }

private:
    QMap<BlockDataKind, QTextBlockUserData*> d_sections;
};

class Document : public QTextDocument
{
    Q_OBJECT

public:
    Document(QObject* parent = nullptr);

    CodeModel* codeModel() { return d_codeModel; }

    QString textForPreview() const;

public slots:
    void setDocumentText(const QString& text);

private slots:
    void propagateDocumentEdit(int from, int charsRemoved, int charsAdded);

signals:
    void contentReset();
    void contentModified();
    void contentEdited(int from, int to, QString text);

private:
    CodeModel* d_codeModel;

    QTimer* d_debounceTimer;

    bool d_suppressContentChangeHandling;
};

}
