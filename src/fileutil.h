/*
 * fileutil.h - common file dialogs
 * Copyright (C) 2008  Michail Pishchagin
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

#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <QObject>

class QWidget;

class FileUtil : public QObject
{
	Q_OBJECT

public:
	static QString lastUsedOpenPath();
	static void setLastUsedOpenPath(const QString& path);

	static QString lastUsedSavePath();
	static void setLastUsedSavePath(const QString& path);

	static QString getImageFileName(QWidget* parent);
	static QString getOpenFileName(QWidget* parent = 0, const QString& caption = QString(), const QString& filter = QString(), QString* selectedFilter = 0);
	static QString getSaveFileName(QWidget* parent = 0, const QString& caption = QString(), const QString& defaultFileName = QString(), const QString& filter = QString(), QString* selectedFilter = 0);

	static QString mimeToFileExt(const QString &mime);
};

#endif
