/*
 * theme.h - base class for any theme
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

/*
 MOST IMPORTANT RULE: the theme never depends on thing to which its applied,
 so its one-way relation.
*/

#ifndef PSITHEME_H
#define PSITHEME_H

#include <QSharedData>
#include <QHash>

class ThemeMetaData;

//-----------------------------------------------
// Theme
//-----------------------------------------------
class Theme {
public:
	Theme(const QString &id);
	Theme(const Theme &other);
	virtual ~Theme();

	static QByteArray loadData(const QString &fileName, const QString &dir, bool caseInsensetive = false);

	const QString &id() const;
	const QString &name() const;
	void setName(const QString &name);
	const QString &version() const;
	const QString &description() const;
	const QStringList &authors() const;
	const QString &creation() const;
	const QString &homeUrl() const;
	const QString &fileName() const;
	void setFileName(const QString &f);
	const QHash<QString, QString> info() const;
	void setInfo(const QHash<QString, QString> &i);

	void setCaseInsensitiveFS(bool state);
	bool caseInsensitiveFS() const;

	virtual QString title() const;
	virtual QByteArray screenshot() = 0; // this hack must be replaced with something widget based
protected:
	QSharedDataPointer<ThemeMetaData> md;
};


#endif
