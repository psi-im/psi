/*
 * atomicxmlfile.cpp - atomic saving of QDomDocuments in files
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

#include "atomicxmlfile.h"

#include <QDomDocument>
#include <QFile>
#include <QStringList>
#include <QTextStream>

/**
 * Creates new instance of AtomicXmlFile class that will be able to
 * atomically save config file, so if application is terminated while
 * saving config file, data is not lost.
 */
AtomicXmlFile::AtomicXmlFile(QString fileName)
	: fileName_(fileName)
{
}

/**
 * Atomically save \a doc to specified name. Prior to saving, back up
 * of old config data is created, and only then data is saved.
 */
bool AtomicXmlFile::saveDocument(const QDomDocument& doc) const
{
	if (!saveDocument(doc, tempFileName())) {
		qWarning("AtomicXmlFile::saveDocument(): Unable to save '%s'. Possibly drive is full.",
		         qPrintable(tempFileName()));
		return false;
	}

	if (QFile::exists(backupFileName()))
		QFile::remove(backupFileName());

	if (QFile::exists(fileName_)) {
		if (!QFile::rename(fileName_, backupFileName())) {
			qWarning("AtomicXmlFile::saveDocument(): Unable to rename '%s' to '%s'.",
			         qPrintable(fileName_), qPrintable(backupFileName()));
			return false;
		}
	}

	if (!QFile::rename(tempFileName(), fileName_)) {
		qWarning("AtomicXmlFile::saveDocument(): Unable to rename '%s' to '%s'.",
		         qPrintable(tempFileName()), qPrintable(fileName_));
		return false;
	}

	return true;
}

/**
 * Tries to load \a doc from config file, or if that fails, from a back up.
 */
bool AtomicXmlFile::loadDocument(QDomDocument* doc) const
{
	Q_ASSERT(doc);

	QStringList fileNames;
	fileNames << fileName_
	          << tempFileName()
	          << backupFileName();

	foreach(QString fileName, fileNames)
		if (loadDocument(doc, fileName))
			return true;

	return false;
}

/**
 * Returns name of the file the config is first written to.
 */
QString AtomicXmlFile::tempFileName() const
{
	return fileName_ + ".temp";
}

/**
 * Returns name of the back up file.
 */
QString AtomicXmlFile::backupFileName() const
{
	return fileName_ + ".backup";
}

bool AtomicXmlFile::saveDocument(const QDomDocument& doc, QString fileName) const
{
	bool result = false;

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		return result;
	}

	QTextStream text;
	text.setDevice(&file);
	text.setCodec("UTF-8");
	text << doc.toString();

	result = file.error() == QFile::NoError;
	file.close();

	// QFile error checking should be enough, but to be completely sure that
	// XML is well-formed we could try to parse data we just had written:
	// if (result) {
	// 	QDomDocument temp;
	// 	result = loadDocument(&temp, fileName);
	// }

	return result;
}

bool AtomicXmlFile::loadDocument(QDomDocument* doc, QString fileName) const
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	if (!doc->setContent(&file)) {
		return false;
	}

	file.close();
	return true;
}
