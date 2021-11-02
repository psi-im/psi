/*
 * psithemeprovider.cpp - kinda adapter for set of themes
 * Copyright (C) 2010-2017  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "psithemeprovider.h"

#include "applicationinfo.h"
#include "psicon.h"

#include <QFileInfo>
#include <QStringList>

PsiThemeProvider::PsiThemeProvider(PsiCon *parent) : QObject(parent), _psi(parent) { }

QString PsiThemeProvider::themePath(const QString &name)
{
    const QStringList dirs
        = { ":", ".", ApplicationInfo::homeDir(ApplicationInfo::DataLocation), ApplicationInfo::resourcesDir() };

    for (const QString &dir : dirs) {
        QString fileName = dir + "/themes/" + name;

        QFileInfo fi(fileName);
        if (fi.exists())
            return fileName;
    }

    qWarning("PsiThemeManager::Private::themePath(\"%s\"): not found", qPrintable(name));
    return QString();
}

// says where theme is able to load in separate thread
bool PsiThemeProvider::threadedLoading() const { return false; }

int PsiThemeProvider::screenshotWidth() const { return 0; }
