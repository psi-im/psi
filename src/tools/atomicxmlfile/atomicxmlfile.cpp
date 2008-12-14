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

QStringList AtomicXmlFile::loadCandidateList() const {
	QStringList fileNames;
	fileNames << fileName_
	          << tempFileName()
	          << backupFileName();
	return fileNames;
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
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}

	QTextStream text;
	text.setDevice(&file);
	text.setCodec("UTF-8");
	text << doc.toString();

	return file.error() == QFile::NoError;
}

bool AtomicXmlFile::loadDocument(QDomDocument* doc, QString fileName) const
{
	Q_ASSERT(doc);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	return doc->setContent(&file);
}

bool AtomicXmlFile::saveDocument(AtomicXmlFileWriter* writer, QString fileName) const
{
	Q_ASSERT(writer);
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}

	if (!writer->write(&file)) {
		return false;
	}

	return file.error() == QFile::NoError;
}

bool AtomicXmlFile::loadDocument(AtomicXmlFileReader* reader, QString fileName) const
{
	Q_ASSERT(reader);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	if (!reader->read(&file)) {
		qWarning("Parse error in file %s at line %d, column %d:\n%s",
		         qPrintable(fileName), (int)reader->lineNumber(),
		         (int)reader->columnNumber(), qPrintable(reader->errorString()));

		return false;
	}

	return true;
}

/**
 * Check if an AtomicXmlFile exists.
 * returns true if any of the files loadDocument tries to read exists,
 * it *doesn't* check that there is at least one uncorupted file.
 */
bool AtomicXmlFile::exists(QString fileName) {
	AtomicXmlFile tmp(fileName);

	foreach(QString fileName, tmp.loadCandidateList()) {
		if (QFile::exists(fileName)) {
			return true;
		}
	}
	return false;
}
