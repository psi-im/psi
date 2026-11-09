/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchive.h"
#include "iris/xmpp_message.h"
#include <QDomElement>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

class PsiAccount;
namespace XMPP {
class MAMTask;
class Client;
}

// One bounded history page. Keeps archive policy outside Iris and presentation
// outside EDB; both private chats and room timelines use the same cache path.
class HistorySync final : public QObject {
    Q_OBJECT
public:
    struct Query {
        enum Mode { Before, After, Index } mode = Before;
        QString cursor;
        int     index = -1, limit = 40, snapshotCount = -1;
    };
    struct CachedMessage {
        XMPP::Message message;
        bool          protocolOnly = false;
    };
    struct ResolvedMessage {
        XMPP::Message message;
        qint64        historyId = 0;
        MessageState::Flags messageState;
        QDateTime     revisionTime;
        QString       reactionTarget, retractionTarget;
        bool          found = false;
    };
    // Storage/account policy stays outside the protocol coordinator.
    struct Context {
        QString                                                                                accountId;
        XMPP::Jid                                                                              owner;
        XMPP::Client                                                                          *client = nullptr;
        std::function<bool()>                                                                  connected, logging;
        std::function<HistoryArchive::Page(const HistoryArchive::Key &, const QString &, int)> read;
        std::function<bool(const HistoryArchive::Key &, const QString &, int, HistoryArchive::Page &)> merge;
        std::function<HistoryArchive::Snapshot(const HistoryArchive::Key &)>                           readSnapshot;
        std::function<HistoryArchive::Page(const HistoryArchive::Key &, int, int, int)>                readIndex;
        std::function<HistoryArchive::Page(const HistoryArchive::Key &, const QString &, int, int)>    readAfter;
        std::function<CachedMessage(const HistoryArchive::Key &, const QString &, const QDateTime &)>  lookup;
        std::function<CachedMessage(const HistoryArchive::Key &, const QString &, const QString &, const QDateTime &)>
            ownPlaintext;
        // Optional async store API for worker-backed databases. The callback
        // is receiver-scoped; completed pages may update persistent event IDs.
        std::function<void(const HistoryArchive::Key &, Query, QObject *,
                           std::function<void(HistoryArchive::Page, HistoryArchive::Snapshot, bool)>)> readAsync;
        std::function<void(const HistoryArchive::Key &, const QString &, int, HistoryArchive::Page, QObject *,
                           std::function<void(HistoryArchive::Page, bool)>)> mergeAsync;
        std::function<void(const QList<qint64> &, QObject *,
                           std::function<void(QHash<qint64, ResolvedMessage>, bool)>)> resolveAsync;
        // Empty + success means a cache miss; failure must stop replay before
        // a potentially already consumed ciphertext reaches decryption again.
        std::function<void(const HistoryArchive::Key &, const QString &, const QDateTime &, QObject *,
                           std::function<void(CachedMessage, bool)>)>
            lookupAsync;
        std::function<void(const HistoryArchive::Key &, const QString &, const QString &, const QDateTime &, QObject *,
                           std::function<void(CachedMessage, bool)>)>
            ownPlaintextAsync;
    };
    static constexpr int PageSize = 40;
    HistorySync(PsiAccount *, const QString &peer, bool room, const QString &before, QObject *parent, int pageSize = PageSize);
    HistorySync(Context, const QString &peer, bool room, const QString &before, QObject *parent = nullptr, int pageSize = PageSize);
    HistorySync(PsiAccount *, Query, const QString &peer, bool room, QObject *parent = nullptr);
    HistorySync(Context, Query, const QString &peer, bool room, QObject *parent = nullptr);
    ~HistorySync() override;
    void               go();
    void               safeDelete();
    bool               success() const { return success_; }
    bool               isComplete() const { return page_.complete; }
    bool               isStable() const { return page_.stable; }
    bool               fromCache() const { return cached_; }
    QString            statusString() const { return error_; }
    QList<QDomElement> archive() const;
    qint64             eventId(const QString &uid) const;
    ResolvedMessage    resolvedMessage(const QString &uid) const;
    XMPP::Message      message(const QString &uid) const;
    bool               protocolOnly(const QString &uid) const;
    int                requestedPageSize() const { return pageSize_; }
    HistoryArchive::Snapshot archiveSnapshot() const { return snapshot_; }
    int                      resultCount() const
    {
        return cached_ && snapshot_.resultCount >= 0 ? snapshot_.resultCount : page_.resultCount;
    }
    int firstResultIndex() const
    {
        return cached_ && snapshot_.resultCount >= 0 && snapshot_.resultCount != page_.resultCount ? -1
                                                                                                   : page_.firstIndex;
    }
    bool queryComplete() const
    {
        return cached_ ? (query_.mode == Query::Before ? page_.complete : page_.queryComplete) : networkComplete_;
    }

signals:
    void finished();

private:
    QDomElement                   readResult(const QString &) const;
    mutable QList<QDomDocument>   documents_; // Qt5 child elements do not keep ownerDocument alive.
    void                          requestPage();
    HistoryArchive::Snapshot      snapshot_;
    void                          decodeNext();
    void                          acceptPage();
    void                          finishAcceptedPage();
    void                          finishResolvedPage();
    void                          processSavedPage(HistoryArchive::Page);
    void                          finishError(const QString &);
    bool                          parseResults(const QList<QDomElement> &, HistoryArchive::Page &);
    Context                       context_;
    Query                         query_;
    int                           resultCount_ = -1, firstResultIndex_ = -1;
    HistoryArchive::Key           key_;
    QString                       before_, error_;
    QString                       after_;
    QSet<QString>                 visitedAfter_;
    HistoryArchive::Page          tail_;
    QList<QDomElement>            results_;
    QHash<QString, XMPP::Message> decoded_;
    QSet<QString>                 protocolOnly_;
    QSet<QString>                 decryptionFailures_;
    QSet<QString>                 bodyElements_;
    int                           pageSize_        = PageSize;
    int                           decodeIndex_     = 0;
    bool                          networkComplete_ = false, networkStable_ = false;
    bool                          decodingSaved_ = false, replaySaved_ = false;
    QPointer<XMPP::MAMTask>       task_;
    HistoryArchive::Page          page_;
    QHash<qint64, ResolvedMessage> resolved_;
    QHash<QString, CachedMessage> cachedLookups_, ownLookups_;
    HistoryArchive::Page pendingSavedPage_;
    int savedLookupsRemaining_ = 0;
    bool savedLookupsStarted_ = false;
    bool                          success_ = false, cached_ = false, cancelled_ = false;
};
