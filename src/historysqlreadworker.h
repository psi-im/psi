/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchive.h"
#include "historyeventpayload.h"
#include "historymediaqueries.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QString>
#include <QTimer>
#include <utility>
#include <functional>

// Only immutable value objects cross the EDB reader's queued connections.
// No PsiEvent, PsiAccount or SQLite connection ever crosses into the worker.
struct HistoryReadRequest {
    enum Type {
        Get,
        Find,
        LocalPage,
        DisplayedCursor,
        ArchiveBefore,
        ArchiveAfter,
        ArchiveIndex,
        ResolveBatch,
        ArchiveLookup,
        OwnPlaintext,
        MediaLinks,
        ChatMediaLinks,
        ChatMediaStatistics,
        MediaLinked
    };
    int id = 0;
    Type type = Get;
    QString account, peer, text;
    HistoryArchive::Key archiveKey;
    QString cursor;
    QString fileDatabasePath;
    HistoryMediaRefs::Cursor mediaCursor;
    QString sender, messageId;
    QDateTime messageTime;
    int archiveIndex = -1, snapshotCount = -1;
    QList<qint64> eventIds;
    QList<QByteArray>   publicationIds;
    QDateTime date;
    qint64 before = 0;
    int direction = 0, start = 0, limit = 40;
};
Q_DECLARE_METATYPE(HistoryReadRequest)

struct HistoryReadResult {
    int id = 0;
    bool success = false;
    int beginRow = 0;
    QList<QSqlRecord> rows;
    HistoryArchive::DisplayedCursor cursor;
    HistoryArchive::Page archivePage;
    HistoryArchive::Snapshot archiveSnapshot;
    QHash<qint64, QSqlRecord> resolvedRows;
    bool archiveExact = false;
    QList<HistoryMediaRefs::Link>   mediaLinks;
    QSet<QByteArray>                linkedPublications;
    HistoryMediaRefs::Statistics mediaStatistics;
};
Q_DECLARE_METATYPE(HistoryReadResult)

struct HistoryWriteRequest {
    enum Type { Append, Erase, StorePage, DeleteLocal, CheckSuppression };
    int id = 0;
    Type type = Append;
    QString account, peer, contactPeer, resource;
    int jidType = 0;
    HistoryEventFactory::Encoded encoded;
    HistoryArchive::Key archiveKey;
    HistoryArchive::Page archivePage;
    QString before;
    int limit = 0;
    qint64                       eventId = 0;
    HistoryArchive::Entry        identity;
};
Q_DECLARE_METATYPE(HistoryWriteRequest)

struct HistoryWriteResult {
    int id = 0;
    bool success = false;
    bool inserted = false;
    bool                 mediaLinksRemoved = false;
    HistoryArchive::Key                 changedKey;
    HistoryArchive::LocalDeletionResult localDeletion;
    HistoryArchive::Page archivePage;
};
Q_DECLARE_METATYPE(HistoryWriteResult)

class HistorySqlReadWorker final : public QObject {
    Q_OBJECT
public:
    explicit HistorySqlReadWorker(QString path) : path_(std::move(path)) { }

    // Called before moveToThread; no GUI-owned data is captured.
    void setEventStoredHook(std::function<bool(QSqlDatabase, qint64, bool *)> hook)
    {
        eventStoredHook_ = std::move(hook);
    }

public slots:
    void open();
    void read(HistoryReadRequest request);
    void write(HistoryWriteRequest request);
    void close();

signals:
    void completed(HistoryReadResult result);
    void writeCompleted(HistoryWriteResult result);
    void stopped();

private:
    // All batching state belongs to the worker thread. No shared mutex or
    // GUI-side SQL transaction is involved.
    void flushPendingAppends();
    QString path_, connectionName_;
    QSqlDatabase db_;
    std::function<bool(QSqlDatabase, qint64, bool *)> eventStoredHook_;
    QTimer *appendCommitTimer_ = nullptr;
    QList<HistoryWriteResult> pendingAppends_;
    bool appendTransactionOpen_ = false;
};
