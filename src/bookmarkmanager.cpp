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
#include "iris/xmpp_pubsubreplica.h"
#include "iris/xmpp_serverinfomanager.h"
#include "iris/xmpp_task.h"
#include "iris/xmpp_xmlcommon.h"
#include "psiaccount.h"

#include <optional>
#include <QTimer>

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
    bool nativeWasActive = false;
    bool legacyPending = false;
    int retryDelayMs = 0;
    XMPP::PubSubReplica<XMPP::NativeBookmark> replica;
    // Local-only MUC join preferences accompany queued native item edits;
    // they do not belong in the shared PEP payload or generic replica.
    QHash<QString, std::optional<ConferenceBookmark>> preferences;
    QHash<QString, std::optional<ConferenceBookmark>> inFlightPreferences;
    QList<URLBookmark> queuedUrls;
    bool queuedUrlsValid = false;
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
        if ((!pep_->active && pep_->replica.state() != XMPP::PubSubReplica<XMPP::NativeBookmark>::State::Loading)
            || service.bare() != account_->jid().bare() || node != XMPP::NativeBookmark::node())
            return;
        const auto native = XMPP::NativeBookmark::fromItem(item);
        if (!native.isValid())
            return;
        pep_->replica.published(native.room().bare(), native);
        if (pep_->active)
            syncNativeBookmarks();
    });
    connect(pubsub, &XMPP::PubSubManager::itemRetracted, this,
            [this](const XMPP::Jid &service, const QString &node, const QString &id) {
        if ((!pep_->active && pep_->replica.state() != XMPP::PubSubReplica<XMPP::NativeBookmark>::State::Loading)
            || service.bare() != account_->jid().bare() || node != XMPP::NativeBookmark::node())
            return;
        pep_->replica.retracted(id);
        if (pep_->active)
            syncNativeBookmarks();
    });
    const auto invalidate = [this](const XMPP::Jid &service, const QString &node) {
        if ((!pep_->active && pep_->replica.state() != XMPP::PubSubReplica<XMPP::NativeBookmark>::State::Loading)
            || service.bare() != account_->jid().bare() || node != XMPP::NativeBookmark::node())
            return;
        pep_->replica.invalidate();
        for (auto it = pep_->inFlightPreferences.cbegin(); it != pep_->inFlightPreferences.cend(); ++it)
            if (!pep_->preferences.contains(it.key()))
                pep_->preferences.insert(it.key(), it.value());
        pep_->inFlightPreferences.clear();
        pep_->active = false;
        loadNativeBookmarks();
    };
    connect(pubsub, &XMPP::PubSubManager::nodePurged, this, invalidate);
    connect(pubsub, &XMPP::PubSubManager::nodeDeleted, this, invalidate);
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
        pep_->nativeWasActive = false;
        pep_->legacyPending = false;
        pep_->retryDelayMs = 0;
        pep_->replica.reset();
        pep_->preferences.clear();
        pep_->inFlightPreferences.clear();
        pep_->queuedUrlsValid = false;
    }

    if (account_->isAvailable() && !accountAvailable_)
        getBookmarks();

    accountAvailable_ = account_->isAvailable();
}

void BookmarkManager::maybeEnableNativeBookmarks()
{
    using State = XMPP::PubSubReplica<XMPP::NativeBookmark>::State;
    if (!account_->isAvailable() || pep_->legacyPending || pep_->active
        || pep_->replica.state() == State::Loading)
        return;
    auto *server = account_->client()->serverInfoManager();
    // Without #compat legacy and PEP bookmarks can diverge. Do not switch
    // to the PEP source automatically until the server promises to bridge it.
    const bool compatible = server->accountFeatures().test("urn:xmpp:bookmarks:1#compat");
    const bool publishOptions = server->accountFeatures().test("http://jabber.org/protocol/pubsub#publish-options")
        || server->serverFeatures().test("http://jabber.org/protocol/pubsub#publish-options");
    if (compatible && server->hasPEP() && publishOptions)
        loadNativeBookmarks();
}

