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
#include "katvan_settingsdialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLibraryInfo>
#include <QSpinBox>
#include <QTabWidget>
#include <QRadioButton>
#include <QVBoxLayout>

namespace katvan {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

EditorSettings SettingsDialog::editorSettings() const
{
    return d_editorSettingsTab->settings();
}

void SettingsDialog::setEditorSettings(const EditorSettings& settings)
{
    d_editorSettingsTab->setSettings(settings);
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("Katvan Settings"));

    d_editorSettingsTab = new EditorSettingsTab();

    QTabWidget* tabWidget = new QTabWidget();
    tabWidget->addTab(d_editorSettingsTab, tr("Editor"));

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
}

EditorSettingsTab::EditorSettingsTab(QWidget* parent)
    : QWidget(parent)
{
    setupUI();

    updateFontSizes();
}

EditorSettings EditorSettingsTab::settings() const
{
    EditorSettings settings;

    settings.setFontFamily(d_editorFontComboBox->currentFont().family());
    settings.setFontSize(d_editorFontSizeComboBox->currentText().toInt());
    settings.setLineNumberStyle(d_lineNumberStyle->currentData().value<EditorSettings::LineNumberStyle>());
    settings.setShowControlChars(d_showControlChars->isChecked());
    settings.setIndentMode(d_indentMode->currentData().value<EditorSettings::IndentMode>());

    if (d_indentWithSpaces->isChecked()) {
        settings.setIndentStyle(EditorSettings::IndentStyle::SPACES);
    }
    else {
        settings.setIndentStyle(EditorSettings::IndentStyle::TABS);
    }

    settings.setIndentWidth(d_indentWidth->value());
    settings.setTabWidth(d_tabWidth->value());
    settings.setAutoBackupInterval(d_backupInterval->value());

    return settings;
}

void EditorSettingsTab::setSettings(const EditorSettings& settings)
{
    QFont font = settings.font();
    d_editorFontComboBox->setCurrentFont(font);
    d_editorFontSizeComboBox->setCurrentText(QString::number(font.pointSize()));

    QVariant lineNumberStyle = QVariant::fromValue(settings.lineNumberStyle());
    d_lineNumberStyle->setCurrentIndex(d_lineNumberStyle->findData(lineNumberStyle));

    d_showControlChars->setChecked(settings.showControlChars());

    QVariant indentMode = QVariant::fromValue(settings.indentMode());
    d_indentMode->setCurrentIndex(d_indentMode->findData(indentMode));

    if (settings.indentStyle() == EditorSettings::IndentStyle::SPACES) {
        d_indentWithSpaces->setChecked(true);
    }
    else {
        d_indentWithTabs->setChecked(true);
    }

    d_indentWidth->setValue(settings.indentWidth());
    d_tabWidth->setValue(settings.tabWidth());
    d_backupInterval->setValue(settings.autoBackupInterval());

    updateControlStates();
}

