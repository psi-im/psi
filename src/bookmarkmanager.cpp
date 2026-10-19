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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "bookmarkmanager.h"

#include "iris/xmpp_client.h"
#include "iris/xmpp_bookmarks.h"
#include "iris/xmpp_features.h"
#include "iris/xmpp_pubsub.h"
#include "iris/xmpp_serverinfomanager.h"
#include "iris/xmpp_task.h"
#include "iris/xmpp_xmlcommon.h"
#include "psiaccount.h"

// -----------------------------------------------------------------------------

class BookmarkTask : public Task {
public:
    BookmarkTask(Task *parent) : Task(parent) { }

    void set(const QList<URLBookmark> &urls, const QList<ConferenceBookmark> &conferences)
    {
        // Store it here to take back if necessary
        urls_        = urls;
        conferences_ = conferences;

        iq_ = createIQ(doc(), "set", "", id());

        QDomElement prvt = doc()->createElementNS("jabber:iq:private", "query");
        iq_.appendChild(prvt);

        QDomElement storage = doc()->createElementNS("storage:bookmarks", "storage");
        prvt.appendChild(storage);

        for (const URLBookmark &u : urls)
            storage.appendChild(u.toXml(*doc()));
        for (const ConferenceBookmark &c : conferences)
            storage.appendChild(c.toXml(*doc()));
    }

    void get()
    {
        iq_ = createIQ(doc(), "get", "", id());

        QDomElement prvt = doc()->createElementNS("jabber:iq:private", "query");
        iq_.appendChild(prvt);

        QDomElement bookmarks = doc()->createElementNS("storage:bookmarks", "storage");
        prvt.appendChild(bookmarks);
    }

    void onGo() { send(iq_); }

    bool take(const QDomElement &x)
    {
        if (!iqVerify(x, "", id()))
            return false;

        if (x.attribute("type") == "result") {
            QDomElement q = queryTag(x);
            for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
                QDomElement e = n.toElement();
                if (!e.isNull() && e.tagName() == "storage" && e.namespaceURI() == "storage:bookmarks") {
                    for (QDomNode m = e.firstChild(); !m.isNull(); m = m.nextSibling()) {
                        QDomElement f = m.toElement();
                        if (f.isNull())
                            continue;

                        if (f.tagName() == "url") {
                            URLBookmark u(f);
                            if (!u.isNull())
                                urls_ += u;
                        } else if (f.tagName() == "conference") {
                            ConferenceBookmark c(f);
                            if (!c.isNull()) {
                                conferences_ += c;
                            }
                        }
                    }
                }
            }
            setSuccess();
        } else {
            setError(x);
        }
        return true;
    }

    const QList<URLBookmark> &urls() const { return urls_; }

    const QList<ConferenceBookmark> &conferences() const { return conferences_; }

private:
    QDomElement               iq_;
    QList<URLBookmark>        urls_;
    QList<ConferenceBookmark> conferences_;
};

// -----------------------------------------------------------------------------

struct BookmarkManager::PepState {
    bool active = false;
    bool loading = false;
    bool writing = false;
    QHash<QString, XMPP::NativeBookmark> originals;
};

