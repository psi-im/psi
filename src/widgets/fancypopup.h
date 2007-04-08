/*
 * fancypopup.h - the FancyPopup passive popup widget
 * Copyright (C) 2003-2005  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef FANCYPOPUP_H
#define FANCYPOPUP_H

#include <QFrame>

class PsiIcon;
class QTimer;

class FancyPopup : public QFrame
{
	Q_OBJECT
public:
	FancyPopup(QString title, const PsiIcon *icon = 0, FancyPopup *prev = 0, bool copyIcon = true);
	~FancyPopup();

	void addLayout(QLayout *layout, int stretch = 0);

	static void setHideTimeout(int);
	static void setBorderColor(QColor);

	void show();
	void restartHideTimer();

signals:
	void clicked(int);

protected:
	void hideEvent(QHideEvent *);
	void mouseReleaseEvent(QMouseEvent *);

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

#endif
