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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QString>
#include <QStringList>
#include <QFile>

#include "minizip/unzip.h"
#include "zip.h"

class UnZipPrivate
{
public:
	UnZipPrivate() {}

	QString name;
	unzFile uf;
	QStringList listing;
};

UnZip::UnZip(const QString &name)
{
	d = new UnZipPrivate;
	d->uf = 0;
	d->name = name;
}

UnZip::~UnZip()
{
	close();
	delete d;
}

void UnZip::setName(const QString &name)
{
	d->name = name;
}

const QString & UnZip::name() const
{
	return d->name;
}

bool UnZip::open()
{
	close();

	d->uf = unzOpen(QFile::encodeName(d->name).data());
	if(!d->uf)
		return false;

	return getList();
}

void UnZip::close()
{
	if(d->uf) {
		unzClose(d->uf);
		d->uf = 0;
	}

	d->listing.clear();
}

const QStringList & UnZip::list() const
{
	return d->listing;
}

bool UnZip::getList()
{
	unz_global_info gi;
	int err = unzGetGlobalInfo(d->uf, &gi);
	if(err != UNZ_OK)
		return false;

	QStringList l;
	for(int n = 0; n < (int)gi.number_entry; ++n) {
		char filename_inzip[256];
		unz_file_info file_info;
		int err = unzGetCurrentFileInfo(d->uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if(err != UNZ_OK)
			return false;

		l += filename_inzip;

		if((n+1) < (int)gi.number_entry) {
			err = unzGoToNextFile(d->uf);
			if(err != UNZ_OK)
				return false;
		}
	}

	d->listing = l;

	return true;
}

bool UnZip::readFile(const QString &fname, QByteArray *buf, int max)
{
	int err = unzLocateFile(d->uf, QFile::encodeName(fname).data(), 0);
	if(err != UNZ_OK)
		return false;

	char filename_inzip[256];
	unz_file_info file_info;
	err = unzGetCurrentFileInfo(d->uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
	if(err != UNZ_OK)
		return false;

	err = unzOpenCurrentFile(d->uf);
	if(err != UNZ_OK)
		return false;

	QByteArray a(0);
	QByteArray chunk;
	chunk.resize(16384);
	while(1) {
		err = unzReadCurrentFile(d->uf, chunk.data(), chunk.size());
		if(err < 0) {
			unzCloseCurrentFile(d->uf);
			return false;
		}
		if(err == 0)
			break;

		int oldsize = a.size();
		if(max > 0 && oldsize + err > max)
			err = max - oldsize;
		a.resize(oldsize + err);
		memcpy(a.data() + oldsize, chunk.data(), err);

		if(max > 0 && (int)a.size() >= max)
			break;
	}

	err = unzCloseCurrentFile(d->uf);
	if(err != UNZ_OK)
		return false;

	*buf = a;
	return true;
}
