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
#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <map>
#include <memory>

class Hunspell;

namespace katvan {

class SpellChecker : public QObject
{
    Q_OBJECT

public:
    SpellChecker(QObject* parent = nullptr);
    ~SpellChecker();

    static QMap<QString, QString> findDictionaries();
    static QString dictionaryDisplayName(const QString& dictName);

    QString currentDictionaryName() const { return d_currentDictName; }
    void setCurrentDictionary(const QString& dictName, const QString& dictAffFile);

    QList<std::pair<size_t, size_t>> checkSpelling(const QString& text);

private:
    QString d_currentDictName;
    std::map<QString, std::unique_ptr<Hunspell>> d_spellers;
};

}
