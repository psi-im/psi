// Adapted from Ri0n/AnyKeep PR #156, db77d42ef791225839d03ecb0c62cd06462b31e3.
// Original implementation: Sergei Ilinykh (AnyKeep).
#ifndef PSI_MEDIACHUNKSCHEDULER_H
#define PSI_MEDIACHUNKSCHEDULER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>

// App-thread admission and single-flight for immutable media chunks.
// Identity MUST include the provider/account, immutable content identity and
// chunk geometry. Each requester owns a cancellable waiter, not a
// network transfer. Providers keep transport verification and cache ownership.
class MediaChunkScheduler final : public QObject {
public:
    using Completion      = std::function<void(QByteArray, QString)>;
    using Fetch           = std::function<void(Completion)>;
    using Abort           = std::function<void()>;
    using CancelableFetch = std::function<Abort(Completion)>;

    explicit MediaChunkScheduler(QObject *parent = nullptr);
    ~MediaChunkScheduler() override;

    static MediaChunkScheduler *instance();

    // Starts fetch once for an uncached chunk, then distributes its validated
    // result to all registered waiters. Fetch MUST complete exactly once and
    // its Completion may be invoked from any thread.
    quint64 request(QString immutableSource, quint64 index, QString lane, int priority, Fetch fetch, Completion done,
                    QObject *fetchOwner = nullptr);
    // A cancelable provider returns an owner-thread-safe abort request. The
    // active slot remains reserved until its completion acknowledges abort.
    quint64 requestCancelable(QString immutableSource, quint64 index, QString lane, int priority, CancelableFetch fetch,
                              Completion done, QObject *fetchOwner = nullptr);

    // Cancels only this consumer. An already active network operation stays
    // accounted for until the provider completes it; never abort a shared
    // download on behalf of another consumer.
    bool cancel(quint64 ticket);
    // Only after a provider's network worker has stopped: discard an
    // orphaned in-flight request whose completion can no longer arrive.
    // Must not be used as ordinary player cancellation.
    bool abandonStopped(quint64 ticket);
    // Called only AFTER a provider's network worker has stopped. Retires
    // every queued/active fetch owned by it, including canceled jobs with no
    // remaining waiters. Late network completions are safely ignored.
    void retireStoppedOwner(QObject *fetchOwner);
    bool reprioritize(quint64 ticket, int priority);

    void setMaxActive(int count);
    void setLaneMaxActive(const QString &lane, int count);
    // Observe an already negotiated, non-preemptible physical transport.
    // Jingle BUNDLE contents sharing a session count only once; overlapping
    // independent sessions count separately. Observations may temporarily
    // exceed the target global budget instead of disrupting SCTP handovers.
    void observeExternalTransfer(QObject *physicalTransport);
    void releaseExternalTransfer(QObject *physicalTransport);
    int  externalActiveCount() const { return externalTransfers_.size(); }
    int  activeCount() const { return active_ + externalTransfers_.size(); }
    int  pendingCount() const;
    void shutdown();

private:
    struct Waiter {
        quint64    ticket;
        int        priority;
        Completion done;
    };
    struct Job {
        QString           key;
        QString           lane;
        quint64           sequence = 0;
        Fetch             fetch;
        CancelableFetch   cancelableFetch;
        Abort             abort;
        bool              aborting = false;
        QPointer<QObject> fetchOwner;
        QList<Waiter>     waiters;
        bool              active = false;
    };
    static QString makeKey(const QString &source, quint64 index);
    quint64        addWaiter(QString immutableSource, quint64 index, QString lane, int priority, Fetch fetch,
                             CancelableFetch cancelableFetch, Completion done, QObject *fetchOwner);
    void           schedule();
    void           pump();
    void           finish(const QString &key, quint64 sequence, QByteArray bytes, QString error);

    QHash<QString, std::shared_ptr<Job>> jobs_;
    QHash<quint64, QString>              ticketKeys_;
    QHash<QString, int>                  laneLimits_;
    QHash<QString, int>                  laneActive_;
    QSet<QObject *>                      externalTransfers_;
    quint64                              nextTicket_   = 0;
    quint64                              nextSequence_ = 0;
    int                                  maxActive_    = 3;
    int                                  active_       = 0;
    bool                                 scheduled_    = false;
    bool                                 stopped_      = false;
};

#endif // PSI_MEDIACHUNKSCHEDULER_H
