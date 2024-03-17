/*
 * zip.h - zip/unzip files
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

#ifndef CS_ZIP_H
#define CS_ZIP_H

#include <QHash>
#include <QString>

class QByteArray;

class UnZip {
public:
    enum CaseSensitivity {
        CS_Default     = 0, // system default
        CS_Sensitive   = 1,
        CS_Insensitive = 2
    };

    UnZip(const QString &fname = QString());
    UnZip(UnZip &&o);
    ~UnZip();

    void           setCaseSensitivity(CaseSensitivity state = CS_Default);
    void           setName(const QString &);
    const QString &name() const;

    bool open();
    void close();

    QHash<QString, QByteArray> unpackAll();
    const QStringList         &list() const;
    bool                       readFile(const QString &, QByteArray *, int max = 0);
    bool                       fileExists(const QString &);

private:
    class UnZipPrivate *d;

    bool getList();
    bool readCurrentFile(QByteArray *buf, int max = 0);
};

#endif // CS_ZIP_H
