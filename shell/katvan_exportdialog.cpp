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
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

namespace katvan {

#if defined(KATVAN_FLATPAK_BUILD)
static constexpr bool IS_SANDBOXED = true;
#else
static constexpr bool IS_SANDBOXED = false;
#endif

static constexpr QLatin1StringView PDF_FORMAT = QLatin1StringView("pdf");
static constexpr QLatin1StringView SINGLE_PNG_FORMAT = QLatin1StringView("png");
static constexpr QLatin1StringView MULTI_PNG_FORMAT = QLatin1StringView("png-multi");

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
    formatChanged();
    pdfVersionChanged();
}

void ExportDialog::setupUI()
{
    setWindowTitle(tr("Export Document"));
    setMinimumWidth(600);

    d_formatCombo = new QComboBox();
    d_formatCombo->addItem(utils::themeIcon("application-pdf"), tr("PDF"), PDF_FORMAT);
    d_formatCombo->addItem(utils::themeIcon("image-png"), tr("PNG (Single File)"), SINGLE_PNG_FORMAT);
    d_formatCombo->addItem(utils::themeIcon("image-png"), tr("PNG (File per Page)"), MULTI_PNG_FORMAT);
    d_formatCombo->setCurrentIndex(d_formatCombo->findData(PDF_FORMAT));
    connect(d_formatCombo, &QComboBox::currentIndexChanged, this, &ExportDialog::formatChanged);

    QLabel* formatLabel = new QLabel(tr("&Format:"));
    formatLabel->setBuddy(d_formatCombo);

    d_targetFilePath = new QLineEdit();
    d_targetFilePath->setLayoutDirection(Qt::LeftToRight);
    d_targetFilePath->setDisabled(true);

    d_selectFileButton = new QPushButton(utils::themeIcon("document-open"), tr("&Browse..."));
    connect(d_selectFileButton, &QPushButton::clicked, this, &ExportDialog::browseForTargetFile);

    d_singleTargetGroup = new QGroupBox();

    d_targetDirPath = new QLineEdit();
    d_targetDirPath->setLayoutDirection(Qt::LeftToRight);
    d_targetDirPath->setDisabled(true);

    QLabel* targetDirPathLabel = new QLabel("Di&rectory:");
    targetDirPathLabel->setBuddy(d_targetDirPath);

    d_selectDirButton = new QPushButton(utils::themeIcon("document-open-folder"), tr("&Browse..."));
    connect(d_selectDirButton, &QPushButton::clicked, this, &ExportDialog::browseForTargetDir);

    d_targetNamePattern = new QLineEdit();
    connect(d_targetNamePattern, &QLineEdit::editingFinished, this, &ExportDialog::updateButtonState);

    QLabel* targetNamePatternLabel = new QLabel("File Name &Pattern:");
    targetNamePatternLabel->setBuddy(d_targetNamePattern);

    QString patternInfo = QStringLiteral(
        "<p>%1</p>"
        "<ul>"
        "<li><code>{p}</code> %2</li>"
        "<li><code>{n}</code> %3</li>"
        "<li><code>{t}</code> %4</li>"
        "</ul>"
    ).arg(
        tr("You may use the following placeholders in the pattern:"),
        tr("Image page number"),
        tr("Image page number (zero-padded)"),
        tr("Total number of pages")
    );
    QLabel* patternInfoLabel = new QLabel(patternInfo);

    d_multiTargetGroup = new QGroupBox();

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

    d_pdfGenerateTags = new QCheckBox(tr("Generate &tagged PDF"));
    d_pdfGenerateTags->setChecked(true);

    d_pdfSettingsGroup = new QGroupBox(tr("PDF Settings"));

    d_rasterDpi = new QSpinBox();
    d_rasterDpi->setRange(10, 10000);
    d_rasterDpi->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    d_rasterDpi->setValue(72);

    d_rasterSettingsGroup = new QGroupBox(tr("Raster Settings"));

    d_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(d_buttonBox, &QDialogButtonBox::accepted, this, &ExportDialog::doExport);
    connect(d_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QHBoxLayout* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(d_formatCombo);
    formatLayout->addStretch(1);

    QHBoxLayout* singleTargetGroupLayout = new QHBoxLayout(d_singleTargetGroup);
    singleTargetGroupLayout->addWidget(d_targetFilePath, 1);
    singleTargetGroupLayout->addWidget(d_selectFileButton);

    Qt::Alignment labelAlignment = static_cast<Qt::Alignment>(style()->styleHint(QStyle::SH_FormLayoutLabelAlignment));

    QGridLayout* multiTargetGroupLayout = new QGridLayout(d_multiTargetGroup);
    multiTargetGroupLayout->addWidget(targetDirPathLabel, 0, 0, labelAlignment);
    multiTargetGroupLayout->addWidget(d_targetDirPath, 0, 1);
    multiTargetGroupLayout->addWidget(d_selectDirButton, 0, 2);
    multiTargetGroupLayout->addWidget(targetNamePatternLabel, 1, 0, labelAlignment);
    multiTargetGroupLayout->addWidget(d_targetNamePattern, 1, 1, 1, 2);
    multiTargetGroupLayout->addWidget(patternInfoLabel, 2, 1, 1, 2);
    multiTargetGroupLayout->setColumnStretch(1, 1);

    QFormLayout* pdfSettingsGroupLayout = new QFormLayout(d_pdfSettingsGroup);
    pdfSettingsGroupLayout->addRow(tr("PDF &Version:"), d_pdfVersionCombo);
    pdfSettingsGroupLayout->addRow(tr("PDF/A &Standard:"), d_pdfaStandardCombo);
    pdfSettingsGroupLayout->addRow(d_pdfGenerateTags);

    QFormLayout* rasterSettingsGroupLayout = new QFormLayout(d_rasterSettingsGroup);
    rasterSettingsGroupLayout->addRow(tr("&DPI:"), d_rasterDpi);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(formatLayout);
    layout->addWidget(d_singleTargetGroup);
    layout->addWidget(d_multiTargetGroup);
    layout->addWidget(d_pdfSettingsGroup);
    layout->addWidget(d_rasterSettingsGroup);
    layout->addStretch(1);
    layout->addWidget(d_buttonBox);
}

static QString getSuggestedTargetPath(const QString& sourceFile, const QString& format)
{
    if (sourceFile.isEmpty()) {
        return QString();
    }

    QString suffix;
    if (format == SINGLE_PNG_FORMAT) {
        suffix = ".png";
    }
    else {
        suffix = ".pdf";
    }

    QFileInfo info { sourceFile };
    return info.path() + QDir::separator() + info.baseName() + suffix;
}

void ExportDialog::setSuggestedTarget()
{
    QString format = d_formatCombo->currentData().toString();

    if (format == MULTI_PNG_FORMAT) {
        if (!d_sourceFile.isEmpty() && !IS_SANDBOXED) {
            QFileInfo info { d_sourceFile };
            d_selectedTargetDir = info.absoluteDir().path();
            d_targetDirPath->setText(d_selectedTargetDir);
            d_targetNamePattern->setText(QStringLiteral("%1_{n}_{t}.png").arg(info.baseName()));
        }
        else {
            d_selectedTargetDir.clear();
            d_targetDirPath->clear();
            d_targetNamePattern->clear();
        }
    }
    else {
        if (!d_sourceFile.isEmpty() && !IS_SANDBOXED) {
            d_selectedTargetFile = getSuggestedTargetPath(d_sourceFile, format);
            d_targetFilePath->setText(d_selectedTargetFile);
        }
        else {
            d_selectedTargetFile.clear();
            d_targetFilePath->clear();
        }
    }
}

void ExportDialog::setSourceFile(const QString& path)
{
    if (!d_sourceFile.isEmpty() && path == d_sourceFile) {
        return;
    }

    d_sourceFile = path;

    setSuggestedTarget();
    updateButtonState();
}

void ExportDialog::formatChanged()
{
    QString format = d_formatCombo->currentData().toString();

    if (format == MULTI_PNG_FORMAT) {
        d_singleTargetGroup->setVisible(false);
        d_multiTargetGroup->setVisible(true);
    }
    else {
        d_singleTargetGroup->setVisible(true);
        d_multiTargetGroup->setVisible(false);
    }

    if (format == PDF_FORMAT) {
        d_pdfSettingsGroup->setVisible(true);
        d_rasterSettingsGroup->setVisible(false);
    }
    else if (format == SINGLE_PNG_FORMAT || format == MULTI_PNG_FORMAT) {
        d_pdfSettingsGroup->setVisible(false);
        d_rasterSettingsGroup->setVisible(true);
    }

    setSuggestedTarget();
    updateButtonState();
}

void ExportDialog::browseForTargetFile()
{
    QFileDialog dialog { this, tr("Select Target File") };
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    QString format = d_formatCombo->currentData().toString();
    if (format == PDF_FORMAT) {
        dialog.setNameFilter(QObject::tr("PDF files (*.pdf)"));
        dialog.setDefaultSuffix("pdf");
    }
    else if (format == SINGLE_PNG_FORMAT) {
        dialog.setNameFilter(QObject::tr("PNG files (*.png)"));
        dialog.setDefaultSuffix("png");
    }

    if (!d_selectedTargetFile.isEmpty()) {
        QFileInfo info { d_selectedTargetFile };
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.fileName());
    }
    else if (IS_SANDBOXED && !d_sourceFile.isEmpty()) {
        QFileInfo info { getSuggestedTargetPath(d_sourceFile, format) };
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.fileName());
    }

    if (dialog.exec() == QDialog::Accepted) {
        d_selectedTargetFile = dialog.selectedFiles().at(0);
        d_targetFilePath->setText(utils::getHostPath(d_selectedTargetFile));
    }

    updateButtonState();
}

