/*
 * theme_p.cpp
 * Copyright (C) 2017  Sergey Ilinykh
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

#include <QDir>
#include <QDirIterator>

#include "theme_p.h"

ThemePrivate::ThemePrivate(PsiThemeProvider *provider) :
	provider(provider),
	name(QObject::tr("Unnamed")),
	caseInsensitiveFS(false)
{

}

ThemePrivate::~ThemePrivate()
{

}

bool ThemePrivate::load()
{
	return false;
}

bool ThemePrivate::load(std::function<void (bool)> loadCallback)
{
	Q_UNUSED(loadCallback);
	return false;
}

bool ThemePrivate::hasPreview() const
{
	return false;
}

QWidget *ThemePrivate::previewWidget()
{
	return nullptr;
}

QByteArray ThemePrivate::loadData(const QString &fileName) const
{
	return Theme::loadData(fileName, filepath, caseInsensitiveFS);
}


//=================================================
// Reource Loader
//=================================================

class FSResourceLoader : public Theme::ResourceLoader
{
	QDir baseDir;

	// case insensetive file cache.
	QHash<QString,QString> ciFSCache; // lower case to real path
	bool caseInsensetive;
public:
	FSResourceLoader(const QDir &d, bool caseInsensetive) :
	    baseDir(d), caseInsensetive(caseInsensetive)
	{ }

	QByteArray loadData(const QString &fileName) {
		QFile file(baseDir.filePath(fileName));
		if (caseInsensetive && !file.exists()) {
			if (ciFSCache.isEmpty()) {
				updateCiFsCache();
			}
			QString realFN = ciFSCache.value(fileName.toLower());
			if (!realFN.isEmpty()) {
				file.setFileName(baseDir.filePath(realFN));
			}
		}
		//qDebug("read data from %s", qPrintable(file.fileName()));
		if (!file.open(QIODevice::ReadOnly)) {
			qDebug("%s Failed to open: %s", __FUNCTION__, qPrintable(file.fileName()));
			return QByteArray();
		}

		return file.readAll();
	}

	void updateCiFsCache()
	{
		int skip = baseDir.path().length() + 1;
		QDirIterator di(baseDir.path(), QDir::Files, QDirIterator::Subdirectories);
		while (di.hasNext()) {
			QString real = di.next().mid(skip);
			ciFSCache.insert(real.toLower(), real);
		}
	}

	bool fileExists(const QString &fileName)
	{
		QString base = baseDir.path() + QLatin1Char('/');
		if (QFileInfo(base + fileName).exists()) {
			return true;
		}
		if (caseInsensetive) {
			if (ciFSCache.isEmpty()) {
				updateCiFsCache();
			}
			QString realFN = ciFSCache.value(fileName.toLower());
			if (!realFN.isEmpty() && QFileInfo(base + realFN).exists()) {
				return true;
			}
		}
		return false;
	}
};

#ifdef Theme_ZIP
class ZipResourceLoader : public Theme::ResourceLoader
{
	UnZip z;
	QString baseName;
public:
	ZipResourceLoader(UnZip &&z, const QString &baseName) :
	    z(std::move(z)), baseName(baseName)
	{ }

	QByteArray loadData(const QString &fileName)
	{
		QString n = baseName + QLatin1Char('/') + fileName;
		QByteArray ba;

		if ( !z.readFile(n, &ba) ) {
			z.readFile(n.mid(baseName.count()), &ba);
		}

		return ba;
	}

	bool fileExists(const QString &fileName)
	{
		QString n = baseName + QLatin1Char('/') + fileName;

		if (z.fileExists(n)) {
			return true;
		}
		return z.fileExists(n.mid(baseName.count()));
	}
};
#endif

Theme::ResourceLoader *ThemePrivate::resourceLoader() const
{
	QFileInfo fi(filepath);
	if ( fi.isDir() ) {
		if (fi.isReadable()) {
			return new FSResourceLoader(QDir(fi.filePath()), caseInsensitiveFS);
		}
	}
#ifdef Theme_ZIP
	else if (Theme::isCompressed(fi)) {
		UnZip z;
		if (z.open()) {
			if (caseInsensitiveFS) {
				z.setCaseSensitivity(UnZip::CS_Insensitive);
			}
			return new ZipResourceLoader(std::move(z), fi.completeBaseName());
		}
	}
#endif
	return nullptr;
}


