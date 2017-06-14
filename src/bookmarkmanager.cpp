/*
 * bookmarkmanager.cpp
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

#include "bookmarkmanager.h"

#include "xmpp_task.h"
#include "xmpp_client.h"
#include "xmpp_xmlcommon.h"
#include "psiaccount.h"
#include "psioptions.h"

// -----------------------------------------------------------------------------

class BookmarkTask : public Task
{
public:
	BookmarkTask(Task* parent) : Task(parent) {
	}

	void set(const QList<URLBookmark>& urls, const QList<ConferenceBookmark>& conferences) {
		// Store it here to take back if necessary
		urls_ = urls;
		conferences_ = conferences;

		iq_ = createIQ(doc(), "set", "", id());

		QDomElement prvt = doc()->createElement("query");
		prvt.setAttribute("xmlns", "jabber:iq:private");
		iq_.appendChild(prvt);

		QDomElement storage = doc()->createElement("storage");
		storage.setAttribute("xmlns", "storage:bookmarks");
		prvt.appendChild(storage);

		foreach(URLBookmark u, urls)
			storage.appendChild(u.toXml(*doc()));
		foreach(ConferenceBookmark c, conferences)
			storage.appendChild(c.toXml(*doc()));
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
							if (!c.isNull()) {
								conferences_ += c;
							}
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

BookmarkManager::BookmarkManager(PsiAccount* account)
	: account_(account)
	, accountAvailable_(false)
	, isAvailable_(false)
{
	connect(account_, SIGNAL(updatedActivity()), SLOT(accountStateChanged()));
}

bool BookmarkManager::isAvailable() const
{
	return isAvailable_;
}

void BookmarkManager::setIsAvailable(bool available)
{
	if (available != isAvailable_) {
		isAvailable_ = available;
		emit availabilityChanged();
	}
}

bool BookmarkManager::isBookmarked(const XMPP::Jid &j)
{
	return indexOfConference(j) >= 0;
}

void BookmarkManager::removeConference(const XMPP::Jid &j)
{
	if (isAvailable_) {
		QList<ConferenceBookmark> confs;
		foreach (ConferenceBookmark c, conferences_) {
			if (!c.jid().compare(j, false)) {
				confs.push_back(c);
			}
		}
		setBookmarks(confs);
	}
}

QList<URLBookmark> BookmarkManager::urls() const
{
	return urls_;
}

QList<ConferenceBookmark> BookmarkManager::conferences() const
{
	return conferences_;
}

int BookmarkManager::indexOfConference(const XMPP::Jid &j) const
{
	if (isAvailable_) {
		int i = 0;
		foreach(ConferenceBookmark c, conferences_) {
			if (c.jid().compare(j, false)) {
				return i;
			}
			i++;
		}
	}
	return -1;
}

QString BookmarkManager::conferenceName(const Jid &j) const
{
	int index = indexOfConference(j);
	if (index >= 0) {
		return conferences_[index].name();
	}
	return QString();
}

void BookmarkManager::accountStateChanged()
{
	if (!account_->isAvailable()) {
		setIsAvailable(false);
	}

	if (account_->isAvailable() && !accountAvailable_) {
		getBookmarks();
	}

	accountAvailable_ = account_->isAvailable();
}

void BookmarkManager::getBookmarks()
{
	BookmarkTask* t = new BookmarkTask(account_->client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(getBookmarks_finished()));
	t->get();
	t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark>& urls, const QList<ConferenceBookmark>& conferences)
{
	BookmarkTask* t = new BookmarkTask(account_->client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setBookmarks_finished()));
	t->set(urls,conferences);
	t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark>& urls)
{
	setBookmarks(urls, conferences());
}

void BookmarkManager::setBookmarks(const QList<ConferenceBookmark>& conferences)
{
	setBookmarks(urls(), conferences);
}

void BookmarkManager::getBookmarks_finished()
{
	BookmarkTask* t = static_cast<BookmarkTask*>(sender());
	if (t->success()) {
		bool urlsWereChanged = urls_ != t->urls();
		bool conferencesWereChanged = conferences_ != t->conferences();
		urls_ = t->urls();
		QStringList localMucs = account_->localMucBookmarks();
		QStringList mucIgnore = account_->ignoreMucBookmarks();
		QSet<QString> strippedMucs;
		for (const QString &m: localMucs) {
			Jid j(m);
			if (j.isValid()) {
				strippedMucs.insert(j.bare());
			}
		}

		conferences_ = t->conferences();
		for (auto &muc: conferences_) {
			if (strippedMucs.contains(muc.jid().bare())) {
				muc.setAutoJoin(ConferenceBookmark::OnlyThisComputer);
			}
			if (mucIgnore.contains(muc.jid().bare())) {
				muc.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
			}
		}

		if (urlsWereChanged)
			emit urlsChanged(urls_);
		if (conferencesWereChanged)
			emit conferencesChanged(conferences_);

		setIsAvailable(true);
	}
	else {
		setIsAvailable(false);
	}
}

void BookmarkManager::setBookmarks_finished()
{
	BookmarkTask* t = static_cast<BookmarkTask*>(sender());
	if (t->success()) {
		bool conferencesWereChanged = conferences_ != t->conferences();
		bool urlsWereChanged = urls_ != t->urls();
		conferences_ = t->conferences();
		urls_ = t->urls();

		QStringList localMucs;
		QStringList ignoreMucs;

		for (const ConferenceBookmark &cb: conferences_) {
			if (cb.autoJoin() == ConferenceBookmark::OnlyThisComputer) {
				localMucs.append(cb.jid().withResource(cb.nick()).full());
			}
			if (cb.autoJoin() == ConferenceBookmark::ExceptThisComputer) {
				ignoreMucs.append(cb.jid().bare());
			}
		}

		account_->setLocalMucBookmarks(localMucs);
		account_->setIgnoreMucBookmarks(ignoreMucs);

		if (urlsWereChanged)
			emit urlsChanged(urls_);
		if (conferencesWereChanged)
			emit conferencesChanged(conferences_);

		emit bookmarksSaved();
	}
}
