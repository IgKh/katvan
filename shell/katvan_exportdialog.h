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

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;

namespace katvan {

class TypstDriverWrapper;

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    ExportDialog(TypstDriverWrapper* driver, QWidget* parent = nullptr);

    void setSourceFile(const QString& path);

private slots:
    void browseForTargetFile();
    void pdfVersionChanged();
    void pdfaStandardChanged();
    void updateButtonState();
    void doExport();

private:
    void setupUI();

    QString d_sourceFile;
    QString d_selectedTargetFile;

    TypstDriverWrapper* d_driver;

    QComboBox* d_formatCombo;

    QLineEdit* d_targetFilePath;
    QPushButton* d_selectFileButton;

    QComboBox* d_pdfVersionCombo;
    QComboBox* d_pdfaStandardCombo;
    QCheckBox* d_pdfGenerateTags;

    QDialogButtonBox* d_buttonBox;
};

}
