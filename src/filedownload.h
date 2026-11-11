/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "mediachunktypes.h"
#include <QHash>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>

class MediaChunkCache;

// One shared demand controller per account-scoped resource. A reader asks only
// for its current span. A full-download goal independently fills missing spans.
class FileDownload : public QObject {
    Q_OBJECT
public:
    using Completion          = MediaChunkCompletion;
    using GoalCompletion      = std::function<void(const QString &, MediaChunkError)>;
    using Progress            = std::function<void(qreal)>;
    using FullDownloadAllowed = std::function<bool()>;
    using Fetch               = std::function<MediaChunkAbort(quint64, quint64, FullDownloadAllowed, Completion)>;
    FileDownload(MediaChunkCache *cache, QString source, quint64 size, QString lane, Fetch fetch,
                 QObject *parent = nullptr);
    ~FileDownload() override;
    void           setLocalFileResolver(std::function<QString()> resolver) { localFile_ = std::move(resolver); }
    quint64        size() const { return size_; }
    void           setPlayhead(quint64 offset);
    quint64        read(quint64 offset, quint64 length, Completion done);
    // A reader may keep extra bytes already resident on disk or in RAM.
    // Enlarging its local buffer never creates additional network demand.
    quint64        readBuffered(quint64 offset, quint64 length, Completion done);
    bool           cancelRead(quint64 ticket);
    void           pin(bool active);
    quint64        downloadAll(GoalCompletion done, Progress progress = {});
    bool           cancelGoal(quint64 goal);
    void           shutdown();
    MediaFileState state() const;

signals:
    void failed(MediaChunkError error);
    void fullDownloadChanged(bool active);

private:
    struct Goal {
        GoalCompletion done;
        Progress       progress;
    };
    struct Pending {
        quint64 ticket = 0, length = 0;
    };
    void                                                         schedule();
    void                                                         pump();
    void finish(MediaChunkError error = {}, QString localPath = {});
    quint64                                                      quantum(bool background) const;
    std::function<MediaChunkAbort(quint64, quint64, Completion)> provider();
    QPointer<MediaChunkCache>                                    cache_;
    QString                                                      source_, lane_;
    quint64                                                      size_, next_ = 0, playhead_ = 0;
    Fetch                                                        fetch_;
    std::function<QString()>                                     localFile_;
    QHash<quint64, Goal>                                         goals_;
    QMap<quint64, Pending>                                       pending_;
    QSet<quint64>                                                reads_;
    bool   scheduled_ = false, stopped_ = false, checkpointing_ = false, goalPinned_ = false;
    double bytesPerMs_ = 0;
};
