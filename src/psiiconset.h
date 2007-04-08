/*
 * psiiconset.h - the Psi iconset class
 * Copyright (C) 2001-2003  Justin Karneges, Michail Pishchagin
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

#ifndef PSIICONSET_H
#define PSIICONSET_H

#include <q3ptrlist.h>
#include <qmap.h>
#include <q3dict.h>

#include "iconset.h"
#include "im.h"

class PsiEvent;
class UserListItem;
struct Options;

class PsiIconset
{
public:
	static PsiIconset* instance();

	bool loadSystem();
	bool loadAll();

	bool optionsChanged(const Options *old); // returns 'true' if Psi needs to be restarted

	Q3Dict<Iconset> roster;
	Q3PtrList<Iconset> emoticons;
	const Iconset &system() const;
	void stripFirstAnimFrame(Iconset *);
	static void removeAnimation(Iconset *);

	Icon *event2icon(PsiEvent *);

	// these two can possibly fail (and return 0)
	Icon *statusPtr(int);
	Icon *statusPtr(const XMPP::Status &);

	// these two return empty Icon on failure and are safe
	Icon status(int);
	Icon status(const XMPP::Status &);

	// JID-enabled status functions
	Icon *statusPtr(const XMPP::Jid &, int);
	Icon *statusPtr(const XMPP::Jid &, const XMPP::Status &);

	Icon status(const XMPP::Jid &, int);
	Icon status(const XMPP::Jid &, const XMPP::Status &);

	// functions to get status icon by transport name
	Icon *transportStatusPtr(QString name, int);
	Icon *transportStatusPtr(QString name, const XMPP::Status &);

	Icon transportStatus(QString name, int);
	Icon transportStatus(QString name, const XMPP::Status &);

	Icon *statusPtr(UserListItem *);
	Icon status(UserListItem *);

private:
	PsiIconset();
	~PsiIconset();

	class Private;
	Private *d;

	static PsiIconset* instance_;
};

#endif
