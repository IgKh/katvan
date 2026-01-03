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
#include "katvan_utils.h"

#if defined(Q_OS_MACOS)
#include "katvan_utils_macos.h"
#endif

#include "katvan_text_utils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>

#if defined(KATVAN_FLATPAK_BUILD)
#include <sys/xattr.h>
#endif

namespace katvan::utils {

QString getApplicationDir(bool& isInstalled)
{
#if defined(Q_OS_MACOS)
    QString path = macos::getApplicationDir();
    isInstalled = (
        path == "/Applications" ||
        path == QDir::homePath() + "/Applications");

    return path;
#else
    isInstalled = false;
    return QCoreApplication::applicationDirPath();
#endif
}

QIcon themeIcon(const char* xdgIcon)
{
    return QIcon::fromTheme(QLatin1String(xdgIcon));
}

QIcon themeIcon(const char* xdgIcon, const char* macIcon)
{
#if !defined(Q_OS_MACOS)
    Q_UNUSED(macIcon)
    return themeIcon(xdgIcon);
#else
    QIcon icon = QIcon::fromTheme(QLatin1String(macIcon));
    if (icon.isNull()) {
        icon = themeIcon(xdgIcon);
    }
    return icon;
#endif
}

QString getHostPath(QString path)
{
#if !defined(KATVAN_FLATPAK_BUILD)
    return path;
#else
    static constexpr char XATTR_HOST_PATH[] = "user.document-portal.host-path";

    ssize_t size = getxattr(qPrintable(path), XATTR_HOST_PATH, nullptr, 0);
    if (size < 0) {
        return path;
    }

    std::vector<char> buff;
    buff.resize(size);

    ssize_t rc = getxattr(qPrintable(path), XATTR_HOST_PATH, buff.data(), size);
    if (rc < 0) {
        return path;
    }

    return QString::fromLocal8Bit(buff.data(), size);
#endif
}


QString formatFilePath(QString path)
{
    // If behind an XDG document portal, use the host file path
    path = getHostPath(std::move(path));
    path = QDir::toNativeSeparators(path);

    if (qGuiApp->layoutDirection() == Qt::RightToLeft) {
        return LRI_MARK + path + PDI_MARK;
    }
    return path;
}

QString showPdfExportDialog(QWidget* parent, const QString& sourceFilePath)
{
#if defined(Q_OS_MACOS)
    return macos::showPdfExportDialog(parent, sourceFilePath);
#else
    QFileDialog dialog(parent, QObject::tr("Export to PDF"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(QObject::tr("PDF files (*.pdf)"));
    dialog.setDefaultSuffix("pdf");

    if (!sourceFilePath.isEmpty()) {
        QFileInfo info(sourceFilePath);
        dialog.setDirectory(info.dir());
        dialog.selectFile(info.baseName() + ".pdf");
    }

    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }
    return dialog.selectedFiles().at(0);
#endif
}

}
