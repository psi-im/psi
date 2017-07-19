/*
 * bookmarkmanager.h
 * Copyright (C) 2006-2008  Remko Troncon, Michail Pishchagin
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

#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <QObject>
#include <QList>

#include "conferencebookmark.h"
#include "urlbookmark.h"

class PsiAccount;

class BookmarkManager : public QObject
{
	Q_OBJECT

public:
	BookmarkManager(PsiAccount* account);

	bool isAvailable() const;
	bool isBookmarked(const XMPP::Jid &);

	QList<URLBookmark> urls() const;
	QList<ConferenceBookmark> conferences() const;
	int indexOfConference(const XMPP::Jid &) const;
	QString conferenceName(const XMPP::Jid &) const; // convenient method

	void setBookmarks(const QList<URLBookmark>&, const QList<ConferenceBookmark>&);
	void setBookmarks(const QList<URLBookmark>&);
	void setBookmarks(const QList<ConferenceBookmark>&);
	void removeConference(const XMPP::Jid &);

signals:
	void availabilityChanged();
	void urlsChanged(const QList<URLBookmark>&);
	void conferencesChanged(const QList<ConferenceBookmark>&);
	void bookmarksSaved();

private slots:
	void getBookmarks_finished();
	void setBookmarks_finished();
	void accountStateChanged();

private:
	void getBookmarks();
	void setIsAvailable(bool available);

private:
	PsiAccount* account_;
	bool accountAvailable_;
	bool isAvailable_;
	QList<URLBookmark> urls_;
	QList<ConferenceBookmark> conferences_;
};

#endif
