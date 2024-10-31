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
#include "katvan_text_utils.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>

namespace katvan::utils {

bool isBidiControlChar(QChar ch)
{
    return ch == ALM_MARK
        || ch == LRM_MARK
        || ch == RLM_MARK
        || ch == LRI_MARK
        || ch == RLI_MARK
        || ch == FSI_MARK
        || ch == PDI_MARK;
}

Qt::LayoutDirection naturalTextDirection(const QString& text)
{
    int count = 0;
    const QChar* ptr = text.constData();
    const QChar* end = ptr + text.length();

    int isolateLevel = 0;

    while (ptr < end) {
        if (count++ > 100) {
            break;
        }

        if (*ptr == LRI_MARK || *ptr == RLI_MARK || *ptr == FSI_MARK) {
            isolateLevel++;
            ptr++;
            continue;
        }
        else if (*ptr == PDI_MARK) {
            isolateLevel = qMax(0, isolateLevel - 1);
            ptr++;
            continue;
        }

        uint codepoint = ptr->unicode();
        if (QChar::isHighSurrogate(codepoint) && ptr + 1 < end) {
            ushort low = ptr[1].unicode();
            if (QChar::isLowSurrogate(low)) {
                codepoint = QChar::surrogateToUcs4(codepoint, low);
                ptr++;
            }
        }

        if (isolateLevel == 0) {
            QChar::Direction direction = QChar::direction(codepoint);
            if (direction == QChar::DirR || direction == QChar::DirAL) {
                return Qt::RightToLeft;
            }
            else if (direction == QChar::DirL) {
                return Qt::LeftToRight;
            }
        }
        ptr++;
    }
    return Qt::LayoutDirectionAuto;
}

class FontIconEngine : public QIconEngine
{
public:
    FontIconEngine(QChar ch, QFont font) : d_char(ch), d_font(font) {}

    QIconEngine* clone() const override;
    bool isNull() override;
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;

private:
    QChar d_char;
    QFont d_font;
};

QIconEngine* FontIconEngine::clone() const
{
    return new FontIconEngine(d_char, d_font);
}

bool FontIconEngine::isNull()
{
    QFontMetrics fm { d_font };
    return !fm.inFont(d_char);
}

void FontIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(state)

    QPalette::ColorGroup cg = QPalette::Normal;
    switch (mode) {
        case QIcon::Normal:     cg = QPalette::Normal; break;
        case QIcon::Disabled:   cg = QPalette::Disabled; break;
        case QIcon::Active:     cg = QPalette::Active; break;
        case QIcon::Selected:   cg = QPalette::Current; break;
    }

    QFont font = d_font;
    font.setPixelSize(rect.height());

    QTextOption option;
    option.setAlignment(Qt::AlignCenter);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    option.setFlags(QTextOption::ShowDefaultIgnorables);
#endif

    painter->save();
    painter->setFont(font);
    painter->setPen(QApplication::palette().color(cg, QPalette::ButtonText));
    painter->drawText(rect, QString(d_char), option);
    painter->restore();
}

QPixmap FontIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    QRect rect { QPoint(0, 0), size };
    paint(&painter, rect, mode, state);

    return pixmap;
}

QIcon fontIcon(QChar ch)
{
    return fontIcon(ch, QFont());
}

QIcon fontIcon(QChar ch, const QFont& font)
{
    return QIcon(new FontIconEngine(ch, font));
}

}