void BookmarkManager::syncNativeBookmarks()
{
    // Payload knowledge belongs here. The generic replica only knows item IDs
    // and authoritative values, not XEP-0402 fields or local join preferences.
    const auto &items = pep_->replica.items();
    QStringList ids = items.keys();
    ids.sort(Qt::CaseInsensitive);
    QList<ConferenceBookmark> rooms;
    QSet<QString> localOnly;
    for (const auto &jid : account_->localMucBookmarks()) {
        const XMPP::Jid parsed(jid);
        if (parsed.isValid())
            localOnly.insert(parsed.bare());
    }
    const auto ignored = account_->ignoreMucBookmarks();
    for (const auto &id : ids) {
        const auto &native = items[id];
        ConferenceBookmark room(native.name(), native.room(),
            native.autoJoin() ? ConferenceBookmark::Always : ConferenceBookmark::Never,
            native.nick(), native.password());
        if (localOnly.contains(id))
            room.setAutoJoin(ConferenceBookmark::OnlyThisComputer);
        else if (ignored.contains(id))
            room.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
        rooms.append(room);
    }
    if (conferences_ != rooms) {
        conferences_ = rooms;
        emit conferencesChanged(conferences_);
    }
}

void BookmarkManager::loadNativeBookmarks()
{
    const auto loadToken = pep_->replica.beginLoad();
    if (!loadToken)
        return;
    auto *task = account_->client()->pubSubManager()->items(XMPP::Jid(), XMPP::NativeBookmark::node());
    connect(task, &XMPP::PubSubItemsTask::finished, this, [this, task, loadToken] {
        if (!account_->isAvailable())
            return;
        const bool missingNode = !task->success()
            && task->error().condition == XMPP::Stanza::Error::ErrorCond::ItemNotFound;
        if (!task->success() && !(pep_->nativeWasActive && missingNode)) {
            if (pep_->replica.failLoad(loadToken) && pep_->nativeWasActive) {
                // Retain all pending native intents across transient errors.
                // Retry with bounded exponential backoff, not via Private XML.
                pep_->retryDelayMs = pep_->retryDelayMs ? qMin(pep_->retryDelayMs * 2, 60000) : 2000;
                const auto epoch = pep_->replica.epoch();
                QTimer::singleShot(pep_->retryDelayMs, this, [this, epoch] {
                    if (account_->isAvailable() && epoch == pep_->replica.epoch() && !pep_->active
                        && pep_->replica.state() == XMPP::PubSubReplica<XMPP::NativeBookmark>::State::ResyncRequired)
                        loadNativeBookmarks();
                });
            }
            return;
        }
        XMPP::PubSubReplica<XMPP::NativeBookmark>::Items items;
        for (const auto &item : task->items()) {
            const auto native = XMPP::NativeBookmark::fromItem(item);
            if (native.isValid())
                items.insert(native.room().bare(), native);
        }
        // An empty native node combined with nonempty legacy bookmarks does
        // not prove those rooms should be imported: a different device could
        // have explicitly deleted them. Stay with Private XML in that case.
        if (!pep_->replica.finishLoad(loadToken, std::move(items)))
            return; // Stale response from an earlier connection/reload.
        // Inspect the reconciled snapshot, not merely IQ items: a push might
        // have populated an initially empty node while the fetch was in flight.
        if (pep_->replica.items().isEmpty() && !conferences_.isEmpty() && !pep_->nativeWasActive) {
            pep_->replica.invalidate();
            return;
        }
        pep_->active = true;
        pep_->nativeWasActive = true;
        pep_->retryDelayMs = 0;
        setIsAvailable(true);
        syncNativeBookmarks();
        flushQueuedBookmarks();
    });
    task->go(true);
}

void BookmarkManager::getBookmarks()
{
    pep_->legacyPending = true;
    BookmarkTask *t = new BookmarkTask(account_->client()->rootTask());
    t->setProperty("bookmarkGeneration", QVariant::fromValue<qulonglong>(pep_->replica.epoch()));
    connect(t, SIGNAL(finished()), SLOT(getBookmarks_finished()));
    t->get();
    t->go(true);
}

void BookmarkManager::applyConferenceEdits(const QList<ConferenceBookmark> &baseline,
                                           const QList<ConferenceBookmark> &edited)
{
    QHash<QString, ConferenceBookmark> before;
    QHash<QString, ConferenceBookmark> after;
    for (const auto &room : baseline)
        before.insert(room.jid().bare(), room);
    for (const auto &room : edited) {
        if (!room.jid().isValid() || room.jid().isEmpty() || !room.jid().resource().isEmpty()
            || after.contains(room.jid().bare()))
            return; // Do not allow an invalid/ambiguous UI snapshot to erase remote data.
        after.insert(room.jid().bare(), room);
    }

    auto merged = conferences_; // current state includes concurrent PEP events
    const auto eraseRoom = [&merged](const QString &id) {
        merged.erase(std::remove_if(merged.begin(), merged.end(),
            [&id](const ConferenceBookmark &room) { return room.jid().bare() == id; }), merged.end());
    };
    for (auto it = before.cbegin(); it != before.cend(); ++it)
        if (!after.contains(it.key()))
            eraseRoom(it.key());
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        const auto old = before.constFind(it.key());
        if (old != before.cend() && old.value() == it.value())
            continue; // A remote update to an untouched room wins.
        eraseRoom(it.key());
        merged.append(it.value());
    }
    setBookmarks(merged);
}

