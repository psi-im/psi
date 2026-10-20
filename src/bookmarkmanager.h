/*
 * bookmarkmanager.h - Psi adapter for the Iris bookmark protocol manager
 * Copyright (C) 2006-2008 Remko Troncon, Michail Pishchagin
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include "conferencebookmark.h"
#include "urlbookmark.h"

#include <QList>
#include <QObject>
#include <memory>

class PsiAccount;

/**
 * Psi-facing bookmark model. XMPP and persistence are handled by
 * XMPP::BookmarksManager in Iris; this class only translates values and
 * applies this device's private MUC join preferences.
 */
class BookmarkManager : public QObject {
    Q_OBJECT
public:
    explicit BookmarkManager(PsiAccount *account);
    ~BookmarkManager() override;

    bool isAvailable() const;
    bool isBookmarked(const XMPP::Jid &);
    const QList<URLBookmark> &urls() const;
    const QList<ConferenceBookmark> &conferences() const;
    int indexOfConference(const XMPP::Jid &) const;
    QString conferenceName(const XMPP::Jid &) const;

    bool setBookmarks(const QList<URLBookmark> &, const QList<ConferenceBookmark> &);
    bool setBookmarks(const QList<URLBookmark> &);
    bool setBookmarks(const QList<ConferenceBookmark> &);
    bool applyConferenceEdits(const QList<ConferenceBookmark> &baseline,
                              const QList<ConferenceBookmark> &edited);
    void addConference(const ConferenceBookmark &);
    void removeConference(const XMPP::Jid &);

signals:
    void availabilityChanged();
    void urlsChanged(const QList<URLBookmark> &);
    void conferencesChanged(const QList<ConferenceBookmark> &);
    void bookmarksSaved();
    void syncError(const QString &message);
    void remoteConferenceChanged(const XMPP::Jid &room, bool autoJoin, bool removed);

private:
    void syncConferences();
    void syncUrls();
    void updateLocalPreferences(const QList<ConferenceBookmark> &baseline,
                                const QList<ConferenceBookmark> &edited);

    class Private;
    std::unique_ptr<Private> d;
};

#endif // BOOKMARKMANAGER_H
