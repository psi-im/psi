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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef ATOMICXMLFILE_H
#define ATOMICXMLFILE_H

#include <QFile>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class QDomDocument;

class AtomicXmlFileReader : public QXmlStreamReader {
public:
    virtual ~AtomicXmlFileReader() { }
    virtual bool read(QIODevice *device) = 0;
};

class AtomicXmlFileWriter : public QXmlStreamWriter {
public:
    virtual ~AtomicXmlFileWriter() { }
    virtual bool write(QIODevice *device) = 0;
};

class AtomicXmlFile {
public:
    AtomicXmlFile(const QString &fileName);

    /**
     * Atomically save \a writer to specified name. Prior to saving, back up
     * of old config data is created, and only then data is saved.
     */
    template <typename T> bool saveDocument(T writer) const
    {
        if (!saveDocument(writer, tempFileName())) {
            qWarning("AtomicXmlFile::saveDocument(): Unable to save '%s'. Possibly drive is full.",
                     qPrintable(tempFileName()));
            return false;
        }

        if (QFile::exists(fileName_)) {
            if (QFile::exists(backupFileName()))
                QFile::remove(backupFileName());
            if (!QFile::rename(fileName_, backupFileName())) {
                qWarning("AtomicXmlFile::saveDocument(): Unable to rename '%s' to '%s'.", qPrintable(fileName_),
                         qPrintable(backupFileName()));
                return false;
            }
        }

        if (!QFile::rename(tempFileName(), fileName_)) {
            qWarning("AtomicXmlFile::saveDocument(): Unable to rename '%s' to '%s'.", qPrintable(tempFileName()),
                     qPrintable(fileName_));
            return false;
        }

        return true;
    }

    /**
     * Tries to load \a reader from config file, or if that fails, from a back up.
     */
    template <typename T> bool loadDocument(T reader) const
    {
        Q_ASSERT(reader);

        const QStringList &fileNames(loadCandidateList());
        for (const QString &fileName : fileNames) {
            if (loadDocument(reader, fileName)) {
                return true;
            }
            if (QFile::exists(fileName)) {
                // The file exists, but it is incorrect. Remove it. Otherwise, enter the backup.
                QFile::remove(fileName);
            }
        }

        return false;
    }

    static bool exists(const QString &fileName);

private:
    QString fileName_;

    QString     tempFileName() const;
    QString     backupFileName() const;
    QStringList loadCandidateList() const;
    bool        saveDocument(const QDomDocument &doc, QString fileName) const;
    bool        loadDocument(QDomDocument *doc, QString fileName) const;
    bool        saveDocument(AtomicXmlFileWriter *writer, QString fileName) const;
    bool        loadDocument(AtomicXmlFileReader *reader, QString fileName) const;
};

#endif // ATOMICXMLFILE_H
