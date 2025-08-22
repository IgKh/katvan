// -*- mode: objective-cpp -*-
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
#include <QUtiMimeConverter>

class ColorUtiMimeConverter : public QUtiMimeConverter
{
public:
    QString mimeForUti(const QString& uti) const override;
    QString utiForMime(const QString& mime) const override;
    QVariant convertToMime(const QString& mime, const QList<QByteArray>& data, const QString& uti) const override;
    QList<QByteArray> convertFromMime(const QString& mime, const QVariant& data, const QString& uti) const override;
};
