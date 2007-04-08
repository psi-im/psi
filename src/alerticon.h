/*
 * alerticon.h - class for handling animating alert icons
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef ALERTICON_H
#define ALERTICON_H

#include "iconset.h"

class QPixmap;
class Impix;
class QString;
class QIcon;
class QImage;

class AlertIcon : public PsiIcon
{
	Q_OBJECT
public:
	AlertIcon(const PsiIcon *icon);
	~AlertIcon();

	// reimplemented
	virtual bool isAnimated() const;
	virtual const QPixmap &pixmap() const;
	virtual const QImage &image() const;
	virtual const QIcon & icon() const;
	virtual const Impix &impix() const;
	virtual int frameNumber() const;
	virtual const QString &name() const;

	virtual PsiIcon *copy() const;

public slots:
	void activated(bool playSound = true);
	void stop();

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

void alertIconUpdateAlertStyle();

#endif
