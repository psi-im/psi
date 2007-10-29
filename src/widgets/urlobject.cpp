/*
 * urlobject.cpp - helper class for handling links
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

#include "urlobject.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>

#include "iconaction.h"

//! \if _hide_doc_
class URLObject::Private : QObject
{
	Q_OBJECT
public:
	QString link;
	IconAction *act_mailto, *act_join_groupchat, *act_send_message, *act_browser, *act_add_to_roster, *act_copy;
	URLObject *urlObject;

	Private(URLObject *parent)
		: QObject(parent)
	{
		urlObject = parent;
		QString tr;
		
		tr = qApp->translate("URLLabel", "Open mail composer");
		act_mailto = new IconAction(tr, "psi/email", tr, 0, this);
		connect(act_mailto, SIGNAL(activated()), SLOT(popupAction()));
	
		tr = qApp->translate("URLLabel", "Open web browser");
		act_browser = new IconAction(tr, "psi/www", tr, 0, this);
		connect(act_browser, SIGNAL(activated()), SLOT(popupAction()));

		tr = qApp->translate("URLLabel", "Add to Roster");
		act_add_to_roster = new IconAction(tr, "psi/addContact", tr, 0, this);
		connect(act_add_to_roster, SIGNAL(activated()), SLOT(popupAction()));

		tr = qApp->translate("URLLabel", "Send massage to");
		act_send_message = new IconAction(tr, "psi/message", tr, 0, this);
		connect(act_send_message, SIGNAL(activated()), SLOT(popupAction()));

		tr = qApp->translate("URLLabel", "Join groupchat");
		act_join_groupchat = new IconAction(tr, "psi/groupChat", tr, 0, this);
		connect(act_join_groupchat, SIGNAL(activated()), SLOT(popupAction()));

		tr = qApp->translate("URLLabel", "Copy location");
		act_copy = new IconAction(tr, tr, 0, this);
		connect(act_copy, SIGNAL(activated()), SLOT(popupCopy()));
	}
		
	QString copyString(QString from)
	{
		QString l = from;

		int colon = l.find(':');
		if ( colon == -1 )
			colon = 0;
		QString service = l.left( colon );

		if ( service == "mailto" || service == "jabber" || service == "jid" || service == "xmpp" || service == "atstyle") {
			if ( colon > -1 )
				l = l.mid( colon + 1 );

			while ( l[0] == '/' )
				l = l.mid( 1 );
		}

		return l;
	}

public slots:
	void popupAction(QString lnk) {
		emit urlObject->openURL(lnk);
	}

	void popupAction() {
		popupAction(link);
	}

	void popupCopy(QString lnk) {
		QApplication::clipboard()->setText( copyString(lnk), QClipboard::Clipboard );
		if(QApplication::clipboard()->supportsSelection())
			QApplication::clipboard()->setText( copyString(lnk), QClipboard::Selection );
	}

	void popupCopy() {
		popupCopy(link);
	}
};
//! \endif

/**
 * \class URLObject
 * \brief Helper class for handling clicking on URLs
 */

/**
 * Default constructor.
 */
URLObject::URLObject()
	: QObject(qApp)
{
	d = new Private(this);
}

/**
 * Returns instance of the class, and creates it if necessary.
 */
URLObject *URLObject::getInstance()
{
	static URLObject *urlObject = 0;
	if (!urlObject)
		urlObject = new URLObject();
	return urlObject;
}

/**
 * Creates QMenu with actions corresponding to link's type.
 * @param lnk link in service:url format
 */
QMenu *URLObject::createPopupMenu(const QString &lnk)
{
	d->link = lnk;
	if ( d->link.isEmpty() )
		return 0;

	int colon = d->link.find(':');
	if ( colon == -1 )
		colon = 0;
	QString service = d->link.left( colon );

	QMenu *m = new QMenu;
	
	if ( service == "mailto" || service == "atstyle") {
		m->addAction(d->act_mailto);
	}
	else if ( service == "jabber" || service == "jid" || service == "xmpp" || service == "atstyle") {
		// TODO: need more actions to jabber item. Ex: "add to roster", "send message"
		m->addAction(d->act_add_to_roster);
		m->addAction(d->act_send_message);
		m->addAction(d->act_join_groupchat);
	}
	else { //if ( service == "http" || service == "https" || service.isEmpty() ) {
		m->addAction(d->act_browser);
	}

	m->addAction(d->act_copy);
	return m;
}

/**
 * Simulates default action using the passed URL.
 * \param lnk URL string
 */
void URLObject::popupAction(QString lnk)
{
	d->popupAction(lnk);
}

#include "urlobject.moc"
