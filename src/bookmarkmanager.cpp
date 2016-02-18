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
			QStringList mucIgnore = PsiOptions::instance()->getOption("options.muc.bookmarks.ignore-join").toStringList();
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
								if (mucIgnore.contains(c.jid().bare())) {
									c.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
								}
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
	urls_ = urls;
	conferences_ = conferences;

	QStringList localMucs;
	QList<ConferenceBookmark> remoteMucs;
	QStringList ignoreMucs;

	foreach (const ConferenceBookmark &cb, conferences) {
		if (cb.autoJoin() == ConferenceBookmark::OnlyThisComputer) {
			localMucs.append(cb.jid().withResource(cb.nick()).full());
		} else {
			if (cb.autoJoin() == ConferenceBookmark::ExceptThisComputer) {
				ignoreMucs.append(cb.jid().bare());
			}
			remoteMucs.append(cb);
		}
	}
	PsiOptions::instance()->setOption("options.muc.bookmarks.local", localMucs);
	PsiOptions::instance()->setOption("options.muc.bookmarks.ignore-join", ignoreMucs);
	BookmarkTask* t = new BookmarkTask(account_->client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setBookmarks_finished()));
	t->set(urls,remoteMucs);
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
		QStringList localMucs = PsiOptions::instance()->getOption("options.muc.bookmarks.local").toStringList();
		conferences_ = t->conferences();
		foreach (const QString &lmuc, localMucs) {
			Jid j(lmuc);
			if (j.isValid()) {
				conferences_.append(ConferenceBookmark(j.node(), Jid(j.bare()),
													   ConferenceBookmark::OnlyThisComputer,
													   j.resource()));
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
	Q_UNUSED(t);
	emit bookmarksSaved();
}
