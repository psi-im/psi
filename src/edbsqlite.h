/*
 * edbsqlite.h
 * Copyright (C) 2011  Aleksey Andreev
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

#ifndef EDBSQLITE_H
#define EDBSQLITE_H

#include "eventdb.h"
#include "historyeventfactory.h"
#include "historysqlreadworker.h"
#include "iris/xmpp_jid.h"
#include "psievent.h"

#include <QDateTime>

class QThread;
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QVariant>

class EDBSqLite : public EDB {
    Q_OBJECT


public:
    EDBSqLite(PsiCon *psi);
    ~EDBSqLite();
    bool    init();
    void       requestDeleteLocalMessage(const HistoryArchive::Key &, qint64, const QString &, int, QObject *,
                                         std::function<void(bool)>) override;
    void       requestLocalSuppression(const HistoryArchive::Key &, HistoryArchive::Entry, QObject *,
                                       std::function<void(bool, bool)>) override;
    void       requestMediaLinks(qint64 position, int limit, QObject *receiver,
                                 std::function<void(QList<HistoryMediaRefs::Link>, bool)> finished);
    void       requestLinkedMedia(const QList<QByteArray> &ids, QObject *receiver,
                                  std::function<void(QSet<QByteArray>, bool)> finished);
    EDBItemPtr archivedEvent(const HistoryArchive::Key &, const QString &, const QDateTime & = {}) override;
    void requestArchivedEvent(const HistoryArchive::Key &, const QString &, const QDateTime &, QObject *,
                              std::function<void(EDBItemPtr, bool)>) override;
    void requestOwnPlaintext(const HistoryArchive::Key &, const QString &, const QString &, const QDateTime &,
                             QObject *, std::function<void(EDBItemPtr, bool)>) override;
    EDBItemPtr ownPlaintext(const HistoryArchive::Key &, const QString &, const QString &, const QDateTime &) override;
    HistoryArchive::DisplayedCursor sentDisplayedCursor(const QString &, const XMPP::Jid &) override;
    void requestSentDisplayedCursor(const QString &, const XMPP::Jid &, QObject *,
                                    std::function<void(HistoryArchive::DisplayedCursor, bool)>) override;

    int features() const override;
    int get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int start, int len) override;
    int find(const QString &accId, const QString &str, const XMPP::Jid &jid, const QDateTime date, int direction) override;
    int append(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int type) override;
    int erase(const QString &accId, const XMPP::Jid &jid) override;
    QList<ContactItem> contacts(const QString &accId, int type) override;
    quint64            eventsCount(const QString &accId, const XMPP::Jid &jid) override;
    QString            getStorageParam(const QString &key) override;
    void               setStorageParam(const QString &key, const QString &val) override;

    EDBResult            localHistoryPage(const QString &, const XMPP::Jid &, qint64, int) override;
    void                 requestLocalHistoryPage(const QString &, const XMPP::Jid &, qint64, int, QObject *,
                                                 std::function<void(EDBResult, bool)>) override;
    EDBItemPtr            resolvedHistoryEvent(qint64 eventId) override;
    void                  requestResolvedHistoryEvents(const QList<qint64> &, QObject *,
                                                      std::function<void(QHash<qint64, EDBItemPtr>, bool)>) override;
    HistoryArchive::Snapshot archiveSnapshot(const HistoryArchive::Key &) override;
    bool                 saveArchiveSnapshot(const HistoryArchive::Key &, const HistoryArchive::Snapshot &) override;
    HistoryArchive::Page archivePage(const HistoryArchive::Key &, const QString &, int) override;
    HistoryArchive::Page archivePageAtIndex(const HistoryArchive::Key &, int, int, int) override;
    HistoryArchive::Page archivePageAfter(const HistoryArchive::Key &, const QString &, int, int) override;
    bool                 mergeArchiveIsland(const HistoryArchive::Key &, int, HistoryArchive::Page &) override;
    bool mergeArchivePage(const HistoryArchive::Key &, const QString &, int, HistoryArchive::Page &) override;
    void requestArchivePage(ArchiveReadMode, const HistoryArchive::Key &, const QString &, int, int, int, QObject *,
                            std::function<void(HistoryArchive::Page, HistoryArchive::Snapshot, bool)>) override;
    void requestStoreArchivePage(const HistoryArchive::Key &, const QString &, int, HistoryArchive::Page, QObject *,
                                 std::function<void(HistoryArchive::Page, bool)>) override;

private:
    struct item_query_req {
        QString       accId;
        XMPP::Jid     j;
        int           jidType = 0;
        int           type = 0;
        int           start = 0;
        int           len = 0;
        int           dir = 0;
        int           id = 0;
        qint64        localBefore = 0;
        QDateTime     date;
        QString       findStr;
        PsiEvent::Ptr                event; // GUI-side notification after a committed message append.
        HistoryEventFactory::Encoded encoded;
        HistoryArchive::Key archiveKey;
        HistoryArchive::Page archivePage;
        HistoryArchive::Entry        suppressionIdentity;
        QList<qint64> eventIds;
        QList<QByteArray>            publicationIds;
        QString archiveCursor;
        QString ownSender, ownMessageId;
        QDateTime ownTime;
        ArchiveReadMode archiveMode = ArchiveReadMode::Before;
        int archiveIndex = -1, snapshotCount = -1;

        enum Type {
            Type_get,
            Type_append,
            Type_find,
            Type_erase,
            Type_local,
            Type_displayed,
            Type_archiveRead,
            Type_archiveWrite,
            Type_resolved,
            Type_lookup,
            Type_plaintext,
            Type_mediaLinks,
            Type_mediaLinked,
            Type_deleteLocal,
            Type_checkSuppression
        };
    };
    bool                    active_ = false;
    QList<item_query_req *> rlist;
    QThread                 *historyReadThread_ = nullptr;
    HistorySqlReadWorker    *historyReader_ = nullptr;
    QHash<int, item_query_req *> pendingReads_;
    QHash<int, item_query_req *> pendingWrites_;
    struct PendingMediaLinks {
        QPointer<QObject>                                        receiver;
        std::function<void(QList<HistoryMediaRefs::Link>, bool)> finished;
    };
    struct PendingMediaLinked {
        QPointer<QObject>                           receiver;
        std::function<void(QSet<QByteArray>, bool)> finished;
    };
    QHash<int, PendingMediaLinks>  pendingMediaLinks_;
    QHash<int, PendingMediaLinked> pendingMediaLinked_;
    struct PendingLocalDeletion {
        QPointer<QObject>               receiver;
        std::function<void(bool, bool)> finished;
    };
    QHash<int, PendingLocalDeletion> pendingLocalDeletion_;
    struct PendingLocalPage {
        QPointer<QObject> receiver;
        std::function<void(EDBResult, bool)> finished;
    };
    QHash<int, PendingLocalPage> pendingLocalPages_;
    struct PendingDisplayedCursor {
        QPointer<QObject> receiver;
        std::function<void(HistoryArchive::DisplayedCursor, bool)> finished;
    };
    QHash<int, PendingDisplayedCursor> pendingDisplayedCursors_;
    struct PendingArchiveRead {
        QPointer<QObject> receiver;
        std::function<void(HistoryArchive::Page, HistoryArchive::Snapshot, bool)> finished;
    };
    struct PendingArchiveWrite {
        QPointer<QObject> receiver;
        std::function<void(HistoryArchive::Page, bool)> finished;
    };
    QHash<int, PendingArchiveRead> pendingArchiveReads_;
    QHash<int, PendingArchiveWrite> pendingArchiveWrites_;
    struct PendingResolved {
        QPointer<QObject> receiver;
        std::function<void(QHash<qint64, EDBItemPtr>, bool)> finished;
    };
    QHash<int, PendingResolved> pendingResolved_;
    struct PendingMessageLookup {
        QPointer<QObject> receiver;
        std::function<void(EDBItemPtr, bool)> finished;
        HistoryArchive::Key key;
        QString uid;
        bool own = false;
    };
    QHash<int, PendingMessageLookup> pendingMessageLookups_;

private:
    EDBResult     decodeLocalRecords(const QList<QSqlRecord> &rows);

signals:
    // Emitted in the GUI after the worker has successfully committed.
    void mediaLinksAdded();
    void mediaLinksRemoved();
    void historyReadRequested(HistoryReadRequest request);
    void historyWriteRequested(HistoryWriteRequest request);
    void historyReadStopRequested();

private slots:
    void onHistoryReadCompleted(HistoryReadResult result);
    void onHistoryWriteCompleted(HistoryWriteResult result);
    void performRequests();
};

#endif // EDBSQLITE_H
