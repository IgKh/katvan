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
#include "katvan_symbolpicker.h"
#include "katvan_text_utils.h"
#include "katvan_typstdriverwrapper.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace katvan {

Q_GLOBAL_STATIC(QList<const char*>, SYMBOL_CATEGORIES, {
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Normal"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Emoji"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Alphabetic"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Relation"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Unary"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Binary"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Opening"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Closing"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Large"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Fence"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Diacritic"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Punctuation"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Space"),
    QT_TRANSLATE_NOOP("katvan::SymbolPicker", "Other"),
});

enum SymbolModelRoles {
    SYMBOL_NAME_ROLE = Qt::UserRole + 1,
    SYMBOL_CODEPOINT_ROLE,
    SYMBOL_DESCRIPTION_ROLE,
    SYMBOL_CATEGORY_ROLE,
};

class SymbolDelegate : public QStyledItemDelegate
{
public:
    SymbolDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(96, 96);
    }
};

SymbolPicker::SymbolPicker(TypstDriverWrapper* driver, QWidget* parent)
    : QDialog(parent)
{
    setupUI();

    d_model = new SymbolListModel(this);

    d_filterModel = new SymbolFilterModel(this);
    d_filterModel->setSourceModel(d_model);
    d_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    d_symbolListView->setModel(d_filterModel);

    connect(d_filterEdit, &QLineEdit::textEdited, d_filterModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(d_categoryCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        d_filterModel->setCategoryFilter(d_categoryCombo->itemData(index));
    });
    connect(d_symbolListView, &QAbstractItemView::activated, this, &QDialog::accept);
    connect(d_symbolListView->selectionModel(), &QItemSelectionModel::currentChanged,
           this, &SymbolPicker::currentSymbolChanged);

    connect(driver, &TypstDriverWrapper::symbolsJsonReady, this, &SymbolPicker::symbolsJsonReady);
    driver->requestAllSymbolsJson();
}

QString SymbolPicker::selectedSymbolName() const
{
    QModelIndex index = d_symbolListView->currentIndex();
    if (!index.isValid()) {
        return QString();
    }

    return index.data(SYMBOL_NAME_ROLE).toString();
}

void SymbolPicker::setupUI()
{
    setWindowTitle(tr("Symbol Picker"));
    resize(820, 600);

    d_filterEdit = new QLineEdit();
    d_filterEdit->setLayoutDirection(Qt::LeftToRight);
    d_filterEdit->setPlaceholderText(tr("Enter partial symbol name or description in English"));

    QLabel* filterLabel = new QLabel(tr("Filter:"));
    filterLabel->setBuddy(d_filterEdit);

    d_categoryCombo = new QComboBox();

    d_categoryCombo->addItem(tr("Any"), QVariant());

    const auto& categories = *SYMBOL_CATEGORIES;
    for (const char* category : categories) {
        d_categoryCombo->addItem(tr(category), category);
    }

    QLabel* categoryLabel = new QLabel(tr("Category:"));
    categoryLabel->setBuddy(d_categoryCombo);

    d_symbolListView = new QListView();
    d_symbolListView->setLayoutDirection(Qt::LeftToRight);
    d_symbolListView->setViewMode(QListView::IconMode);
    d_symbolListView->setMovement(QListView::Static);
    d_symbolListView->setResizeMode(QListView::Adjust);
    d_symbolListView->setLayoutMode(QListView::Batched);
    d_symbolListView->setGridSize(QSize(96, 96));
    d_symbolListView->setIconSize(QSize(48, 48));
    d_symbolListView->setSpacing(5);
    d_symbolListView->setUniformItemSizes(true);
    d_symbolListView->setItemDelegate(new SymbolDelegate(this));

    d_detailsLabel = new QLabel();
    d_detailsLabel->setLayoutDirection(Qt::LeftToRight);
    d_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d_detailsLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    d_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(d_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(d_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(d_filterEdit, 1);
    filterLayout->addSpacing(1);
    filterLayout->addWidget(categoryLabel);
    filterLayout->addWidget(d_categoryCombo);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(filterLayout);
    layout->addWidget(d_symbolListView, 1);
    layout->addWidget(d_detailsLabel);
    layout->addWidget(d_buttonBox);
}

void SymbolPicker::showEvent(QShowEvent*)
{
    d_filterEdit->clear();
    d_filterModel->setFilterFixedString(QString());
    d_filterModel->setCategoryFilter(QVariant());
    d_symbolListView->setCurrentIndex(QModelIndex());
    d_symbolListView->clearSelection();
    d_symbolListView->scrollToTop();
}

void SymbolPicker::symbolsJsonReady(QByteArray json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);

    if (doc.isNull()) {
        qWarning() << "Failed parsing symbols:" << error.errorString();
        return;
    }

    d_model->setSymbolData(doc.array());
}

void SymbolPicker::currentSymbolChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    if (!current.isValid()) {
        d_detailsLabel->clear();
        d_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }

    QString codepoint = current.data(SYMBOL_CODEPOINT_ROLE).toString();
    QString name = current.data(SYMBOL_NAME_ROLE).toString();
    QString description = current.data(SYMBOL_DESCRIPTION_ROLE).toString();

    ulong point = static_cast<ulong>(utils::firstCodepointOf(codepoint));
    QString pointHex = QString::number(point, 16)
        .rightJustified(4, QLatin1Char('0'))
        .toUpper();

    d_detailsLabel->setText(QStringLiteral("U+%1 %2 (%3)").arg(pointHex, description, name));
    d_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

SymbolListModel::SymbolListModel(QObject* parent)
    : QAbstractListModel(parent)
    , d_symbolFont(utils::SYMBOL_FONT_FAMILY)
{
}

void SymbolListModel::setSymbolData(QJsonArray data)
{
    beginResetModel();
    d_data = std::move(data);
    endResetModel();
}

int SymbolListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(d_data.size());
}

