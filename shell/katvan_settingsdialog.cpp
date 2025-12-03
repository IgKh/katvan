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
#include "katvan_settingsdialog.h"
#include "katvan_utils.h"

#include "typstdriver_packagemanager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QIdentityProxyModel>
#include <QLabel>
#include <QLibraryInfo>
#include <QListView>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStringListModel>
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

typstdriver::TypstCompilerSettings SettingsDialog::compilerSettings() const
{
    return d_compilerSettingsTab->settings();
}

void SettingsDialog::setCompilerSettings(const typstdriver::TypstCompilerSettings& settings)
{
    d_compilerSettingsTab->setSettings(settings);
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("Katvan Settings"));

    d_editorSettingsTab = new EditorSettingsTab();
    d_compilerSettingsTab = new CompilerSettingsTab();

    QTabWidget* tabWidget = new QTabWidget();
    tabWidget->addTab(d_editorSettingsTab, tr("&Editor"));
    tabWidget->addTab(d_compilerSettingsTab, tr("&Compiler"));

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
    settings.setColorScheme(d_colorScheme->currentData().toString());
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
    settings.setAutoBrackets(d_autoBrackets->isChecked());
    settings.setAutoTriggerCompletions(d_autoTriggerCompletions->isChecked());
    settings.setAutoBackupInterval(d_backupInterval->value());

    return settings;
}

