#pragma once

#include <QByteArray>
#include <QObject>
#include <functional>
#include <memory>

// One worker serializes disk access, integrity checks and eviction. Callbacks
// return to the owning thread. Destruction drains outstanding disk operations.
class MediaChunkStore : public QObject {
public:
    using Completion = std::function<void(QByteArray)>;
    MediaChunkStore(QString directory, quint64 diskLimit, QObject *parent = nullptr);
    ~MediaChunkStore() override;
    void    read(const QString &key, quint64 expected, Completion done);
    void    write(const QString &key, QByteArray bytes, Completion done);
    void    touch(const QString &key);
    quint64 diskBytes() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