BookmarkManager::BookmarkManager(PsiAccount *account) :
    account_(account), accountAvailable_(false), isAvailable_(false), pep_(std::make_unique<PepState>())
{
    connect(account_, SIGNAL(updatedActivity()), SLOT(accountStateChanged()));
    connect(account_->client()->serverInfoManager(), &XMPP::ServerInfoManager::featuresChanged,
            this, &BookmarkManager::maybeEnableNativeBookmarks);
    auto *pubsub = account_->client()->pubSubManager();
    connect(pubsub, &XMPP::PubSubManager::itemPublished, this,
            [this](const XMPP::Jid &service, const QString &node, const XMPP::PubSubItem &item) {
        if (!pep_->active || service.bare() != account_->jid().bare() || node != XMPP::NativeBookmark::node())
            return;
        const auto native = XMPP::NativeBookmark::fromItem(item);
        if (!native.isValid())
            return;
        pep_->originals.insert(native.room().bare(), native);
        const ConferenceBookmark conference(native.name(), native.room(),
            native.autoJoin() ? ConferenceBookmark::Always : ConferenceBookmark::Never,
            native.nick(), native.password());
        auto result = conferences_;
        const auto it = std::find_if(result.begin(), result.end(),
             [&native](const ConferenceBookmark &b) { return b.jid().bare() == native.room().bare(); });
        if (it == result.end())
            result.append(conference);
        else
            *it = conference;
        if (result != conferences_) {
            conferences_ = result;
            emit conferencesChanged(conferences_);
        }
    });
    connect(pubsub, &XMPP::PubSubManager::itemRetracted, this,
            [this](const XMPP::Jid &service, const QString &node, const QString &id) {
        if (!pep_->active || service.bare() != account_->jid().bare() || node != XMPP::NativeBookmark::node())
            return;
        pep_->originals.remove(id);
        auto result = conferences_;
        result.erase(std::remove_if(result.begin(), result.end(),
                      [&id](const ConferenceBookmark &b) { return b.jid().bare() == id; }), result.end());
        if (result != conferences_) {
            conferences_ = result;
            emit conferencesChanged(conferences_);
        }
    });
}

BookmarkManager::~BookmarkManager() = default;

bool BookmarkManager::isAvailable() const { return isAvailable_; }

void BookmarkManager::setIsAvailable(bool available)
{
    if (available != isAvailable_) {
        isAvailable_ = available;
        emit availabilityChanged();
    }
}

bool BookmarkManager::isBookmarked(const XMPP::Jid &j) { return indexOfConference(j) >= 0; }

void BookmarkManager::addConference(const ConferenceBookmark &newb)
{
    if (!isAvailable_ && !newb.isNull())
        return;
    auto it = std::find_if(conferences_.begin(), conferences_.end(),
                           [newb](auto const &c) { return newb.jid().compare(c.jid(), false); });
    if (it == conferences_.end()) {
        auto copy = conferences_;
        copy.append(newb);
        setBookmarks(copy);
    }
}

void BookmarkManager::removeConference(const XMPP::Jid &j)
{
    if (isAvailable_) {
        QList<ConferenceBookmark> confs;
        for (const ConferenceBookmark &c : std::as_const(conferences_)) {
            if (!c.jid().compare(j, false)) {
                confs.push_back(c);
            }
        }
        setBookmarks(confs);
    }
}

const QList<URLBookmark> &BookmarkManager::urls() const { return urls_; }

const QList<ConferenceBookmark> &BookmarkManager::conferences() const { return conferences_; }

