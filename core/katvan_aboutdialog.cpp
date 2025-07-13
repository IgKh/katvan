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
#include "katvan_aboutdialog.h"
#include "katvan_typstdriverwrapper.h"

#include <QFile>
#include <QGuiApplication>
#include <QOperatingSystemVersion>
#include <QTextStream>

namespace katvan {

#ifdef Q_OS_LINUX

static QByteArray getLinuxRelease()
{
    QFile f { "/etc/os-release" };
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QByteArray();
    }

    QByteArray prefix = QByteArrayLiteral("PRETTY_NAME=");

    while (!f.atEnd()) {
        QByteArray line = f.readLine().trimmed();

        if (line.startsWith(prefix)) {
            // Strip prefix and quotes
            return line.sliced(prefix.size() + 1, line.size() - prefix.size() - 2);
        }
    }
    return QByteArray();
}

static QByteArray getLinuxDesktop()
{
    QByteArray result = qgetenv("XDG_CURRENT_DESKTOP");
    if (!result.isEmpty()) {
        return result.split(';').first().trimmed();
    }

    return qgetenv("XDG_SESSION_DESKTOP");
}

static bool isOnWayland()
{
    auto* iface = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    return iface != nullptr;
}

#endif

static QString operatingSystemDescription()
{
#ifndef Q_OS_LINUX
    QOperatingSystemVersion os = QOperatingSystemVersion::current();
    return QString("%1 %2").arg(os.name(), os.version().toString());
#else
    QByteArray release = getLinuxRelease();
    if (release.isEmpty()) {
        return QStringLiteral("<unknown>");
    }

    QString result = QString::fromLatin1(release);
    QStringList details;

    QByteArray desktop = getLinuxDesktop();
    if (!desktop.isEmpty()) {
        details.append(QString::fromLatin1(desktop));
    }
    if (isOnWayland()) {
        details.append(QStringLiteral("Wayland"));
    }

    if (!details.isEmpty()) {
        result += QStringLiteral(" (%1)").arg(details.join("; "));
    }
    return result;
#endif
}

static QString acknowledgements()
{
    QFile file(":/assets/ACKNOWLEDGEMENTS.txt");
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << file.errorString();
        return QString();
    }

    QTextStream stream(&file);
    return stream.readAll().trimmed();
}

AboutDialog::AboutDialog(QWidget* parent)
    : QMessageBox(parent)
{
    QString mainText = tr(
        "<h3>Katvan</h3>"
        "<a href=\"%1\">%1</a>"
        "<p>A bare-bones editor for <i>Typst</i> files, with a bias for RTL</p>"
        "<p>Version %2 (Qt %3; Typst %4) on %5</p>"
    )
    .arg(
        QStringLiteral("https://katvan.app"),
        QCoreApplication::applicationVersion(),
        QLatin1String(qVersion()),
        TypstDriverWrapper::typstVersion(),
        operatingSystemDescription());

    QString informativeText = tr(
        "<p>Katvan is offered under the terms of the <a href=\"%1\">GNU General Public License Version 3</a>. "
        "Includes third party assets, see more details below.</p>"
    )
    .arg(QStringLiteral("https://www.gnu.org/licenses/gpl-3.0.en.html"));

    setWindowTitle(tr("About Katvan"));
    setIconPixmap(QIcon(":/assets/katvan.svg").pixmap(QSize(128, 128)));
    setStandardButtons(QMessageBox::Ok);
    setText(mainText);
    setInformativeText(informativeText);
    setDetailedText(acknowledgements());
}

}

#include "moc_katvan_aboutdialog.cpp"
