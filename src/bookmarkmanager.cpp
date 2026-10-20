/*
 * bookmarkmanager.cpp - Psi bookmark presentation and local MUC preferences
 * Copyright (C) 2006-2008 Remko Troncon, Michail Pishchagin
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "bookmarkmanager.h"

#include "iris/xmpp_bookmarkmanager.h"
#include "psiaccount.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {
XMPP::NativeBookmark asNative(const ConferenceBookmark &room)
{
    return XMPP::NativeBookmark::create(room.jid(), room.name(),
        room.autoJoin() == ConferenceBookmark::Always
            || room.autoJoin() == ConferenceBookmark::ExceptThisComputer,
        room.nick(), room.password());
}

QList<XMPP::NativeBookmark> asNative(const QList<ConferenceBookmark> &rooms)
{
    QList<XMPP::NativeBookmark> result;
    result.reserve(rooms.size());
    for (const auto &room : rooms)
        result.append(asNative(room));
    return result;
}

QList<XMPP::BookmarkUrl> asIrisUrls(const QList<URLBookmark> &urls)
{
    QList<XMPP::BookmarkUrl> result;
    result.reserve(urls.size());
    for (const auto &url : urls)
        result.append({ url.name(), url.url() });
    return result;
}

bool validConferences(const QList<ConferenceBookmark> &rooms)
{
    QSet<QString> ids;
    for (const auto &room : rooms) {
        if (!room.jid().isValid() || room.jid().isEmpty() || !room.jid().resource().isEmpty()
            || ids.contains(room.jid().bare()))
            return false;
        ids.insert(room.jid().bare());
    }
    return true;
}
} // namespace

class BookmarkManager::Private {
public:
    PsiAccount *account = nullptr;
    XMPP::BookmarksManager *protocol = nullptr;
    QList<URLBookmark> urls;
    QList<ConferenceBookmark> conferences;
};

BookmarkManager::BookmarkManager(PsiAccount *account) : QObject(account), d(std::make_unique<Private>())
{
    d->account = account;
    d->protocol = new XMPP::BookmarksManager(account->client(), this);

    connect(account, &PsiAccount::updatedActivity, this, [this] {
        d->protocol->setOnline(d->account->isAvailable());
    });
    connect(d->protocol, &XMPP::BookmarksManager::availabilityChanged, this,
            [this](bool) { emit availabilityChanged(); });
    connect(d->protocol, &XMPP::BookmarksManager::conferencesChanged,
            this, &BookmarkManager::syncConferences);
    connect(d->protocol, &XMPP::BookmarksManager::urlsChanged,
            this, &BookmarkManager::syncUrls);
    connect(d->protocol, &XMPP::BookmarksManager::bookmarksSaved,
            this, &BookmarkManager::bookmarksSaved);
    connect(d->protocol, &XMPP::BookmarksManager::syncError,
            this, &BookmarkManager::syncError);
    connect(d->protocol, &XMPP::BookmarksManager::conferenceUpdated, this,
            [this](const XMPP::NativeBookmark &room, XMPP::BookmarksManager::ChangeSource source) {
        if (source == XMPP::BookmarksManager::ChangeSource::Push)
            emit remoteConferenceChanged(room.room(), room.autoJoin(), false);
    });
    connect(d->protocol, &XMPP::BookmarksManager::conferenceRemoved, this,
            [this](const XMPP::Jid &room, XMPP::BookmarksManager::ChangeSource source) {
        if (source == XMPP::BookmarksManager::ChangeSource::Push)
            emit remoteConferenceChanged(room, false, true);
    });
    d->protocol->setOnline(account->isAvailable());
}

BookmarkManager::~BookmarkManager() = default;
bool BookmarkManager::isAvailable() const { return d->protocol->isAvailable(); }
bool BookmarkManager::isBookmarked(const XMPP::Jid &jid) { return indexOfConference(jid) >= 0; }

const QList<URLBookmark> &BookmarkManager::urls() const { return d->urls; }
const QList<ConferenceBookmark> &BookmarkManager::conferences() const { return d->conferences; }

int BookmarkManager::indexOfConference(const XMPP::Jid &jid) const
{
    if (isAvailable()) {
        for (int i = 0; i < d->conferences.size(); ++i)
            if (d->conferences.at(i).jid().compare(jid, false))
                return i;
    }
    return -1;
}

QString BookmarkManager::conferenceName(const XMPP::Jid &jid) const
{
    const int index = indexOfConference(jid);
    return index < 0 ? QString() : d->conferences.at(index).name();
}

void BookmarkManager::syncUrls()
{
    QList<URLBookmark> result;
    for (const auto &url : d->protocol->urls())
        result.append(URLBookmark(url.name, url.url));
    if (result == d->urls)
        return;
    d->urls = result;
    emit urlsChanged(d->urls);
}

void BookmarkManager::syncConferences()
{
    QSet<QString> localOnly;
    for (const auto &entry : d->account->localMucBookmarks()) {
        const XMPP::Jid local(entry);
        if (local.isValid())
            localOnly.insert(local.bare());
    }
    const auto ignored = d->account->ignoreMucBookmarks();
    QList<ConferenceBookmark> rooms;
    for (const auto &native : d->protocol->conferences()) {
        ConferenceBookmark bookmark(native.name(), native.room(),
            native.autoJoin() ? ConferenceBookmark::Always : ConferenceBookmark::Never,
            native.nick(), native.password());
        if (localOnly.contains(native.room().bare()))
            bookmark.setAutoJoin(ConferenceBookmark::OnlyThisComputer);
        else if (ignored.contains(native.room().bare()))
            bookmark.setAutoJoin(ConferenceBookmark::ExceptThisComputer);
        rooms.append(bookmark);
    }
    if (d->conferences == rooms)
        return;
    d->conferences = rooms;
    emit conferencesChanged(d->conferences);
}

void BookmarkManager::updateLocalPreferences(const QList<ConferenceBookmark> &baseline,
                                             const QList<ConferenceBookmark> &edited)
{
    QHash<QString, ConferenceBookmark> old;
    for (const auto &room : baseline)
        old.insert(room.jid().bare(), room);

    auto local = d->account->localMucBookmarks();
    auto ignored = d->account->ignoreMucBookmarks();
    bool changed = false;
    const auto update = [&local, &ignored, &changed](const QString &id,
                                                     const ConferenceBookmark *room) {
        for (auto it = local.begin(); it != local.end();) {
            if (XMPP::Jid(*it).bare() == id) {
                it = local.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        if (ignored.removeAll(id))
            changed = true;
        if (room) {
            if (room->autoJoin() == ConferenceBookmark::OnlyThisComputer) {
                local.append(room->jid().withResource(room->nick()).full());
                changed = true;
            } else if (room->autoJoin() == ConferenceBookmark::ExceptThisComputer) {
                ignored.append(id);
                changed = true;
            }
        }
    };

    QSet<QString> requested;
    for (const auto &room : edited) {
        const QString id = room.jid().bare();
        requested.insert(id);
        const auto prev = old.constFind(id);
        if (prev == old.cend() || !(prev.value() == room))
            update(id, &room);
    }
    for (const auto &room : baseline)
        if (!requested.contains(room.jid().bare()))
            update(room.jid().bare(), nullptr);
    if (changed) {
        d->account->setLocalMucBookmarks(local);
        d->account->setIgnoreMucBookmarks(ignored);
        syncConferences();
    }
}

bool BookmarkManager::setBookmarks(const QList<URLBookmark> &urls,
                                    const QList<ConferenceBookmark> &conferences)
{
    if (!validConferences(conferences)) {
        emit syncError(QStringLiteral("Invalid conference bookmark list"));
        return false;
    }
    // Never treat URL save success as implicit consent to a destructive
    // Private XML write when the server uses XEP-0402.
    const auto old = d->conferences;
    if (!d->protocol->setBookmarks(asIrisUrls(urls), asNative(conferences)))
        return false;
    updateLocalPreferences(old, conferences);
    return true;
}

bool BookmarkManager::setBookmarks(const QList<URLBookmark> &urls)
{
    return d->protocol->setUrls(asIrisUrls(urls));
}

bool BookmarkManager::setBookmarks(const QList<ConferenceBookmark> &conferences)
{
    if (!validConferences(conferences)) {
        emit syncError(QStringLiteral("Invalid conference bookmark list"));
        return false;
    }
    const auto old = d->conferences;
    if (!d->protocol->setConferences(asNative(conferences)))
        return false;
    updateLocalPreferences(old, conferences);
    return true;
}

bool BookmarkManager::applyConferenceEdits(const QList<ConferenceBookmark> &baseline,
                                           const QList<ConferenceBookmark> &edited)
{
    if (!validConferences(edited)) {
        emit syncError(QStringLiteral("Invalid conference bookmark list"));
        return false;
    }
    if (!d->protocol->applyConferenceEdits(asNative(baseline), asNative(edited)))
        return false;
    updateLocalPreferences(baseline, edited);
    return true;
}

void BookmarkManager::addConference(const ConferenceBookmark &room)
{
    if (!isAvailable() || room.isNull() || indexOfConference(room.jid()) >= 0)
        return;
    const auto before = d->conferences;
    if (d->protocol->upsertConference(asNative(room))) {
        auto after = before;
        after.append(room);
        updateLocalPreferences(before, after);
    }
}

void BookmarkManager::removeConference(const XMPP::Jid &jid)
{
    if (!isAvailable())
        return;
    const auto before = d->conferences;
    if (d->protocol->removeConference(jid)) {
        auto after = before;
        after.erase(std::remove_if(after.begin(), after.end(),
            [&jid](const ConferenceBookmark &room) { return room.jid().compare(jid, false); }), after.end());
        updateLocalPreferences(before, after);
    }
}
