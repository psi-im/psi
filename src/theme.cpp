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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "theme.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStringList>

#ifndef NO_Theme_ZIP
#define Theme_ZIP
#endif

#ifdef Theme_ZIP
#	include "zip/zip.h"
#endif

#ifndef HAVE_QT5
# define QLatin1Literal QLatin1String
#endif

#include "psithemeprovider.h"
#include "theme_p.h"




//--------------------------------------
// Theme
//--------------------------------------
Theme::Theme()
{

}

Theme::Theme(ThemePrivate *priv) :
    d(priv)
{

}

Theme::Theme(const Theme &other) :
	d(other.d)
{

}

Theme &Theme::operator=(const Theme &other)
{
	d = other.d;
	return *this;
}

Theme::~Theme()
{

}

bool Theme::isValid() const
{
	return d;
}

Theme::State Theme::state() const
{
	if (!d) {
		return Invalid;
	}
	return d->state;
}

bool Theme::exists()
{
	return d && d->exists();
}

bool Theme::load()
{
	if (!d) {
		return false;
	}
	return d->load();
}

bool Theme::load(std::function<void (bool)> loadCallback)
{
	return d->load(loadCallback);
}

bool Theme::hasPreview() const
{
	return d->hasPreview();
}

QWidget* Theme::previewWidget()
{
	return d->previewWidget();
}

bool Theme::isCompressed(const QFileInfo &fi)
{
	QString sfx = fi.suffix();
	return fi.isDir() && (sfx == QLatin1Literal("jisp") ||
	                      sfx == QLatin1Literal("zip") ||
	                      sfx == QLatin1Literal("theme"));
}

bool Theme::isCompressed() const
{
	return isCompressed(QFileInfo(d->filepath));
}

QByteArray Theme::loadData(const QString &fileName, const QString &themePath, bool caseInsensetive)
{
	QByteArray ba;
	//qDebug("loading %s from %s", qPrintable(fileName), qPrintable(dir));
	QFileInfo fi(themePath);
	if ( fi.isDir() ) {
		QFile file(themePath + '/' + fileName);
		if (caseInsensetive && !file.exists()) {
			QDir d(themePath);
			foreach (const QString &name, fileName.toLower().split('/')) {
				if (name.isEmpty()) { // force relative path and drop double slahses
					continue;
				}
				QDirIterator di(d);
				QFileInfo fi;
				bool found = false;
				while (di.hasNext()) {
					di.next();
					if (di.fileName().compare(name, Qt::CaseInsensitive) == 0) {
						found = true;
						fi = di.fileInfo();
						break;
					}
				}
				if (!found) {
					qDebug("%s Not found: %s/%s", __FUNCTION__, qPrintable(d.path()), qPrintable(name));
					return ba;
				}
				if (fi.isFile()) {
					file.setFileName(fi.filePath());
					break;
				}
				d.cd(fi.fileName()); // so that was directory. go into.
			}
		}
		//qDebug("read data from %s", qPrintable(file.fileName()));
		if (!file.open(QIODevice::ReadOnly)) {
			qDebug("%s Failed to open: %s", __FUNCTION__, qPrintable(file.fileName()));
			return ba;
		}

		ba = file.readAll();
	}
#ifdef Theme_ZIP
	else if ( fi.suffix() == "jisp" || fi.suffix() == "zip" || fi.suffix() == "theme" ) {
		UnZip z(themePath);
		if ( !z.open() )
			return ba;
		if (caseInsensetive) {
			z.setCaseSensitivity(UnZip::CS_Insensitive);
		}

		QString n = fi.completeBaseName() + '/' + fileName;
		if ( !z.readFile(n, &ba) ) {
			n = "/" + fileName;
			z.readFile(n, &ba);
		}
	}
#endif

	return ba;
}

QByteArray Theme::loadData(const QString &fileName) const
{
	return d->loadData(fileName);
}

Theme::ResourceLoader *Theme::resourceLoader() const
{
	return d->resourceLoader();
}

const QString Theme::id() const
{
	return d? d->id : QString();
}

void Theme::setId(const QString &id)
{
	d->id = id;
}

const QString &Theme::name() const
{
	return d->name;
}

void Theme::setName(const QString &name)
{
	d->name = name;
}

const QString &Theme::version() const
{
	return d->version;
}

const QString &Theme::description() const
{
	return d->description;
}

/**
 * Returns the Theme authors list.
 */
const QStringList &Theme::authors() const
{
	return d->authors;
}

/**
 * Returns the Theme creation date.
 */
const QString &Theme::creation() const
{
	return d->creation;
}

const QString &Theme::homeUrl() const
{
	return d->homeUrl;
}

PsiThemeProvider *Theme::themeProvider() const
{
	return d->provider;
}

/**
 * Returns directory (or .zip/.jisp archive) name from which Theme was loaded.
 */
const QString &Theme::filePath() const
{
	return d->filepath;
}

/**
 * Sets the Theme directory (.zip archive) name.
 */
void Theme::setFilePath(const QString &f)
{
	d->filepath = f;
}

/**
 * Returns additional Theme information.
 * \sa setInfo()
 */
const QHash<QString, QString> Theme::info() const
{
	return d->info;
}

/**
 * Sets additional Theme information.
 * \sa info()
 */
void Theme::setInfo(const QHash<QString, QString> &i)
{
	d->info = i;
}

void Theme::setCaseInsensitiveFS(bool state)
{
	d->caseInsensitiveFS = state;
}

bool Theme::caseInsensitiveFS() const
{
	return d->caseInsensitiveFS;
}

/**
 * Title suitable to display in options dialog
 */
QString Theme::title() const
{
	return d->name.isEmpty()? d->id : d->name;
}

void Theme::setState(Theme::State state)
{
	d->state = state;
}

Theme::ResourceLoader::~ResourceLoader()
{

}
