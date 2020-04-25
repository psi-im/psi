/*
 * fileutil.h - common file dialogs
 * Copyright (C) 2008  Michail Pishchagin
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

#include "fileutil.h"

#include "psioptions.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>

#include <sys/types.h>
#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

static QString lastUsedOpenPathOptionPath = "options.ui.last-used-open-path";
static QString lastUsedSavePathOptionPath = "options.ui.last-used-save-path";

QString FileUtil::lastUsedOpenPath()
{
    return PsiOptions::instance()->getOption(lastUsedOpenPathOptionPath).toString();
}

void FileUtil::setLastUsedOpenPath(const QString &path)
{
    QFileInfo fi(path);
    if (fi.exists()) {
        PsiOptions::instance()->setOption(lastUsedOpenPathOptionPath, path);
    }
}

QString FileUtil::lastUsedSavePath()
{
    return PsiOptions::instance()->getOption(lastUsedSavePathOptionPath).toString();
}

void FileUtil::setLastUsedSavePath(const QString &path)
{
    QFileInfo fi(path);
    if (fi.exists()) {
        PsiOptions::instance()->setOption(lastUsedSavePathOptionPath, path);
    }
}

QString FileUtil::getOpenFileName(QWidget *parent, const QString &caption, const QString &filter,
                                  QString *selectedFilter)
{
    while (true) {
        if (lastUsedOpenPath().isEmpty()) {
            setLastUsedOpenPath(QDir::homePath());
        }
        QString fileName = QFileDialog::getOpenFileName(parent, caption, lastUsedOpenPath(), filter, selectedFilter);
        if (!fileName.isEmpty()) {
            QFileInfo fi(fileName);
            if (!fi.exists()) {
                QMessageBox::information(parent, tr("Error"), tr("The file specified does not exist."));
                continue;
            }

            setLastUsedOpenPath(fi.path());
            return fileName;
        }
        break;
    }

    return QString();
}

QStringList FileUtil::getOpenFileNames(QWidget *parent, const QString &caption, const QString &filter,
                                       QString *selectedFilter)
{
    while (true) {
        if (lastUsedOpenPath().isEmpty()) {
            setLastUsedOpenPath(QDir::homePath());
        }
        QStringList result;
        QStringList fileNames
            = QFileDialog::getOpenFileNames(parent, caption, lastUsedOpenPath(), filter, selectedFilter);
        for (const QString &fileName : fileNames) {
            QFileInfo fi(fileName);
            if (!fi.exists()) {
                QMessageBox::information(parent, tr("Error"), tr("The file specified does not exist."));
                continue;
            }
            result << fileName;
            setLastUsedOpenPath(fi.path());
        }
        return result;
    }
}

QString FileUtil::getSaveFileName(QWidget *parent, const QString &caption, const QString &defaultFileName,
                                  const QString &filter, QString *selectedFilter)
{
    if (lastUsedSavePath().isEmpty()) {
        if (!lastUsedOpenPath().isEmpty()) {
            setLastUsedSavePath(lastUsedOpenPath());
        } else {
            setLastUsedSavePath(QDir::homePath());
        }
    }

    QString dir      = QDir(lastUsedSavePath()).filePath(defaultFileName);
    QString fileName = QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter);
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        if (QDir(fi.path()).exists()) {
            setLastUsedSavePath(fi.path());
            return fileName;
        }
    }

    return QString();
}

QString FileUtil::getSaveDirName(QWidget *parent, const QString &caption)
{
    if (lastUsedSavePath().isEmpty()) {
        if (!lastUsedOpenPath().isEmpty()) {
            setLastUsedSavePath(lastUsedOpenPath());
        } else {
            setLastUsedSavePath(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        }
    }

    QString dirName = QFileDialog::getExistingDirectory(parent, caption, lastUsedSavePath());

    if (!dirName.isEmpty()) {
        if (QDir(dirName).exists()) {
            setLastUsedSavePath(dirName);
            return dirName;
        }
    }

    return QString();
}

QString FileUtil::getImageFileName(QWidget *parent, QString caption)
{
    // double extenstions because of QTBUG-51712
    return FileUtil::getOpenFileName(parent, caption.isEmpty() ? tr("Choose a file") : caption,
                                     tr("Images (*.png *.xpm *.jpg *.jpeg *.webp *.PNG *.XPM *.JPG *.JPEG *.WEBP)"));
}

QString FileUtil::getInbandImageFileName(QWidget *parent)
{
    return getImageFileName(parent, QString("%1 (<50KB)").arg(tr("Choose a file")));
}

// see also image2type in xmpp_vcard.cpp
QString FileUtil::mimeToFileExt(const QString &mime)
{
    static QMap<QString, QString> mimes;
    if (!mimes.size()) {
        mimes["image/png"]     = QLatin1String("png");
        mimes["image/x-mng"]   = QLatin1String("mng");
        mimes["image/gif"]     = QLatin1String("gif");
        mimes["image/bmp"]     = QLatin1String("bmp");
        mimes["image/x-xpm"]   = QLatin1String("xpm");
        mimes["image/svg+xml"] = QLatin1String("svg");
        mimes["image/jpeg"]    = QLatin1String("jpg");
        mimes["image/webp"]    = QLatin1String("webp");

        mimes["application/octet-stream"] = "bin";
        // add more when needed
    }
    return mimes.value(mime);
}

QString FileUtil::cleanFileName(const QString &s)
{
    //#ifdef Q_OS_WIN
    QString badchars = "\\/|?*:\"<>";
    QString str;
    str.reserve(s.size());
    for (int n = 0; n < s.length(); ++n) {
        auto c = s.at(n);
        if (badchars.indexOf(c) != -1)
            continue;
        str += c;
    }
    if (str.isEmpty())
        str = "unnamed";
    return str;
    //#else
    //    return s;
    //#endif
}

void FileUtil::openFolder(const QString &path)
{
#if defined(Q_OS_WIN)
    QProcess::startDetached("explorer.exe",
                            QStringList() << QLatin1String("/select,") << QDir::toNativeSeparators(path));
#elif defined(Q_OS_MAC)
    QProcess::execute("/usr/bin/osascript",
                      QStringList() << "-e"
                                    << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(path));
    QProcess::execute("/usr/bin/osascript",
                      QStringList() << "-e"
                                    << "tell application \"Finder\" to activate");
#else
    // we cannot select a file here, because no file browser really supports it...
    const QFileInfo fileInfo(path);
    QProcess::startDetached("xdg-open", QStringList(fileInfo.path()));
#endif
    // printf("item open dest: [%s]\n", path.latin1());
}

void FileUtil::setModificationTime(const QString &filename, const QDateTime &mtime)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    qint64 secs = mtime.toSecsSinceEpoch();
#else
    qint64  secs = mtime.toTime_t();
#endif
#ifdef Q_OS_WIN
    _utimbuf t;
    t.actime  = secs;
    t.modtime = secs;
    _wutime(filename.toStdWString().c_str(), &t);
#else
    utimbuf t;
    t.actime  = secs;
    t.modtime = secs;
    utime(filename.toLocal8Bit().data(), &t);
#endif
}
