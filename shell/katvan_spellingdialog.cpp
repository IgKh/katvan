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
#include "katvan_spellingdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListView>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace katvan {

static constexpr int DICT_NAME_ROLE = Qt::UserRole;
static constexpr int DICT_PATH_ROLE = Qt::UserRole + 1;

SpellingDialog::SpellingDialog(QWidget* parent)
    : QDialog(parent)
{
    d_dictionaryModel = new QStandardItemModel(this);

    setupUI();
}

void SpellingDialog::loadDictionaries(SpellChecker* spellChecker)
{
    d_dictionaryModel->clear();

    QStringList current = spellChecker->currentDictionaryNames();

    const auto dicts = spellChecker->findDictionaries();
    for (const auto& [dictName, dictPath] : dicts.asKeyValueRange()) {
        QString label = QString("%1 - %2").arg(
            dictName,
            spellChecker->dictionaryDisplayName(dictName));

        QStandardItem* item = new QStandardItem(label);
        item->setData(dictName, DICT_NAME_ROLE);
        item->setData(dictPath, DICT_PATH_ROLE);
        item->setCheckable(true);
        item->setSelectable(false);

        if (current.contains(dictName)) {
            item->setCheckState(Qt::Checked);
        }

        d_dictionaryModel->appendRow(item);
    }

    int width = d_dictionaryList->sizeHintForColumn(0)
        + 2 * d_dictionaryList->frameWidth()
        + d_dictionaryList->verticalScrollBar()->sizeHint().width();

    d_dictionaryList->setMinimumWidth(qMin(width, 500));
}

QList<SpellChecker::DictionaryDef> SpellingDialog::selectedDictionaries() const
{
    QList<SpellChecker::DictionaryDef> result;

    QStandardItem* root = d_dictionaryModel->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* item = root->child(i);
        if (item->checkState() != Qt::Checked) {
            continue;
        }

        result.append(std::make_pair(
            item->data(DICT_NAME_ROLE).toString(),
            item->data(DICT_PATH_ROLE).toString()));
    }

    return result;
}

void SpellingDialog::setupUI()
{
    setWindowTitle(tr("Spell Checking"));

    d_dictionaryList = new QListView();
    d_dictionaryList->setModel(d_dictionaryModel);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(tr("Select dictionaries to use for spell checking")));
    mainLayout->addWidget(d_dictionaryList);
    mainLayout->addWidget(buttonBox);
}

}

#include "moc_katvan_spellingdialog.cpp"
