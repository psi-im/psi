/*
 * psithemeprovider.h - kinda adapter for set of themes
 * Copyright (C) 2010-2017 Sergey Ilinykh
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

#ifndef PSITHEMEPROVIDER_H
#define PSITHEMEPROVIDER_H

#include <QFuture>
#include <functional>
#include "theme.h"

class PsiCon;

class PsiThemeProvider : public QObject
{
	Q_OBJECT

	PsiCon *_psi;

public:
	PsiThemeProvider(PsiCon *parent);

	inline PsiCon* psi() const { return _psi; }

	virtual const char* type() const = 0;
	virtual Theme theme(const QString &id) = 0; // make new theme
	virtual const QStringList themeIds() const = 0;
	virtual bool loadCurrent() = 0;
	virtual Theme current() const = 0;
	virtual void setCurrentTheme(const QString &) = 0;

	virtual bool threadedLoading() const;
	virtual int screenshotWidth() const;

	virtual QString optionsName() const = 0;
	virtual QString optionsDescription() const = 0;

	static QString themePath(const QString &name);
};

#endif