void EditorSettingsTab::setSettings(const EditorSettings& settings)
{
    QFont font = settings.font();
    d_editorFontComboBox->setCurrentFont(font);
    d_editorFontSizeComboBox->setCurrentText(QString::number(font.pointSize()));

    d_colorScheme->setCurrentIndex(d_colorScheme->findData(settings.colorScheme()));

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
    d_autoBrackets->setChecked(settings.autoBrackets());
    d_autoTriggerCompletions->setChecked(settings.autoTriggerCompletions());
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

    d_colorScheme = new QComboBox();
    d_colorScheme->addItem(tr("Follow System Settings"), "auto");
    d_colorScheme->addItem(tr("Light"), "light");
    d_colorScheme->addItem(tr("Dark"), "dark");

    d_lineNumberStyle = new QComboBox();
    d_lineNumberStyle->addItem(tr("On Both Sides"), QVariant::fromValue(EditorSettings::LineNumberStyle::BOTH_SIDES));
    d_lineNumberStyle->addItem(tr("On Primary Side"), QVariant::fromValue(EditorSettings::LineNumberStyle::PRIMARY_ONLY));
    d_lineNumberStyle->addItem(tr("Don't Show"), QVariant::fromValue(EditorSettings::LineNumberStyle::NONE));

    d_showControlChars = new QCheckBox(tr("Show BiDi Control Characters"));
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

    d_autoBrackets = new QCheckBox(tr("Automatically insert &closing brackets"));
    d_autoTriggerCompletions = new QCheckBox(tr("Automatically show &autocomplete suggestions"));

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
    appearanceLayout->addRow(tr("C&olor Scheme:"), d_colorScheme);
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

    QGroupBox* behaviourGroup = new QGroupBox(tr("Behaviour"));
    QVBoxLayout* behaviourLayout = new QVBoxLayout(behaviourGroup);
    behaviourLayout->addWidget(d_autoBrackets);
    behaviourLayout->addWidget(d_autoTriggerCompletions);

    QGroupBox* autoBackupGroup = new QGroupBox(tr("Automatically Backup Unsaved Changes"));

    QFormLayout* autoBackupLayout = new QFormLayout(autoBackupGroup);
    autoBackupLayout->addRow(tr("&Backup Interval:"), d_backupInterval);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(appearanceGroup);
    mainLayout->addWidget(indentationGroup);
    mainLayout->addWidget(behaviourGroup);
    mainLayout->addWidget(autoBackupGroup);
    mainLayout->addStretch(1);
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

CompilerSettingsTab::CompilerSettingsTab(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void CompilerSettingsTab::setupUI()
{
    d_allowPreviewPackages = new QCheckBox(tr("&Allow download and use of @preview packages"));
    d_enableA11yExtras = new QCheckBox(tr("Enable experimental a&ccessibility features"));
    d_allowedPaths = new PathList();
    d_cacheSize = new QLabel();

    QPushButton* browseCacheButton = new QPushButton(tr("&Browse..."));
    browseCacheButton->setToolTip(tr("Show the download cache folder in a file browser"));
    connect(browseCacheButton, &QPushButton::clicked, this, &CompilerSettingsTab::browseCache);

    QGroupBox* flagsGroup = new QGroupBox(tr("Compiler Flags"));
    QVBoxLayout* flagsLayout = new QVBoxLayout(flagsGroup);

    flagsLayout->addWidget(d_allowPreviewPackages);
    flagsLayout->addWidget(d_enableA11yExtras);

    QGroupBox* allowedPathsGroup = new QGroupBox(tr("Allowed Paths"));
    QVBoxLayout* allowedPathsLayout = new QVBoxLayout(allowedPathsGroup);

    allowedPathsLayout->addWidget(new QLabel(tr("Allow including resources also from the following directories and their subdirectories:")));
    allowedPathsLayout->addWidget(d_allowedPaths, 1);

    QFormLayout* downloadCacheTopLayout = new QFormLayout();
    downloadCacheTopLayout->addRow(tr("Cache size: "), d_cacheSize);

    QHBoxLayout* downloadCacheButtonLayout = new QHBoxLayout();
    downloadCacheButtonLayout->addStretch(1);
    downloadCacheButtonLayout->addWidget(browseCacheButton);

    QGroupBox* downloadCacheGroup = new QGroupBox(tr("Download Cache"));
    QVBoxLayout* downloadCacheLayout = new QVBoxLayout(downloadCacheGroup);
    downloadCacheLayout->addLayout(downloadCacheTopLayout);
    downloadCacheLayout->addLayout(downloadCacheButtonLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(flagsGroup);
    mainLayout->addWidget(allowedPathsGroup, 1);
    mainLayout->addWidget(downloadCacheGroup);
}

typstdriver::TypstCompilerSettings CompilerSettingsTab::settings() const
{
    typstdriver::TypstCompilerSettings settings;
    settings.setAllowPreviewPackages(d_allowPreviewPackages->isChecked());
    settings.setEnableA11yExtras(d_enableA11yExtras->isChecked());
    settings.setAllowedPaths(d_allowedPaths->paths());

    return settings;
}

void CompilerSettingsTab::setSettings(const typstdriver::TypstCompilerSettings& settings)
{
    d_allowPreviewPackages->setChecked(settings.allowPreviewPackages());
    d_enableA11yExtras->setChecked(settings.enableA11yExtras());
    d_allowedPaths->setPaths(settings.allowedPaths());
}

void CompilerSettingsTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateCacheSize();
}

void CompilerSettingsTab::updateCacheSize()
{
    auto stats = typstdriver::PackageManager::cacheStatistics();

    QString text = tr("%2 distinct versions of %1 packages (total %3)").arg(
        QString::number(stats.numPackages),
        QString::number(stats.numPackageVersions),
        QLocale().formattedDataSize(stats.totalSize));

    d_cacheSize->setText(text);
}

void CompilerSettingsTab::browseCache()
{
    QUrl url = QUrl::fromLocalFile(typstdriver::PackageManager::downloadCacheDirectory());
    QDesktopServices::openUrl(url);
}

class PathProxyModel : public QIdentityProxyModel
{
public:
    PathProxyModel(QObject* parent = nullptr): QIdentityProxyModel(parent)
    {
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DisplayRole) {
            QModelIndex sourceIndex = mapToSource(index);
            QString origPath = sourceModel()->data(sourceIndex, Qt::DisplayRole).toString();
            return utils::formatFilePath(std::move(origPath));
        }
        else if (role == Qt::ToolTipRole) {
            QModelIndex sourceIndex = mapToSource(index);
            return sourceModel()->data(sourceIndex, Qt::DisplayRole).toString();
        }
        return QIdentityProxyModel::data(index, role);
    }
};

PathList::PathList(QWidget* parent)
    : QWidget(parent)
{
    d_pathsModel = new QStringListModel(this);

    PathProxyModel* proxyModel = new PathProxyModel();
    proxyModel->setSourceModel(d_pathsModel);

    d_pathsListView = new QListView();
    d_pathsListView->setModel(proxyModel);
    d_pathsListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(d_pathsListView->selectionModel(), &QItemSelectionModel::currentChanged, this, &PathList::currentPathChanged);

    QPushButton* addPathButton = new QPushButton();
    addPathButton->setIcon(utils::themeIcon("list-add"));
    addPathButton->setToolTip(tr("Add a path"));
    connect(addPathButton, &QPushButton::clicked, this, &PathList::addPath);

    d_removePathButton = new QPushButton();
    d_removePathButton->setIcon(utils::themeIcon("list-remove"));
    d_removePathButton->setToolTip(tr("Remove the selected path from the list"));
    d_removePathButton->setEnabled(false);
    connect(d_removePathButton, &QPushButton::clicked, this, &PathList::removePath);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(addPathButton);
    buttonLayout->addWidget(d_removePathButton);

    layout->addWidget(d_pathsListView, 1);
    layout->addLayout(buttonLayout);
}

QStringList PathList::paths() const
{
    return d_pathsModel->stringList();
}

void PathList::setPaths(const QStringList& paths)
{
    d_pathsModel->setStringList(paths);
}

void PathList::addPath()
{
    QString dirName = QFileDialog::getExistingDirectory(window(), tr("Select Directory"));
    if (dirName.isEmpty()) {
        return;
    }

    QStringList values = d_pathsModel->stringList();
    if (!values.contains(dirName)) {
        values.append(dirName);
        d_pathsModel->setStringList(values);
    }
}

void PathList::removePath()
{
    QModelIndex current = d_pathsListView->currentIndex();
    if (!current.isValid()) {
        return;
    }
    d_pathsModel->removeRow(current.row());
}

void PathList::currentPathChanged()
{
    d_removePathButton->setEnabled(d_pathsListView->currentIndex().isValid());
}

}

#include "moc_katvan_settingsdialog.cpp"
