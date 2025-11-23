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
#include "katvan_exportdialog.h"
#include "katvan_utils.h"

#include "katvan_typstdriverwrapper.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace katvan {

#if defined(KATVAN_FLATPAK_BUILD)
static constexpr bool IS_SANDBOXED = true;
#else
static constexpr bool IS_SANDBOXED = false;
#endif

ExportDialog::ExportDialog(TypstDriverWrapper* driver, QWidget* parent)
    : QDialog(parent)
    , d_driver(driver)
{
    setupUI();
}

void ExportDialog::setupUI()
{
    setWindowTitle(tr("Export Document"));
    setMinimumWidth(500);

    d_formatCombo = new QComboBox();
    d_formatCombo->addItem(utils::themeIcon("application-pdf"), tr("PDF"));
    //d_formatCombo->addItem(tr("PNG (Single File)"));
    //d_formatCombo->addItem(tr("PNG (Multiple Files)"));

    QLabel* formatLabel = new QLabel(tr("&Format:"));
    formatLabel->setBuddy(d_formatCombo);

    d_targetFilePath = new QLineEdit();
    d_targetFilePath->setLayoutDirection(Qt::LeftToRight);
    d_targetFilePath->setPlaceholderText(tr("Target path"));
    if (IS_SANDBOXED) {
        d_targetFilePath->setDisabled(true);
    }

    d_selectFileButton = new QPushButton(utils::themeIcon("document-open"), tr("Browse..."));
    connect(d_selectFileButton, &QPushButton::clicked, this, &ExportDialog::browseForTargetFile);

    QGroupBox* singleTargetGroup = new QGroupBox();

    d_pdfVersionCombo = new QComboBox();
    d_pdfVersionCombo->addItem("PDF 1.4", "1.4");
    d_pdfVersionCombo->addItem("PDF 1.5", "1.5");
    d_pdfVersionCombo->addItem("PDF 1.6", "1.6");
    d_pdfVersionCombo->addItem("PDF 1.7", "1.7");
    d_pdfVersionCombo->addItem("PDF 2.0", "2.0");
    d_pdfVersionCombo->setCurrentIndex(d_pdfVersionCombo->findData("1.7"));

    d_pdfGenerateTags = new QCheckBox(tr("Generate tagged PDF"));
    d_pdfGenerateTags->setChecked(true);

    QGroupBox* pdfSettingsGroup = new QGroupBox(tr("PDF Settings"));

    d_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(d_buttonBox, &QDialogButtonBox::accepted, this, &ExportDialog::doExport);
    connect(d_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QHBoxLayout* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(d_formatCombo);
    formatLayout->addStretch(1);

    QHBoxLayout* singleTargetGroupLayout = new QHBoxLayout(singleTargetGroup);
    singleTargetGroupLayout->addWidget(d_targetFilePath, 1);
    singleTargetGroupLayout->addWidget(d_selectFileButton);

    QFormLayout* pdfSettingsGroupLayout = new QFormLayout(pdfSettingsGroup);
    pdfSettingsGroupLayout->addRow(new QLabel(tr("PDF Version:")), d_pdfVersionCombo);
    pdfSettingsGroupLayout->addRow(d_pdfGenerateTags);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(formatLayout);
    layout->addWidget(singleTargetGroup);
    layout->addWidget(pdfSettingsGroup);
    layout->addStretch(1);
    layout->addWidget(d_buttonBox);
}

static QString getSuggestedTargetPath(const QString& sourceFile)
{
    if (sourceFile.isEmpty()) {
        return QString();
    }

    QFileInfo info { sourceFile };
    return info.path() + QDir::separator() + info.baseName() + ".pdf";
}

void ExportDialog::setSourceFile(const QString& path)
{
    if (!d_sourceFile.isEmpty() && path == d_sourceFile) {
        return;
    }

    d_sourceFile = path;

    if (!path.isEmpty() && !IS_SANDBOXED) {
        d_selectedTargetFile = getSuggestedTargetPath(d_sourceFile);
        d_targetFilePath->setText(d_selectedTargetFile);
    }
    else {
        d_selectedTargetFile.clear();
        d_targetFilePath->clear();
    }

    updateButtonState();
}

void ExportDialog::browseForTargetFile()
{
    QFileDialog dialog { this, tr("Select Target File") };
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    dialog.setNameFilter(QObject::tr("PDF files (*.pdf)"));
    dialog.setDefaultSuffix("pdf");

    if (!d_selectedTargetFile.isEmpty()) {
        QFileInfo info { d_selectedTargetFile };
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.fileName());
    }
    else if (IS_SANDBOXED && !d_sourceFile.isEmpty()) {
        QFileInfo info { getSuggestedTargetPath(d_sourceFile) };
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.fileName());
    }

    if (dialog.exec() == QDialog::Accepted) {
        d_selectedTargetFile = dialog.selectedFiles().at(0);
        d_targetFilePath->setText(utils::getHostPath(d_selectedTargetFile));
    }

    updateButtonState();
}

void ExportDialog::updateButtonState()
{
    d_buttonBox->button(QDialogButtonBox::Save)->setEnabled(!d_selectedTargetFile.isEmpty());
}

void ExportDialog::doExport()
{
    d_driver->exportToPdf(d_selectedTargetFile,
        d_pdfVersionCombo->currentData().toString(),
        d_pdfGenerateTags->isChecked());

    qApp->setOverrideCursor(Qt::WaitCursor);
    accept();
}

}

#include "moc_katvan_exportdialog.cpp"