int BookmarkManager::indexOfConference(const XMPP::Jid &j) const
{
    if (isAvailable_) {
        int i = 0;
        for (const ConferenceBookmark &c : conferences_) {
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
        pep_->active = false;
        pep_->loading = false;
        pep_->originals.clear();
    }

    if (account_->isAvailable() && !accountAvailable_) {
        getBookmarks();
    }

    accountAvailable_ = account_->isAvailable();
}

void BookmarkManager::maybeEnableNativeBookmarks()
{
    if (!account_->isAvailable() || !isAvailable_ || pep_->active || pep_->loading)
        return;
    auto *server = account_->client()->serverInfoManager();
    // #compat means the server itself mirrors legacy private XML bookmarks.
    // publish-options + whitelist are mandatory to avoid leaking room passwords.
    const bool pubOptions = server->accountFeatures().test("http://jabber.org/protocol/pubsub#publish-options")
        || server->serverFeatures().test("http://jabber.org/protocol/pubsub#publish-options");
    if (server->hasPEP() && server->accountFeatures().test("urn:xmpp:bookmarks:1#compat") && pubOptions)
        loadNativeBookmarks();
}

void BookmarkManager::loadNativeBookmarks()
{
    pep_->loading = true;
    auto *task = account_->client()->pubSubManager()->items(XMPP::Jid(), XMPP::NativeBookmark::node());
    connect(task, &XMPP::PubSubItemsTask::finished, this, [this, task] {
        pep_->loading = false;
        if (!account_->isAvailable() || !task->success())
            return; // Keep the working private-XML backend on retrieval error.
        QHash<QString, XMPP::NativeBookmark> originals;
        QList<ConferenceBookmark> result;
        for (const auto &item : task->items()) {
            const auto bookmark = XMPP::NativeBookmark::fromItem(item);
            if (!bookmark.isValid())
                continue;
            originals.insert(bookmark.room().bare(), bookmark);
            result.append(ConferenceBookmark(bookmark.name(), bookmark.room(),
                bookmark.autoJoin() ? ConferenceBookmark::Always : ConferenceBookmark::Never,
                bookmark.nick(), bookmark.password()));
        }
        pep_->originals = originals;
        pep_->active = true;

        // Even on a server advertising #compat, pre-existing private-XML
        // bookmarks may not yet have been migrated. Preserve unmatched
        // legacy rooms locally and publish them individually with whitelist
        // options. Never overwrite an already existing PEP bookmark.
        for (const auto &legacy : conferences_) {
            if (!legacy.jid().isValid() || legacy.jid().isEmpty()
                || originals.contains(legacy.jid().bare()))
                continue;
            result.append(legacy);
            const bool sharedAutoJoin = legacy.autoJoin() == ConferenceBookmark::Always
                || legacy.autoJoin() == ConferenceBookmark::ExceptThisComputer;
            auto native = XMPP::NativeBookmark::create(legacy.jid(), legacy.name(), sharedAutoJoin,
                                                        legacy.nick(), legacy.password());
            if (!native.isValid())
                continue;
            QDomDocument doc;
            auto *publish = account_->client()->pubSubManager()->publish(
                XMPP::Jid(), XMPP::NativeBookmark::node(), native.toItem(doc),
                XMPP::NativeBookmark::privatePublishOptions());
            connect(publish, &XMPP::PubSubPublishTask::finished, this, [this, publish, native] {
                if (publish->success() && pep_->active)
                    pep_->originals.insert(native.room().bare(), native);
            });
            publish->go(true);
        }

        // The old Private XML fetch still supplies URL bookmarks, which
        // XEP-0402 intentionally doesn't encode.
        for (auto &room : result) {
            if (account_->ignoreMucBookmarks().contains(room.jid().bare()))
                room.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
        }
        if (result != conferences_) {
            conferences_ = result;
            emit conferencesChanged(conferences_);
        }
    });
    task->go(true);
}

void BookmarkManager::getBookmarks()
{
    BookmarkTask *t = new BookmarkTask(account_->client()->rootTask());
    connect(t, SIGNAL(finished()), SLOT(getBookmarks_finished()));
    t->get();
    t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark> &urls, const QList<ConferenceBookmark> &conferences)
{
    if (pep_->active) {
        if (pep_->writing)
            return; // Do not issue overlapping non-atomic PubSub edits.
        struct Edit {
            bool removal;
            QString room;
            ConferenceBookmark bookmark;
        };
        QList<Edit> edits;
        QHash<QString, ConferenceBookmark> requested;
        for (const auto &room : conferences) {
            if (!room.jid().isValid() || room.jid().resource().size())
                return;
            requested.insert(room.jid().bare(), room);
            const auto old = std::find_if(conferences_.cbegin(), conferences_.cend(),
                [&room](const ConferenceBookmark &b) { return b.jid().bare() == room.jid().bare(); });
            if (old == conferences_.cend() || *old != room)
                edits.append({ false, room.jid().bare(), room });
        }
        for (const auto &room : conferences_)
            if (!requested.contains(room.jid().bare()))
                edits.append({ true, room.jid().bare(), room });
        if (edits.isEmpty()) {
            if (urls != urls_) {
                BookmarkTask *t = new BookmarkTask(account_->client()->rootTask());
                connect(t, SIGNAL(finished()), SLOT(setBookmarks_finished()));
                t->set(urls, conferences_);
                t->go(true);
            }
            return;
        }
        pep_->writing = true;
        auto remaining = std::make_shared<int>(edits.size());
        auto failed = std::make_shared<bool>(false);
        auto originalUpdates = std::make_shared<QHash<QString, XMPP::NativeBookmark>>(pep_->originals);
        auto *pubsub = account_->client()->pubSubManager();
        for (const auto &edit : edits) {
            XMPP::Task *task;
            if (edit.removal) {
                task = pubsub->retract(XMPP::Jid(), XMPP::NativeBookmark::node(), edit.room);
                originalUpdates->remove(edit.room);
            } else {
                XMPP::NativeBookmark native;
                if (originalUpdates->contains(edit.room)) {
                    native = originalUpdates->value(edit.room);
                } else {
                    native = XMPP::NativeBookmark::create(edit.bookmark.jid(), edit.bookmark.name(),
                                                         edit.bookmark.autoJoin() == ConferenceBookmark::Always
                                                             || edit.bookmark.autoJoin() == ConferenceBookmark::ExceptThisComputer,
                                                         edit.bookmark.nick(),
                                                         edit.bookmark.password());
                }
                native.setName(edit.bookmark.name());
                native.setAutoJoin(edit.bookmark.autoJoin() == ConferenceBookmark::Always
                                   || edit.bookmark.autoJoin() == ConferenceBookmark::ExceptThisComputer);
                native.setNick(edit.bookmark.nick());
                native.setPassword(edit.bookmark.password());
                QDomDocument document;
                task = pubsub->publish(XMPP::Jid(), XMPP::NativeBookmark::node(), native.toItem(document),
                                       XMPP::NativeBookmark::privatePublishOptions());
                originalUpdates->insert(edit.room, native);
            }
            connect(task, &XMPP::Task::finished, this,
                    [this, task, remaining, failed, originalUpdates, urls, conferences] {
                if (!task->success())
                    *failed = true;
                if (--*remaining != 0)
                    return;
                pep_->writing = false;
                if (*failed) {
                    // Partial writes may have happened: reload before allowing
                    // another change rather than pretending the batch was atomic.
                    pep_->active = false;
                    loadNativeBookmarks();
                    return;
                }
                pep_->originals = *originalUpdates;
                const bool changed = conferences_ != conferences;
                conferences_ = conferences;
                if (changed)
                    emit conferencesChanged(conferences_);
                emit bookmarksSaved();
                if (urls != urls_)
                    setBookmarks(urls); // private XML retains URL bookmarks.
            });
            task->go(true);
        }
        return;
    }
    BookmarkTask *t = new BookmarkTask(account_->client()->rootTask());
    connect(t, SIGNAL(finished()), SLOT(setBookmarks_finished()));
    t->set(urls, conferences);
    t->go(true);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark> &urls) { setBookmarks(urls, conferences()); }

void BookmarkManager::setBookmarks(const QList<ConferenceBookmark> &conferences) { setBookmarks(urls(), conferences); }

void BookmarkManager::getBookmarks_finished()
{
    BookmarkTask *t = static_cast<BookmarkTask *>(sender());
    if (t->success()) {
        bool urlsWereChanged        = urls_ != t->urls();
        bool conferencesWereChanged = conferences_ != t->conferences();
        urls_                       = t->urls();
        QStringList   localMucs     = account_->localMucBookmarks();
        QStringList   mucIgnore     = account_->ignoreMucBookmarks();
        QSet<QString> strippedMucs;
        for (const QString &m : std::as_const(localMucs)) {
            Jid j(m);
            if (j.isValid()) {
                strippedMucs.insert(j.bare());
            }
        }

        conferences_ = t->conferences();
        for (auto &muc : conferences_) {
            if (strippedMucs.contains(muc.jid().bare())) {
                muc.setAutoJoin(ConferenceBookmark::OnlyThisComputer);
            }
            if (mucIgnore.contains(muc.jid().bare())) {
                muc.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
            }
        }

        setIsAvailable(true);
        maybeEnableNativeBookmarks();
        if (urlsWereChanged)
            emit urlsChanged(urls_);
        if (conferencesWereChanged)
            emit conferencesChanged(conferences_);
    } else {
        setIsAvailable(false);
    }
}

void BookmarkManager::setBookmarks_finished()
{
    BookmarkTask *t = static_cast<BookmarkTask *>(sender());
    if (t->success()) {
        bool conferencesWereChanged = conferences_ != t->conferences();
        bool urlsWereChanged        = urls_ != t->urls();
        conferences_                = t->conferences();
        urls_                       = t->urls();

        QStringList localMucs;
        QStringList ignoreMucs;

        for (const ConferenceBookmark &cb : std::as_const(conferences_)) {
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
