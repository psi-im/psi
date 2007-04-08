/*
 * psitextview.h - PsiIcon-aware QTextView subclass widget
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef URLLABEL_H
#define URLLABEL_H

#include <QLabel>

class QWidget;
class QObject;

class URLLabel : public QLabel
{
	Q_OBJECT

	Q_PROPERTY( QString url READ url WRITE setUrl )
	Q_PROPERTY( QString title READ title WRITE setTitle )

	Q_OVERRIDE( QString text DESIGNABLE false SCRIPTABLE false )
	//Q_OVERRIDE( TextFormat DESIGNABLE false SCRIPTABLE false )

public:
	URLLabel(QWidget *parent = 0);
	~URLLabel();

	const QString &url() const;
	void setUrl(const QString &);

	const QString &title() const;
	void setTitle(const QString &);

protected:
	virtual void contextMenuEvent(QContextMenuEvent *);
	virtual void mouseReleaseEvent(QMouseEvent *);
	
	void updateText();

public:
	class Private;
private:
	Private *d;
};

#endif
