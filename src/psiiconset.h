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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef PSIICONSET_H
#define PSIICONSET_H

#include <QMap>

#include "iconset.h"
#include "psievent.h"

class UserListItem;
namespace XMPP {
	class Status;
	class Jid;
}

class PsiIconset : public QObject
{
	Q_OBJECT
public:
	static PsiIconset* instance();

	bool loadSystem();
	void reloadRoster();
	bool loadAll();

	QHash<QString, Iconset*> roster;
	QList<Iconset*> emoticons;
	Iconset moods;
	Iconset activities;
	Iconset clients;
	Iconset affiliations;
	const Iconset &system() const;
	void stripFirstAnimFrame(Iconset *);
	static void removeAnimation(Iconset *);

	PsiIcon *event2icon(const PsiEvent::Ptr &e);

	// these two can possibly fail (and return 0)
	PsiIcon *statusPtr(int);
	PsiIcon *statusPtr(const XMPP::Status &);

	// these two return empty PsiIcon on failure and are safe
	PsiIcon status(int);
	PsiIcon status(const XMPP::Status &);

	// JID-enabled status functions
	PsiIcon *statusPtr(const XMPP::Jid &, int);
	PsiIcon *statusPtr(const XMPP::Jid &, const XMPP::Status &);

	PsiIcon status(const XMPP::Jid &, int);
	PsiIcon status(const XMPP::Jid &, const XMPP::Status &);

	// functions to get status icon by transport name
	PsiIcon *transportStatusPtr(QString name, int);
	PsiIcon *transportStatusPtr(QString name, const XMPP::Status &);

	PsiIcon transportStatus(QString name, int);
	PsiIcon transportStatus(QString name, const XMPP::Status &);

	PsiIcon *statusPtr(UserListItem *);
	PsiIcon status(UserListItem *);

	QString caps2client(const QString &name);
signals:
	void emoticonsChanged();
	void systemIconsSizeChanged(int);
	void rosterIconsSizeChanged(int);

public slots:
	static void reset();

private slots:
	void optionChanged(const QString& option);

private:
	PsiIconset();
	~PsiIconset();

	class Private;
	Private *d;

	static PsiIconset* instance_;

	bool loadRoster();
	void loadEmoticons();
	bool loadMoods();
	bool loadActivity();
	bool loadClients();
	bool loadAffiliations();

};

QString status2name(int s);

#endif
