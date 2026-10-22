#pragma once

#include "mediachunktypes.h"
#include <QIODevice>
#include <QObject>
#include <functional>
#include <memory>

// One payload file and one atomic JSON sidecar per immutable, account-scoped
// resource. Disk access, range merging and checksum verification run on a worker.
class MediaChunkStore : public QObject {
    Q_OBJECT
public:
    using Completion = MediaChunkCompletion;
    using Hasher     = std::function<QByteArray(const QString &, QIODevice *)>;
    MediaChunkStore(QString directory, quint64 diskLimit, QObject *parent = nullptr, Hasher hasher = { });
    ~MediaChunkStore() override;
    void           configure(QString key, quint64 size, QList<MediaFileChecksum> checksums, QVariantMap metadata,
                             Completion done = { });
    void           read(QString key, quint64 size, quint64 offset, quint64 length, Completion done);
    void           write(QString key, quint64 size, quint64 offset, QByteArray bytes, Completion done);
    void           resume(QString key, Completion done);
    void           invalidate(QString key, quint64 size, quint64 offset, quint64 length, QString message);
    void           touch(const QString &key);
    void           pin(const QString &key, bool active);
    quint64        diskBytes() const;
    MediaFileState state(const QString &key) const;
    bool           contains(const QString &key, quint64 offset, quint64 length) const;

signals:
    void availabilityChanged();
    void fileChanged(const QString &key);

private:
    class Private;
    std::unique_ptr<Private> d;
};
