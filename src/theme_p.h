/*
 * theme_p.h
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

#ifndef THEME_P_H
#define THEME_P_H

#include <QSharedData>
#include "theme.h"

class QWidget;

class ThemePrivate : public QSharedData
{
public:
	PsiThemeProvider *provider;
	Theme::State state = Theme::NotLoaded;

	// metadata
	QString id, name, version, description, creation, homeUrl;
	QStringList authors;
	QHash<QString, QString> info;

	// runtime info
	QString filepath;
	bool caseInsensitiveFS;

public:
	ThemePrivate(PsiThemeProvider *provider);
	virtual ~ThemePrivate();

	virtual bool exists() = 0;
	virtual bool load(); // synchronous load
	virtual bool load(std::function<void(bool)> loadCallback);  // asynchronous load

	virtual bool hasPreview() const;
	virtual QWidget* previewWidget(); // this hack must be replaced with something widget based

	QByteArray loadData(const QString &fileName) const;
	Theme::ResourceLoader *resourceLoader() const;
};

#endif // THEME_P_H