void BookmarkManager::setBookmarks(const QList<URLBookmark> &urls, const QList<ConferenceBookmark> &conferences)
{
    if (!pep_->active && !pep_->nativeWasActive) {
        BookmarkTask *task = new BookmarkTask(account_->client()->rootTask());
        task->setProperty("bookmarkGeneration", QVariant::fromValue<qulonglong>(pep_->replica.epoch()));
        connect(task, SIGNAL(finished()), SLOT(setBookmarks_finished()));
        task->set(urls, conferences);
        task->go(true);
        return;
    }

    // The UI sends a complete list, but a PEP edit is always item-scoped.
    // Never retract a newly pushed room merely because it was not in the
    // earlier snapshot supplied by the UI.
    QHash<QString, ConferenceBookmark> requested;
    for (const auto &room : conferences) {
        if (!room.jid().isValid() || room.jid().isEmpty() || !room.jid().resource().isEmpty())
            return;
        requested.insert(room.jid().bare(), room);
    }
    const auto &currentItems = pep_->replica.items();
    for (const auto &room : conferences) {
        const QString id = room.jid().bare();
        const auto old = std::find_if(conferences_.cbegin(), conferences_.cend(),
            [&id](const ConferenceBookmark &candidate) { return candidate.jid().bare() == id; });
        if (old != conferences_.cend() && *old == room)
            continue;
        auto native = currentItems.value(id);
        if (!native.isValid())
            native = XMPP::NativeBookmark::create(room.jid(), room.name(), false);
        native.setName(room.name());
        native.setAutoJoin(room.autoJoin() == ConferenceBookmark::Always
                           || room.autoJoin() == ConferenceBookmark::ExceptThisComputer);
        native.setNick(room.nick());
        native.setPassword(room.password());
        pep_->replica.put(id, native);
        pep_->preferences.insert(id, room);
    }
    for (const auto &old : conferences_) {
        const auto id = old.jid().bare();
        if (!requested.contains(id)) {
            pep_->replica.remove(id);
            pep_->preferences.insert(id, std::nullopt);
        }
    }
    if (urls != urls_) {
        pep_->queuedUrls = urls;
        pep_->queuedUrlsValid = true;
    }
    flushQueuedBookmarks();
    if (!pep_->active
        && pep_->replica.state() != XMPP::PubSubReplica<XMPP::NativeBookmark>::State::Loading
        && account_->isAvailable())
        loadNativeBookmarks(); // Native writes must never fall back to Private XML after activation.
}

