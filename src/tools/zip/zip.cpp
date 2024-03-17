/*
 * zip.cpp - zip/unzip files
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "zip.h"

#ifdef PSIMINIZIP
#include "minizip/unzip.h"
#else
#include <unzip.h>
#endif

#include <QFile>
#include <QString>
#include <QStringList>

class UnZipPrivate {
public:
    UnZipPrivate() : caseSensitive(UnZip::CS_Default) { }

    QString                name;
    unzFile                uf;
    QStringList            listing;
    UnZip::CaseSensitivity caseSensitive;
};

UnZip::UnZip(const QString &name)
{
    d       = new UnZipPrivate;
    d->uf   = nullptr;
    d->name = name;
}

UnZip::UnZip(UnZip &&o) : d(o.d) { o.d = nullptr; }

UnZip::~UnZip()
{
    if (d) {
        close();
        delete d;
    }
}

void UnZip::setCaseSensitivity(CaseSensitivity state) { d->caseSensitive = state; }

void UnZip::setName(const QString &name) { d->name = name; }

const QString &UnZip::name() const { return d->name; }

bool UnZip::open()
{
    close();

    d->uf = unzOpen(QFile::encodeName(d->name).data());
    if (!d->uf)
        return false;

    return getList();
}

void UnZip::close()
{
    if (d->uf) {
        unzClose(d->uf);
        d->uf = nullptr;
    }

    d->listing.clear();
}

QHash<QString, QByteArray> UnZip::unpackAll()
{
    close();
    QHash<QString, QByteArray> ret;
    d->uf = unzOpen(QFile::encodeName(d->name).data());
    if (!d->uf)
        return ret;

    unz_global_info gi;
    int             err = unzGetGlobalInfo(d->uf, &gi);
    if (err != UNZ_OK) {
        close();
        return ret;
    }

    for (int n = 0; n < (int)gi.number_entry; ++n) {
        char          filename_inzip[256];
        unz_file_info file_info;
        int           err
            = unzGetCurrentFileInfo(d->uf, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
        if (err != UNZ_OK)
            break;

        QString    filename = QString::fromUtf8(filename_inzip);
        QByteArray data;
        if (!readCurrentFile(&data))
            break;

        ret.insert(filename, data);

        if ((n + 1) < (int)gi.number_entry) {
            err = unzGoToNextFile(d->uf);
            if (err != UNZ_OK)
                break;
        }
    }

    close();
    return ret;
}

const QStringList &UnZip::list() const { return d->listing; }

bool UnZip::getList()
{
    unz_global_info gi;
    int             err = unzGetGlobalInfo(d->uf, &gi);
    if (err != UNZ_OK)
        return false;

    QStringList l;
    for (int n = 0; n < (int)gi.number_entry; ++n) {
        char          filename_inzip[256];
        unz_file_info file_info;
        int           err
            = unzGetCurrentFileInfo(d->uf, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
        if (err != UNZ_OK)
            return false;

        l += filename_inzip;

        if ((n + 1) < (int)gi.number_entry) {
            err = unzGoToNextFile(d->uf);
            if (err != UNZ_OK)
                return false;
        }
    }

    d->listing = l;

    return true;
}

bool UnZip::readCurrentFile(QByteArray *buf, int max)
{
    int err = unzOpenCurrentFile(d->uf);
    if (err != UNZ_OK)
        return false;

    QByteArray a;
    QByteArray chunk;
    chunk.resize(16384);
    while (1) {
        err = unzReadCurrentFile(d->uf, chunk.data(), chunk.size());
        if (err < 0) {
            unzCloseCurrentFile(d->uf);
            return false;
        }
        if (err == 0)
            break;

        int oldsize = a.size();
        if (max > 0 && oldsize + err > max)
            err = max - oldsize;
        a.resize(oldsize + err);
        memcpy(a.data() + oldsize, chunk.data(), err);

        if (max > 0 && (int)a.size() >= max)
            break;
    }

    err = unzCloseCurrentFile(d->uf);
    if (err != UNZ_OK)
        return false;

    *buf = a;
    return true;
}

bool UnZip::readFile(const QString &fname, QByteArray *buf, int max)
{
    int err = unzLocateFile(d->uf, QFile::encodeName(fname).data(), d->caseSensitive);
    if (err != UNZ_OK)
        return false;

    char          filename_inzip[256];
    unz_file_info file_info;
    err = unzGetCurrentFileInfo(d->uf, &file_info, filename_inzip, sizeof(filename_inzip), nullptr, 0, nullptr, 0);
    if (err != UNZ_OK)
        return false;

    return readCurrentFile(buf, max);
}

bool UnZip::fileExists(const QString &fname)
{
    int err = unzLocateFile(d->uf, QFile::encodeName(fname).data(), d->caseSensitive);
    return err == UNZ_OK;
}
