/*
 * theme.cpp - base class for any theme
 * Copyright (C) 2010 Justin Karneges, Michail Pishchagin, Rion (Sergey Ilinyh)
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

#include "theme.h"

#include <QFileInfo>
#include <QStringList>

#ifndef NO_Theme_ZIP
#define Theme_ZIP
#endif

#ifdef Theme_ZIP
#	include "zip/zip.h"
#endif

class ThemeMetaData : public QSharedData
{
public:
	QString id, name, version, description, creation, homeUrl, filename;
	QStringList authors;
	QHash<QString, QString> info;

public:
	ThemeMetaData(const QString &id);
	//QByteArray loadData(const QString &fileName, const QString &dir) const;
};

ThemeMetaData::ThemeMetaData(const QString &id) :
	id(id),
	name(QObject::tr("Unnamed"))
{

}


//--------------------------------------
// Theme
//--------------------------------------
Theme::Theme(const QString &id) :
	md(new ThemeMetaData(id))
{

}

Theme::Theme(const Theme &other) :
	md(other.md)
{

}

Theme::~Theme()
{

}

QByteArray Theme::loadData(const QString &fileName, const QString &dir)
{
	QByteArray ba;

	QFileInfo fi(dir);
	if ( fi.isDir() ) {
		QFile file ( dir + '/' + fileName );
		//qDebug("read data from %s", qPrintable(file.fileName()));
		if (!file.open(QIODevice::ReadOnly))
			return ba;

		ba = file.readAll();
	}
#ifdef Theme_ZIP
	else if ( fi.suffix() == "jisp" || fi.suffix() == "zip" || fi.suffix() == "theme" ) {
		UnZip z(dir);
		if ( !z.open() )
			return ba;

		QString n = fi.completeBaseName() + '/' + fileName;
		if ( !z.readFile(n, &ba) ) {
			n = "/" + fileName;
			z.readFile(n, &ba);
		}
	}
#endif

	return ba;
}

const QString &Theme::id() const
{
	return md->id;
}

const QString &Theme::name() const
{
	return md->name;
}

void Theme::setName(const QString &name)
{
	md->name = name;
}

const QString &Theme::version() const
{
	return md->version;
}

const QString &Theme::description() const
{
	return md->description;
}

/**
 * Returns the Theme authors list.
 */
const QStringList &Theme::authors() const
{
	return md->authors;
}

/**
 * Returns the Theme creation date.
 */
const QString &Theme::creation() const
{
	return md->creation;
}

const QString &Theme::homeUrl() const
{
	return md->homeUrl;
}

/**
 * Returns directory (or .zip/.jisp archive) name from which Theme was loaded.
 */
const QString &Theme::fileName() const
{
	return md->filename;
}

/**
 * Sets the Theme directory (.zip archive) name.
 */
void Theme::setFileName(const QString &f)
{
	md->filename = f;
}

/**
 * Returns additional Theme information.
 * \sa setInfo()
 */
const QHash<QString, QString> Theme::info() const
{
	return md->info;
}

/**
 * Sets additional Theme information.
 * \sa info()
 */
void Theme::setInfo(const QHash<QString, QString> &i)
{
	md->info = i;
}

/**
 * Title suitable to display in options dialog
 */
QString Theme::title() const
{
	return md->name.isEmpty()? md->id : md->name;
}
