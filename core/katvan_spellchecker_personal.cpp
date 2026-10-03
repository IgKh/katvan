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
#include "katvan_spellchecker_personal.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace katvan {

QString PersonalDictionary::s_dictionaryLocation;

PersonalDictionary::PersonalDictionary(QObject* parent)
    : QObject(parent)
{
    d_watcher = new QFileSystemWatcher(this);
    connect(d_watcher, &QFileSystemWatcher::fileChanged, this, &PersonalDictionary::dictionaryFileChanged);

    setDictionaryPath();
}

void PersonalDictionary::setDictionaryLocation(const QString& dirPath)
{
    s_dictionaryLocation = dirPath;
}

bool PersonalDictionary::isWordInDictionary(const QString& word) const
{
    return d_words.contains(word.normalized(QString::NormalizationForm_D));
}

bool PersonalDictionary::isNormalizedWordInDictionary(const QString& normalizedWord) const
{
    return d_words.contains(normalizedWord);
}

void PersonalDictionary::addToDictionary(const QString& word)
{
    d_words.insert(word.normalized(QString::NormalizationForm_D));
    flushDictionary();
}

void PersonalDictionary::flushDictionary()
{
    QDir dictDir = QFileInfo(d_path).dir();
    if (!dictDir.exists()) {
        dictDir.mkpath(".");
    }

    QSaveFile file(d_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(
            QApplication::activeWindow(),
            QCoreApplication::applicationName(),
            tr("Saving personal dictionary to %1 failed: %2").arg(d_path, file.errorString()));

        return;
    }

    QTextStream stream(&file);
    for (const QString& word : std::as_const(d_words)) {
        stream << word << "\n";
    }
    file.commit();
}

void PersonalDictionary::loadDictionary()
{
    if (!QFileInfo::exists(d_path)) {
        return;
    }

    QFile file(d_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(
            QApplication::activeWindow(),
            QCoreApplication::applicationName(),
            tr("Loading personal dictionary from %1 failed: %2").arg(d_path, file.errorString()));

        return;
    }

    d_words.clear();

    QString line;
    QTextStream stream(&file);
    while (stream.readLineInto(&line)) {
        if (line.isEmpty()) {
            continue;
        }
        d_words.insert(line.normalized(QString::NormalizationForm_D));
    }
}

void PersonalDictionary::setDictionaryPath()
{
    QString loc = s_dictionaryLocation;
    if (loc.isEmpty()) {
        loc = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    d_path = loc + QDir::separator() + "personal.dic";

    loadDictionary();

    const QStringList watchedFiles = d_watcher->files();
    for (const QString& file : watchedFiles) {
        d_watcher->removePath(file);
    }
    d_watcher->addPath(d_path);
}

void PersonalDictionary::dictionaryFileChanged()
{
    qDebug() << "Personal dictionary file changed on disk";
    loadDictionary();

    if (!d_watcher->files().contains(d_path)) {
        d_watcher->addPath(d_path);
    }
}

}

#include "moc_katvan_spellchecker_personal.cpp"
