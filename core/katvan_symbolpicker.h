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

#include <QAbstractListModel>
#include <QDialog>
#include <QJsonArray>
#include <QSortFilterProxyModel>

QT_BEGIN_NAMESPACE
class QDialogButtonBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
QT_END_NAMESPACE

namespace katvan {

class TypstDriverWrapper;

class SymbolListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    SymbolListModel(QObject* parent = nullptr);

    void setSymbolData(QJsonArray data);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    QJsonArray d_data;
    QFont d_symbolFont;
};

class SymbolFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SymbolFilterModel(QObject* parent = nullptr);

public slots:
    void setCategoryFilter(const QVariant& category);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QVariant d_categoryFilter;
};

class SymbolPicker : public QDialog
{
    Q_OBJECT

public:
    SymbolPicker(TypstDriverWrapper* driver, QWidget* parent = nullptr);

    QString selectedSymbolName() const;

protected:
    void showEvent(QShowEvent*) override;

private slots:
    void symbolsJsonReady(QByteArray json);
    void currentSymbolChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    void setupUI();

    SymbolListModel* d_model;
    SymbolFilterModel* d_filterModel;

    QListView* d_symbolListView;
    QLineEdit* d_filterEdit;
    QComboBox* d_categoryCombo;
    QLabel* d_detailsLabel;
    QDialogButtonBox* d_buttonBox;
};

}