void BookmarkManager::flushQueuedBookmarks()
{
    if (!pep_->active || pep_->replica.state() != XMPP::PubSubReplica<XMPP::NativeBookmark>::State::Ready)
        return;

    const auto batch = pep_->replica.beginWrite();
    if (!batch.isValid()) {
        if (pep_->queuedUrlsValid) {
            // XEP-0402 has no URL items. Preserve the existing Private XML
            // path for URL bookmarks; servers offering #compat must reconcile
            // that legacy storage with their native bookmark node.
            BookmarkTask *task = new BookmarkTask(account_->client()->rootTask());
            task->setProperty("bookmarkGeneration", QVariant::fromValue<qulonglong>(pep_->replica.epoch()));
            connect(task, SIGNAL(finished()), SLOT(setBookmarks_finished()));
            task->set(pep_->queuedUrls, conferences_);
            pep_->queuedUrlsValid = false;
            task->go(true);
        }
        return;
    }

    // The replica owns transport ordering, while per-device MUC preferences
    // are kept separately in this consumer (not published to PEP).
    QHash<QString, std::optional<ConferenceBookmark>> preferences;
    for (auto it = batch.changes.cbegin(); it != batch.changes.cend(); ++it) {
        auto pref = pep_->preferences.find(it.key());
        if (pref != pep_->preferences.end()) {
            preferences.insert(it.key(), pref.value());
            pep_->preferences.erase(pref);
        }
    }

    pep_->inFlightPreferences = preferences;
    auto remaining = std::make_shared<int>(batch.changes.size());
    auto failed = std::make_shared<bool>(false);
    auto *pubsub = account_->client()->pubSubManager();

    for (auto it = batch.changes.cbegin(); it != batch.changes.cend(); ++it) {
        XMPP::Task *task = nullptr;
        if (!it.value()) {
            task = pubsub->retract(XMPP::Jid(), XMPP::NativeBookmark::node(), it.key());
        } else {
            QDomDocument doc;
            task = pubsub->publish(XMPP::Jid(), XMPP::NativeBookmark::node(),
                it.value()->toItem(doc), XMPP::NativeBookmark::privatePublishOptions());
        }
        connect(task, &XMPP::Task::finished, this,
                [this, task, remaining, failed, batch, preferences] {
            if (!task->success())
                *failed = true;
            if (--*remaining != 0)
                return;
            if (!pep_->replica.finishWrite(batch, !*failed))
                return; // An earlier account epoch or invalidated node.
            pep_->inFlightPreferences.clear();
            if (*failed) {
                // Some IQs may have succeeded: refetch before retrying queued
                // user intents. A later local edit takes precedence.
                for (auto it = preferences.cbegin(); it != preferences.cend(); ++it)
                    if (!pep_->preferences.contains(it.key()))
                        pep_->preferences.insert(it.key(), it.value());
                pep_->active = false;
                loadNativeBookmarks();
                return;
            }

            // Persist only this device's flags for the touched room IDs.
            auto localMucs = account_->localMucBookmarks();
            auto ignoreMucs = account_->ignoreMucBookmarks();
            for (auto it = preferences.cbegin(); it != preferences.cend(); ++it) {
                const QString id = it.key();
                for (auto local = localMucs.begin(); local != localMucs.end();) {
                    if (XMPP::Jid(*local).bare() == id)
                        local = localMucs.erase(local);
                    else
                        ++local;
                }
                ignoreMucs.removeAll(id);
                if (it.value()) {
                    const auto &room = *it.value();
                    if (room.autoJoin() == ConferenceBookmark::OnlyThisComputer)
                        localMucs.append(room.jid().withResource(room.nick()).full());
                    else if (room.autoJoin() == ConferenceBookmark::ExceptThisComputer)
                        ignoreMucs.append(id);
                }
            }
            account_->setLocalMucBookmarks(localMucs);
            account_->setIgnoreMucBookmarks(ignoreMucs);
            syncNativeBookmarks();
            emit bookmarksSaved();
            flushQueuedBookmarks();
        });
        task->go(true);
    }
}

void BookmarkManager::setBookmarks(const QList<URLBookmark> &urls) { setBookmarks(urls, conferences()); }

void BookmarkManager::setBookmarks(const QList<ConferenceBookmark> &conferences) { setBookmarks(urls(), conferences); }

void BookmarkManager::getBookmarks_finished()
{
    BookmarkTask *t = static_cast<BookmarkTask *>(sender());
    if (t->property("bookmarkGeneration").toULongLong() != pep_->replica.epoch())
        return;
    pep_->legacyPending = false;
    if (t->success()) {
        const bool urlsWereChanged = urls_ != t->urls();
        urls_ = t->urls();
        bool conferencesWereChanged = false;
        if (!pep_->active) {
            auto conferences = t->conferences();
            QSet<QString> localOnlyRooms;
            for (const auto &localJid : account_->localMucBookmarks()) {
                const Jid jid(localJid);
                if (jid.isValid())
                    localOnlyRooms.insert(jid.bare());
            }
            const QStringList ignoreRooms = account_->ignoreMucBookmarks();
            for (auto &room : conferences) {
                if (localOnlyRooms.contains(room.jid().bare()))
                    room.setAutoJoin(ConferenceBookmark::OnlyThisComputer);
                else if (ignoreRooms.contains(room.jid().bare()))
                    room.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
            }
            conferencesWereChanged = conferences_ != conferences;
            conferences_ = conferences;
        }
        setIsAvailable(true);
        if (urlsWereChanged)
            emit urlsChanged(urls_);
        if (conferencesWereChanged)
            emit conferencesChanged(conferences_);
    } else if (!pep_->active) {
        setIsAvailable(false);
    }
    // XEP-0402 can work without legacy Private XML or #compat.
    maybeEnableNativeBookmarks();
}

void BookmarkManager::setBookmarks_finished()
{
    BookmarkTask *t = static_cast<BookmarkTask *>(sender());
    if (t->property("bookmarkGeneration").toULongLong() != pep_->replica.epoch())
        return;
    if (t->success()) {
        const bool urlsWereChanged = urls_ != t->urls();
        urls_ = t->urls();
        if (pep_->active) {
            // URL-only Private XML writes must not overwrite newer PEP events.
            if (urlsWereChanged)
                emit urlsChanged(urls_);
            emit bookmarksSaved();
            return;
        }
        const bool conferencesWereChanged = conferences_ != t->conferences();
        conferences_ = t->conferences();

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
