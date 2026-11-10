#pragma once

#include "mediachunktypes.h"
#include <QIODevice>
#include <QObject>
#include <functional>
#include <limits>
#include <memory>

class FileCache;
class FileCacheMemory;

// Range-I/O backend owned by FileCache. Payload ownership, ranges and checksums
// live in files.sqlite; payload I/O and checkpoints run on its private thread.
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
    bool           containsCached(const QString &key, quint64 size, quint64 offset, quint64 length) const;
    bool           isCompleteFile(const QString &path) const;
    void           setDiskLimit(quint64 limit);
    // Force a checkpoint, for pause/close and before publishing a payload path.
    void checkpoint(Completion done = {});

signals:
    void availabilityChanged();
    void fileChanged(const QString &key);

private:
    friend class FileCache;
    MediaChunkStore(QString partition, QString directory, QString databasePath, quint64 diskLimit,
                    FileCacheMemory *memory, QObject *parent, Hasher hasher);
    quint64 beginMutation(const QString &key, Completion done, quint64 offset = 0,
                          quint64 length = std::numeric_limits<quint64>::max());
    void    dispatchRead(quint64 id, const QString &key, quint64 size, quint64 offset, quint64 length);
    class Private;
    std::unique_ptr<Private> d;
};
