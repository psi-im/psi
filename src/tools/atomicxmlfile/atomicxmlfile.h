/*
 * atomicxmlfile.h - atomic saving of QDomDocuments in files
 * Copyright (C) 2007  Michail Pishchagin
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <QString>

class QDomDocument;

class AtomicXmlFile
{
public:
	AtomicXmlFile(QString fileName);

	bool saveDocument(const QDomDocument& doc) const;
	bool loadDocument(QDomDocument* doc) const;

private:
	QString fileName_;

	QString tempFileName() const;
	QString backupFileName() const;
	bool saveDocument(const QDomDocument& doc, QString fileName) const;
	bool loadDocument(QDomDocument* doc, QString fileName) const;
};

#endif