QVariant SymbolListModel::data(const QModelIndex& index, int role) const
{
    if (index.parent().isValid() || index.column() != 0 || index.row() >= d_data.size()) {
        return QVariant();
    }

    QJsonObject obj = d_data[index.row()].toObject();

    if (role == Qt::DisplayRole) {
        // For display, strip the first component from the ID
        QString name = obj["name"].toString();
        qsizetype pos = name.indexOf('.');
        if (pos >= 0) {
            return name.sliced(pos + 1);
        }
        return name;
    }
    else if (role == SYMBOL_NAME_ROLE) {
        return obj["name"].toString();
    }
    else if (role == Qt::DecorationRole) {
        QString codepoint = obj["codepoint"].toString();
        if (!codepoint.isEmpty()) {
            return utils::fontIcon(utils::firstCodepointOf(codepoint), d_symbolFont);
        }
    }
    else if (role == Qt::ToolTipRole || role == SYMBOL_DESCRIPTION_ROLE) {
        return obj["description"].toString();
    }
    else if (role == SYMBOL_CODEPOINT_ROLE) {
        return obj["codepoint"].toString();
    }
    else if (role == SYMBOL_CATEGORY_ROLE) {
        return obj["category"].toString();
    }
    return QVariant();
}

SymbolFilterModel::SymbolFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void SymbolFilterModel::setCategoryFilter(const QVariant& category)
{
    d_categoryFilter = category;
    invalidateRowsFilter();
}

bool SymbolFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (sourceParent.isValid()) {
        return false;
    }

    QModelIndex index = sourceModel()->index(sourceRow, 0);
    if (!index.isValid()) {
        return false;
    }

    QRegularExpression re = filterRegularExpression();
    QString name = index.data(SYMBOL_NAME_ROLE).toString();
    QString description = index.data(SYMBOL_DESCRIPTION_ROLE).toString();
    QString category = index.data(SYMBOL_CATEGORY_ROLE).toString();

    if (!d_categoryFilter.isNull() && category != d_categoryFilter.toString()) {
        return false;
    }

    return re.match(name).hasMatch()
        || re.match(description).hasMatch();
}

}

#include "moc_katvan_symbolpicker.cpp"
