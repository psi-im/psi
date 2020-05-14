/*
 * gpgprocess.cpp - QProcess wrapper makes it easy to handle gpg
 *
 * Copyright (C) 2013 Ivan Romanov <drizt@land.ru>
 * Copyright (C) 2020 Boris Pek <tehnick-8@yandex.ru>
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

#include "gpgprocess.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

GpgProcess::GpgProcess(QObject *parent) : QProcess(parent)
{
    m_bin = findBin();
}

void GpgProcess::start(const QStringList &arguments, QIODevice::OpenMode mode)
{
    QProcess::start(m_bin, arguments, mode);
}

void GpgProcess::start(QIODevice::OpenMode mode)
{
    QProcess::start(m_bin, mode);
}

bool GpgProcess::success() const
{
    return (exitCode() == 0);
}

inline bool checkBin(const QString &bin)
{
    QFileInfo fi(bin);
    return fi.exists();
}

#ifdef Q_OS_WIN
static bool getRegKey(HKEY root, const char *path, QString &value)
{
    HKEY hkey = 0;
    bool res  = false;

    if (RegOpenKeyExA(root, path, 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
        DWORD dwLen = 256;
        char  szValue[256];

        if (RegQueryValueExA(hkey, "Install Directory", NULL, NULL, (LPBYTE)szValue, &dwLen) == ERROR_SUCCESS) {
            value = QString::fromLocal8Bit(szValue);
            res   = true;
        }
        RegCloseKey(hkey);
    }
    return res;
}

static QString findRegGpgProgram()
{
    QStringList bins { "gpg.exe", "gpg2.exe" };

    const char *path  = "Software\\GNU\\GnuPG";
    const char *path2 = "Software\\Wow6432Node\\GNU\\GnuPG";

    QString dir;
    getRegKey(HKEY_CURRENT_USER, path, dir) || getRegKey(HKEY_CURRENT_USER, path2, dir)
        || getRegKey(HKEY_LOCAL_MACHINE, path, dir) || getRegKey(HKEY_LOCAL_MACHINE, path2, dir);

    if (!dir.isEmpty()) {
        for (const QString &bin : bins) {
            if (checkBin(dir + "\\" + bin)) {
                return dir + "\\" + bin;
            }
        }
    }
    return QString();
}
#endif

QString GpgProcess::findBin() const
{
    // gpg and gpg2 has identical semantics
    // so any from them can be used
#ifdef Q_OS_WIN
    QStringList bins { "gpg.exe", "gpg2.exe" };
#else
    QStringList bins { "gpg", "gpg2" };
#endif

    // Prefer bundled gpg
    for (const QString &bin : bins) {
        if (checkBin(QCoreApplication::applicationDirPath() + "/" + bin)) {
            return QCoreApplication::applicationDirPath() + "/" + bin;
        }
    }

#ifdef Q_OS_WIN
    // On Windows look up at registry
    QString bin = findRegGpgProgram();
    if (!bin.isEmpty())
        return bin;
#endif

        // Look up at PATH environment
#ifdef Q_OS_WIN
    QString pathSep = ";";
#else
    QString     pathSep = ":";
#endif

    QStringList paths = QString::fromLocal8Bit(qgetenv("PATH")).split(pathSep, QString::SkipEmptyParts);

#ifdef Q_OS_MAC
    // On Mac OS bundled always uses system default PATH
    // so it need explicitly add extra paths which can
    // contain gpg
    // Mac GPG and brew use /usr/local/bin
    // MacPorts uses /opt/local/bin
    paths << "/usr/local/bin"
          << "/opt/local/bin";
#endif
    paths.removeDuplicates();

    for (const QString &path : paths) {
        for (const QString &bin : bins) {
            if (checkBin(path + "/" + bin)) {
                return path + "/" + bin;
            }
        }
    }

    // Return nothing if gpg not found
    return QString();
}

bool GpgProcess::info(QString &message)
{
    const QStringList &&arguments = { "--version", "--no-tty" };
    start(arguments);
    waitForFinished();

    bool res = false;

    if (!m_bin.isEmpty()) {
        if (error() == FailedToStart) {
            message = tr("Can't start ") + m_bin;
        } else {
            message = QString("%1 %2\n%3")
                          .arg(QDir::toNativeSeparators(m_bin))
                          .arg(arguments.join(" "))
                          .arg(QString::fromLocal8Bit(readAll()));
            res = true;
        }
    } else {
        message = tr("GnuPG program not found");
    }

    return res;
}
