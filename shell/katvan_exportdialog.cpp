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
#include <QSet>
#include <QVBoxLayout>

namespace katvan {

#if defined(KATVAN_FLATPAK_BUILD)
static constexpr bool IS_SANDBOXED = true;
#else
static constexpr bool IS_SANDBOXED = false;
#endif

struct PdfAStandard
{
    const char* name;
    QString code;
    QString minPdfVersion;
    QString maxPdfVersion;
};

Q_GLOBAL_STATIC(QList<PdfAStandard>, PDFA_STANDARDS, {
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-1b"), "a-1b", "", "1.4" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-1a"), "a-1a", "", "1.4" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-2b"), "a-2b", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-2u"), "a-2u", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-2a"), "a-2a", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-3b"), "a-3b", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-3u"), "a-3u", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-3a"), "a-3a", "", "1.7" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-4"), "a-4", "2.0", "2.0" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-4f"), "a-4f", "2.0", "2.0" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/A-4e"), "a-4e", "2.0", "2.0" },
    PdfAStandard { QT_TRANSLATE_NOOP("katvan::ExportDialog", "PDF/UA-1"), "ua-1", "", "1.7" },
});

Q_GLOBAL_STATIC(QSet<QString>, PDFA_STANDARDS_REQUIRING_TAGGING, {
    "a-1a",
    "a-2a",
    "a-3a",
    "ua-1",
});

ExportDialog::ExportDialog(TypstDriverWrapper* driver, QWidget* parent)
    : QDialog(parent)
    , d_driver(driver)
{
    setupUI();
    pdfVersionChanged();
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
    connect(d_pdfVersionCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::pdfVersionChanged);

    d_pdfaStandardCombo = new QComboBox();
    connect(d_pdfaStandardCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::pdfaStandardChanged);

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
    pdfSettingsGroupLayout->addRow(new QLabel(tr("PDF/A Standard:")), d_pdfaStandardCombo);
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

void ExportDialog::pdfVersionChanged()
{
    QString pdfVersion = d_pdfVersionCombo->currentData().toString();
    QString selectedStandard = d_pdfaStandardCombo->currentData().toString();

    d_pdfaStandardCombo->clear();
    d_pdfaStandardCombo->addItem(tr("None"), "");

    for (const auto& standard : *PDFA_STANDARDS) {
        if (pdfVersion >= standard.minPdfVersion && pdfVersion <= standard.maxPdfVersion) {
            d_pdfaStandardCombo->addItem(tr(standard.name), standard.code);
        }
    }

    int idx = d_pdfaStandardCombo->findData(selectedStandard);
    if (idx >= 0) {
        d_pdfaStandardCombo->setCurrentIndex(idx);
    }
    else {
        d_pdfaStandardCombo->setCurrentIndex(0);
    }
}

void ExportDialog::pdfaStandardChanged()
{
    QString standardCode = d_pdfaStandardCombo->currentData().toString();

    bool taggingRequired = PDFA_STANDARDS_REQUIRING_TAGGING->contains(standardCode);
    if (taggingRequired) {
        d_pdfGenerateTags->setEnabled(false);
        d_pdfGenerateTags->setChecked(true);
    }
    else {
        d_pdfGenerateTags->setEnabled(true);
    }
}

void ExportDialog::updateButtonState()
{
    d_buttonBox->button(QDialogButtonBox::Save)->setEnabled(!d_selectedTargetFile.isEmpty());
}

void ExportDialog::doExport()
{
    d_driver->exportToPdf(d_selectedTargetFile,
        d_pdfVersionCombo->currentData().toString(),
        d_pdfaStandardCombo->currentData().toString(),
        d_pdfGenerateTags->isChecked());

    qApp->setOverrideCursor(Qt::WaitCursor);
    accept();
}

}

#include "moc_katvan_exportdialog.cpp"