void EditorSettingsTab::setupUI()
{
    d_editorFontComboBox = new QFontComboBox();
    connect(d_editorFontComboBox, &QFontComboBox::currentFontChanged, this, &EditorSettingsTab::updateFontSizes);

    QLabel* editorFontLabel = new QLabel(tr("Editor &Font:"));
    editorFontLabel->setBuddy(d_editorFontComboBox);

    d_editorFontSizeComboBox = new QComboBox();

    d_lineNumberStyle = new QComboBox();
    d_lineNumberStyle->addItem(tr("On Both Sides"), QVariant::fromValue(EditorSettings::LineNumberStyle::BOTH_SIDES));
    d_lineNumberStyle->addItem(tr("On Primary Side"), QVariant::fromValue(EditorSettings::LineNumberStyle::PRIMARY_ONLY));
    d_lineNumberStyle->addItem(tr("Don't Show"), QVariant::fromValue(EditorSettings::LineNumberStyle::NONE));

    d_showControlChars = new QCheckBox(tr("Show Control Characters"));
    if (QLibraryInfo::version() < QVersionNumber(6, 9)) {
        d_showControlChars->setEnabled(false);
        d_showControlChars->setToolTip(tr("Requires Qt version 6.9 or above"));
    }

    d_indentMode = new QComboBox();
    d_indentMode->addItem(tr("None"), QVariant::fromValue(EditorSettings::IndentMode::NONE));
    d_indentMode->addItem(tr("Normal"), QVariant::fromValue(EditorSettings::IndentMode::NORMAL));
    d_indentMode->addItem(tr("Smart"), QVariant::fromValue(EditorSettings::IndentMode::SMART));

    d_indentWithSpaces = new QRadioButton(tr("&Spaces"));
    d_indentWithTabs = new QRadioButton(tr("&Tabs"));

    QButtonGroup* indentStyleBtnGroup = new QButtonGroup(this);
    indentStyleBtnGroup->addButton(d_indentWithSpaces);
    indentStyleBtnGroup->addButton(d_indentWithTabs);
    connect(indentStyleBtnGroup, &QButtonGroup::buttonToggled, this, &EditorSettingsTab::updateControlStates);

    d_indentWidth = new QSpinBox();
    d_indentWidth->setSuffix(tr(" characters"));

    d_tabWidth = new QSpinBox();
    d_tabWidth->setSuffix(tr(" characters"));

    QLabel* indentWidthLabel = new QLabel(tr("&Indent Width:"));
    indentWidthLabel->setBuddy(d_indentWidth);

    QLabel* tabWidthLabel = new QLabel(tr("Tab &Display Width:"));
    tabWidthLabel->setBuddy(d_tabWidth);

    d_backupInterval = new QSpinBox();
    d_backupInterval->setSuffix(tr(" seconds"));
    d_backupInterval->setSpecialValueText(tr("Disabled"));
    d_backupInterval->setSingleStep(5);

    QHBoxLayout* editorFontLayout = new QHBoxLayout();
    editorFontLayout->addWidget(d_editorFontComboBox, 1);
    editorFontLayout->addWidget(d_editorFontSizeComboBox);

    QGroupBox* appearanceGroup = new QGroupBox(tr("Appearance"));
    QFormLayout* appearanceLayout = new QFormLayout(appearanceGroup);
    appearanceLayout->addRow(editorFontLabel, editorFontLayout);
    appearanceLayout->addRow(tr("Show &Line Numbers:"), d_lineNumberStyle);
    appearanceLayout->addRow(new QLabel(), d_showControlChars);

    QGroupBox* indentationGroup = new QGroupBox(tr("Indentation"));
    QVBoxLayout* indentationLayout = new QVBoxLayout(indentationGroup);

    QFormLayout* indentationTopLayout = new QFormLayout();
    indentationTopLayout->addRow(tr("Automatic Indentation:"), d_indentMode);

    QGridLayout* indentationStyleLayout = new QGridLayout();
    indentationStyleLayout->setColumnStretch(0, 3);
    indentationStyleLayout->setColumnStretch(3, 1);

    indentationStyleLayout->addWidget(new QLabel(tr("Indent with:")), 0, 0);
    indentationStyleLayout->addWidget(d_indentWithSpaces, 1, 0);
    indentationStyleLayout->addWidget(d_indentWithTabs, 2, 0);

    indentationStyleLayout->addWidget(indentWidthLabel, 1, 2);
    indentationStyleLayout->addWidget(d_indentWidth, 1, 3);
    indentationStyleLayout->addWidget(tabWidthLabel, 2, 2);
    indentationStyleLayout->addWidget(d_tabWidth, 2, 3);

    indentationLayout->addLayout(indentationTopLayout);
    indentationLayout->addLayout(indentationStyleLayout);

    QGroupBox* autoBackupGroup = new QGroupBox(tr("Automatically backup unsaved changes"));

    QFormLayout* autoBackupLayout = new QFormLayout(autoBackupGroup);
    autoBackupLayout->addRow(tr("Backup interval:"), d_backupInterval);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(appearanceGroup);
    mainLayout->addWidget(indentationGroup);
    mainLayout->addWidget(autoBackupGroup);
}

void EditorSettingsTab::updateControlStates()
{
    d_indentWidth->setEnabled(!d_indentWithTabs->isChecked());
}

void EditorSettingsTab::updateFontSizes()
{
    QString currentFamily = d_editorFontComboBox->currentFont().family();

    QList<int> pointSizes = QFontDatabase::pointSizes(currentFamily);
    if (pointSizes.isEmpty()) {
        // Might be a style tucked into the family name
        qsizetype pos = currentFamily.lastIndexOf(QLatin1Char(' '));
        if (pos > 0) {
            pointSizes = QFontDatabase::pointSizes(currentFamily.sliced(0, pos).trimmed());
        }
    }

    if (pointSizes.isEmpty()) {
        // If still empty, just use defaults
        pointSizes = QFontDatabase::standardSizes();
    }

    QString currentSizeText = d_editorFontSizeComboBox->currentText();
    int currentSizeNewIndex = -1;

    QStringList values;
    values.reserve(pointSizes.size());
    for (int size : std::as_const(pointSizes)) {
        values.append(QString::number(size));

        if (values.last() == currentSizeText) {
            currentSizeNewIndex = values.size() - 1;
        }
    }

    d_editorFontSizeComboBox->clear();
    d_editorFontSizeComboBox->addItems(values);

    if (currentSizeNewIndex >= 0) {
        d_editorFontSizeComboBox->setCurrentIndex(currentSizeNewIndex);
    }
}

}
