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
#include "katvan_text_utils.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>

#include <algorithm>

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

bool isSingleBidiMark(QChar ch)
{
    return ch == utils::LRM_MARK
        || ch == utils::RLM_MARK
        || ch == utils::ALM_MARK;
}

bool isWhitespace(QChar ch)
{
    return ch.category() == QChar::Separator_Space || ch == QLatin1Char('\t') || isSingleBidiMark(ch);
}

bool isAllWhitespace(const QString& text)
{
    return std::all_of(text.begin(), text.end(), isWhitespace);
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

char32_t firstCodepointOf(const QString& str)
{
    if (str.isEmpty()) {
        return 0;
    }

    ushort codepoint = str[0].unicode();

    if (QChar::isHighSurrogate(codepoint) && str.size() > 1) {
        ushort low = str[1].unicode();
        if (QChar::isLowSurrogate(low)) {
            return QChar::surrogateToUcs4(codepoint, low);
        }
    }
    return codepoint;
}

void loadAuxiliaryFonts()
{
    int rc = QFontDatabase::addApplicationFont(":/assets/KatvanControl.otf");
    if (rc < 0) {
        qWarning() << "Failed to load control character font";
    }

    rc = QFontDatabase::addApplicationFont(":/assets/fonts/AdobeBlank.otf");
    if (rc < 0) {
        qWarning() << "Failed to load blank font";
    }

    rc = QFontDatabase::addApplicationFont(":/assets/fonts/NotoSansMath-Regular.otf");
    if (rc < 0) {
        qWarning() << "Failed to load math symbol font";
    }
}

class FontIconEngine : public QIconEngine
{
public:
    FontIconEngine(char32_t codepoint, QFont font)
        : d_valid(QFontMetrics(font).inFontUcs4(codepoint))
        , d_codepoint(codepoint)
        , d_font(font) {}

    QIconEngine* clone() const override;
    bool isNull() override;
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;

private:
    bool d_valid;
    char32_t d_codepoint;
    QFont d_font;
};

QIconEngine* FontIconEngine::clone() const
{
    return new FontIconEngine(d_codepoint, d_font);
}

bool FontIconEngine::isNull()
{
    return !d_valid;
}

void FontIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(state)

    if (!d_valid) {
        return;
    }

    QPalette::ColorGroup cg = QPalette::Normal;
    switch (mode) {
        case QIcon::Normal:     cg = QPalette::Normal; break;
        case QIcon::Disabled:   cg = QPalette::Disabled; break;
        case QIcon::Active:     cg = QPalette::Active; break;
        case QIcon::Selected:   cg = QPalette::Current; break;
    }

    QFont font = d_font;
    font.setPixelSize(rect.height());

    QString text = QString::fromUcs4(&d_codepoint, 1);

    QFontMetrics fm { font };
    QRect actualRect = fm.boundingRect(rect, Qt::AlignCenter, text);
    if (actualRect.width() > 0 && !rect.contains(actualRect)) {
        // If due to font features, the character will overflow the paint rect,
        // scale it down...
        qreal xoverflow = qMax(static_cast<qreal>(actualRect.width()) / rect.width(), 1.0);
        qreal yoverflow = qMax(static_cast<qreal>(actualRect.height()) / rect.height(), 1.0);

        int size = qRound(rect.height() / qMax(xoverflow, yoverflow));
        font.setPixelSize(size);
    }

    QTextOption option;
    option.setAlignment(Qt::AlignCenter);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    option.setFlags(QTextOption::ShowDefaultIgnorables);
#endif

    painter->save();
    painter->setFont(font);
    painter->setPen(QApplication::palette().color(cg, QPalette::ButtonText));
    painter->drawText(rect, text, option);
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
    return fontIcon(ch.unicode(), QFont());
}

QIcon fontIcon(QChar ch, const QFont& font)
{
    return fontIcon(ch.unicode(), font);
}

QIcon fontIcon(char32_t codepoint, const QFont& font)
{
    return QIcon(new FontIconEngine(codepoint, font));
}

}
