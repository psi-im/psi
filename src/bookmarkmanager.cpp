/*
 * bookmarkmanager.cpp
 * Copyright (C) 2006  Remko Troncon
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

#include "xmpp.h"
#include "xmpp_xmlcommon.h"
#include "bookmarkmanager.h"

using namespace XMPP;

// -----------------------------------------------------------------------------

class BookmarkTask : public Task
{
public:
	BookmarkTask(Task* parent) : Task(parent) {
	}
	
	void set(const QList<URLBookmark>& urls, const QList<ConferenceBookmark>& conferences) {
		iq_ = createIQ(doc(), "set", "", id());
		
		QDomElement prvt = doc()->createElement("query");
		prvt.setAttribute("xmlns", "jabber:iq:private");
		iq_.appendChild(prvt);
		
		foreach(URLBookmark u, urls)
			prvt.appendChild(u.toXml(*doc()));
		foreach(ConferenceBookmark c, conferences)
			prvt.appendChild(c.toXml(*doc()));
	}
	
	void get() {
		iq_ = createIQ(doc(), "get", "", id());
		
		QDomElement prvt = doc()->createElement("query");
		prvt.setAttribute("xmlns", "jabber:iq:private");
		iq_.appendChild(prvt);

		QDomElement bookmarks = doc()->createElement("storage");
		bookmarks.setAttribute("xmlns", "storage:bookmarks");
		prvt.appendChild(bookmarks);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (!e.isNull() && e.tagName() == "storage" && e.attribute("xmlns") == "storage:bookmarks") {
					for (QDomNode m = e.firstChild(); !m.isNull(); m = m.nextSibling()) {
						QDomElement f = m.toElement();
						if (f.isNull())
							continue;

						if (f.tagName() == "url") {
							URLBookmark u(f);
							if (!u.isNull())
								urls_ += u;
						}
						else if (f.tagName() == "conference") {
							ConferenceBookmark c(f);
							if (!c.isNull())
								conferences_ += c;
						}
					}
				}
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}
	
	const QList<URLBookmark>& urls() const {
		return urls_;
	}

	const QList<ConferenceBookmark>& conferences() const {
		return conferences_;
	}

private:
	QDomElement iq_;
	QList<URLBookmark> urls_;
	QList<ConferenceBookmark> conferences_;
};

// -----------------------------------------------------------------------------

BookmarkManager::BookmarkManager(XMPP::Client* client) : client_(client)
{
}

void BookmarkManager::getBookmarks()
{
	BookmarkTask* t = new BookmarkTask(client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(getBookmarks_finished()));
	t->get();
	t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark>& urls, const QList<ConferenceBookmark>& conferences)
{
	BookmarkTask* t = new BookmarkTask(client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setBookmarks_finished()));
	t->set(urls,conferences);
	t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark>& urls)
{
	BookmarkTask* t = new BookmarkTask(client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setBookmarks_finished()));
	t->set(urls,conferences_);
	t->go(true);
}

void BookmarkManager::setBookmarks(const QList<ConferenceBookmark>& conferences)
{
	BookmarkTask* t = new BookmarkTask(client_->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setBookmarks_finished()));
	t->set(urls_,conferences);
	t->go(true);
}

void BookmarkManager::getBookmarks_finished()
{
	BookmarkTask* t = (BookmarkTask*) sender();
	if (t->success()) {
		urls_ = t->urls();
		conferences_ = t->conferences();
		emit getBookmarks_success(urls_,conferences_);
	}
	else {
		emit getBookmarks_error(t->statusCode(), t->statusString());
	}
}

void BookmarkManager::setBookmarks_finished()
{
	BookmarkTask* t = (BookmarkTask*) sender();
	if (t->success()) {
		emit setBookmarks_success();
	}
	else {
		emit setBookmarks_error(t->statusCode(), t->statusString());
	}
}
