/*
 * urllabel.cpp - clickable URL-label
 * Copyright (C) 2003-2006  Michail Pishchagin
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

#include "urllabel.h"

#include <QMenu>
#include <QContextMenuEvent>

#include "urlobject.h"

//! \if _hide_doc_
class URLLabel::Private
{
public:
	QString url;
	QString title;
};
//! \endif

/**
 * \class URLLabel
 * \brief Clickable URL-label
 */

/**
 * Default constructor.
 */
URLLabel::URLLabel(QWidget *parent)
: QLabel(parent)
{
	d = new Private;
	setCursor( Qt::PointingHandCursor );
}

URLLabel::~URLLabel()
{
	delete d;
}

/**
 * Returns URL of this label.
 */
const QString &URLLabel::url() const
{
	return d->url;
}

/**
 * Sets the URL of this label to url
 * \param url an URL string
 */
void URLLabel::setUrl(const QString &url)
{
	d->url = url;
	updateText();
}

/**
 * Returns title of this label.
 */
const QString &URLLabel::title() const
{
	return d->title;
}

/**
 * Sets the title of this lable to t
 * \param t this string will be shown to user
 */
void URLLabel::setTitle(const QString &t)
{
	d->title = t;
	updateText();
}

void URLLabel::updateText()
{
	setText( QString("<a href=\"%1\">%2</a>").arg(d->url).arg(d->title) );

	if ( d->url != d->title )
		setToolTip(d->url);
	else
		setToolTip(QString());
}

void URLLabel::contextMenuEvent(QContextMenuEvent *e)
{
	QMenu *m = URLObject::getInstance()->createPopupMenu(d->url);

	if ( m ) {
		m->exec( e->globalPos() );
		delete m;
	}
	
	e->accept();
}

void URLLabel::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton)
		URLObject::getInstance()->popupAction(url());
	
	QLabel::mouseReleaseEvent(e);
}