void ExportDialog::browseForTargetDir()
{
    QString initialDir;
    if (!d_selectedTargetDir.isEmpty()) {
        initialDir = d_selectedTargetDir;
    }
    else if (IS_SANDBOXED && !d_sourceFile.isEmpty()) {
        QFileInfo info { d_sourceFile };
        initialDir = info.absoluteDir().path();
    }

    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Target Directory"), initialDir);
    if (dir.isEmpty()) {
        return;
    }

    d_selectedTargetDir = dir;
    d_targetDirPath->setText(utils::getHostPath(d_selectedTargetDir));

    updateButtonState();
}

void ExportDialog::pdfVersionChanged()
{
    QString pdfVersion = d_pdfVersionCombo->currentData().toString();
    QString selectedStandard = d_pdfaStandardCombo->currentData().toString();

    d_pdfaStandardCombo->clear();
    d_pdfaStandardCombo->addItem(tr("None"), "");

    for (const auto& standard : std::as_const(*PDFA_STANDARDS)) {
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
    QPushButton* exportButton = d_buttonBox->button(QDialogButtonBox::Save);
    QString format = d_formatCombo->currentData().toString();

    if (format == MULTI_PNG_FORMAT) {
        exportButton->setEnabled(!d_selectedTargetDir.isEmpty() && !d_targetNamePattern->text().isEmpty());
    }
    else {
        exportButton->setEnabled(!d_selectedTargetFile.isEmpty());
    }
}

void ExportDialog::doExport()
{
    QString format = d_formatCombo->currentData().toString();

    if (format == PDF_FORMAT) {
        d_driver->exportToPdf(d_selectedTargetFile,
            d_pdfVersionCombo->currentData().toString(),
            d_pdfaStandardCombo->currentData().toString(),
            d_pdfGenerateTags->isChecked());
    }
    else if (format == SINGLE_PNG_FORMAT) {
        d_driver->exportToPng(d_selectedTargetFile, d_rasterDpi->value());
    }
    else if (format == MULTI_PNG_FORMAT) {
        d_driver->exportToPngMulti(d_selectedTargetDir,
            d_targetNamePattern->text(),
            d_rasterDpi->value());
    }

    qApp->setOverrideCursor(Qt::WaitCursor);
    accept();
}

}

#include "moc_katvan_exportdialog.cpp"
