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

#include <QDialog>

#include "katvan_spellchecker.h"

QT_BEGIN_NAMESPACE
class QListView;
class QStandardItemModel;
QT_END_NAMESPACE

namespace katvan {

class SpellingDialog : public QDialog
{
    Q_OBJECT

public:
    SpellingDialog(QWidget* parent = nullptr);

    void loadDictionaries(SpellChecker* spellChecker);

    QList<SpellChecker::DictionaryDef> selectedDictionaries() const;

private:
    void setupUI();

    QStandardItemModel* d_dictionaryModel;

    QListView* d_dictionaryList;
};

}
