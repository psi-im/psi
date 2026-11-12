/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filereader.h"
#include "filedownload.h"
#include <QFile>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

class FileReader::Private {
public:
    mutable QMutex         mutex;
    QWaitCondition         changed;
    QFile                  file;
    QString                path;
    QPointer<FileDownload> download;
    quint64                fileSize = 0, base = 0, length = 0, wanted = 0, wantedLength = 0, epoch = 0;
    std::optional<quint64> requestedLength;
    QByteArray             buffer;
    quint64                bufferStart = 0;
    bool                   local = false, opened = false, aborted = false, failed = false;
    bool                   blockingDemand = false, suspended = false;
    MediaChunkError::Code  error = MediaChunkError::None;
    // The following scheduling fields are used only on the owner thread.
    bool                   scheduled = false, pinned = false;
    quint64                ticket = 0, scheduledEpoch = 0;
    std::optional<quint64> finishedEpoch;

    qint64 available(quint64 position) const
    {
        if (position < bufferStart || position - bufferStart >= quint64(buffer.size()))
            return 0;
        return qint64(quint64(buffer.size()) - (position - bufferStart));
    }
};

FileReader::FileReader(const QString &path, quint64 offset, std::optional<quint64> length, QObject *parent) :
    QIODevice(parent), d(new Private)
{
    d->local           = true;
    d->path            = path;
    d->base            = offset;
    d->requestedLength = length;
}

FileReader::FileReader(FileDownload *download, quint64 offset, std::optional<quint64> length, QObject *parent) :
    QIODevice(parent), d(new Private)
{
    d->download = download;
    d->fileSize = download ? download->size() : 0;
    d->base     = offset;
    d->length   = length.value_or(offset <= d->fileSize ? d->fileSize - offset : 0);
    if (download) {
        connect(download, &FileDownload::failed, this, &FileReader::fail);
        connect(download, &QObject::destroyed, this, [this] { fail(QStringLiteral("File download unavailable")); });
    }
}

FileReader::~FileReader() { abort(); }

bool FileReader::open(OpenMode mode)
{
    if (QThread::currentThread() != thread() || (mode != ReadOnly && mode != (ReadOnly | Unbuffered)))
        return false;
    {
        QMutexLocker lock(&d->mutex);
        if (d->aborted || d->failed)
            return false;
        if (d->opened)
            return true;
    }
    if (d->local) {
        d->file.setFileName(d->path);
        if (!d->file.open(ReadOnly)) {
            setErrorString(d->file.errorString());
            return false;
        }
        d->fileSize = quint64(d->file.size());
        if (d->base <= d->fileSize)
            d->length = d->requestedLength.value_or(d->fileSize - d->base);
    }
    if (d->fileSize > quint64(std::numeric_limits<qint64>::max()) || d->base > d->fileSize
        || d->length > d->fileSize - d->base || (!d->local && !d->download)) {
        setErrorString(QStringLiteral("Invalid file read range or source"));
        d->file.close();
        return false;
    }
    if (!QIODevice::open(ReadOnly | Unbuffered))
        return false;
    {
        QMutexLocker lock(&d->mutex);
        d->opened = true;
    }
    if (!d->local) {
        d->download->pin(true);
        d->pinned = true;
    }
    if (!d->length) {
        finishRead(d->epoch);
    }
    return true;
}

void FileReader::close()
{
    abort();
    d->file.close();
    QIODevice::close();
}

void FileReader::abort()
{
    {
        QMutexLocker lock(&d->mutex);
        if (!d->aborted) {
            d->aborted = true;
            ++d->epoch;
            d->buffer.clear();
            if (!d->failed)
                d->error = MediaChunkError::Cancelled;
            d->changed.wakeAll();
        }
    }
    if (QThread::currentThread() == thread())
        release();
    else
        QMetaObject::invokeMethod(this, [this] { release(); }, Qt::QueuedConnection);
}

void FileReader::setSuspended(bool suspended)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, suspended] { setSuspended(suspended); }, Qt::QueuedConnection);
        return;
    }
    {
        QMutexLocker lock(&d->mutex);
        if (d->suspended == suspended || d->aborted)
            return;
        d->suspended = suspended;
        // Invalidate callbacks for the cancelled request, not the decoder.
        ++d->epoch;
        d->changed.wakeAll();
    }
    if (suspended) {
        cancelPending();
        if (d->download)
            d->download->resetRangeTiming();
    } else
        schedule();
}

void FileReader::cancelPending()
{
    const auto ticket = std::exchange(d->ticket, 0);
    if (!d->download)
        return;
    if (ticket)
        d->download->cancelRead(ticket);
}

void FileReader::release()
{
    cancelPending();
    if (std::exchange(d->pinned, false) && d->download)
        d->download->pin(false);
}

qint64 FileReader::size() const
{
    QMutexLocker lock(&d->mutex);
    return qint64(d->length);
}

bool FileReader::seek(qint64 position)
{
    {
        QMutexLocker lock(&d->mutex);
        if (!d->opened || d->aborted || d->failed || position < 0 || quint64(position) > d->length)
            return false;
    }
    if (!QIODevice::seek(position))
        return false;
    {
        QMutexLocker lock(&d->mutex);
        ++d->epoch;
        d->wanted       = quint64(position);
        d->wantedLength = 0;
    }
    if (QThread::currentThread() == thread()) {
        cancelPending();
        if (d->download)
            d->download->setPlayhead(d->base + quint64(position));
    } else {
        QMetaObject::invokeMethod(
            this,
            [this] {
                cancelPending();
                if (d->download) {
                    QMutexLocker lock(&d->mutex);
                    d->download->setPlayhead(d->base + d->wanted);
                }
                schedule();
            },
            Qt::QueuedConnection);
    }
    return true;
}

qint64 FileReader::bytesAvailable() const
{
    const auto   position = QIODevice::pos();
    QMutexLocker lock(&d->mutex);
    if (!d->opened || d->aborted || d->failed || position < 0 || quint64(position) >= d->length)
        return 0;
    return d->local ? qint64(d->length - quint64(position))
                    : std::min(qint64(d->length - quint64(position)), d->available(quint64(position)));
}

bool FileReader::atEnd() const
{
    const auto   position = QIODevice::pos();
    QMutexLocker lock(&d->mutex);
    return !d->opened || d->aborted || d->failed || (position >= 0 && quint64(position) >= d->length);
}

MediaChunkError::Code FileReader::errorCode() const
{
    QMutexLocker lock(&d->mutex);
    return d->error;
}

void FileReader::fail(const MediaChunkError &error)
{
    {
        QMutexLocker lock(&d->mutex);
        if (d->aborted || d->failed)
            return;
        d->failed = true;
        d->error  = error.code();
        ++d->epoch;
        d->buffer.clear();
        setErrorString(error.message());
        d->changed.wakeAll();
    }
    release();
    emit failed();
}

void FileReader::finishRead(quint64 epoch)
{
    QMetaObject::invokeMethod(
        this,
        [this, epoch] {
            {
                QMutexLocker lock(&d->mutex);
                if (d->aborted || d->failed || d->epoch != epoch || d->finishedEpoch == epoch)
                    return;
                d->finishedEpoch = epoch;
            }
            emit readChannelFinished();
        },
        Qt::QueuedConnection);
}

void FileReader::schedule()
{
    quint64 scheduledEpoch;
    {
        QMutexLocker lock(&d->mutex);
        scheduledEpoch = d->epoch;
    }
    if ((d->scheduled && d->scheduledEpoch == scheduledEpoch) || d->ticket)
        return;
    d->scheduled      = true;
    d->scheduledEpoch = scheduledEpoch;
    QTimer::singleShot(0, this, [this, scheduledEpoch] {
        quint64 position, epoch, requested;
        bool    buffered = false;
        {
            QMutexLocker lock(&d->mutex);
            if (scheduledEpoch != d->epoch)
                return;
            d->scheduled = false;
            if (d->aborted || d->failed || d->suspended || !d->opened || d->local || !d->wantedLength)
                return;
            position  = d->wanted;
            requested = d->wantedLength;
            buffered  = d->blockingDemand;
            epoch     = d->epoch;
            if (position >= d->length) {
                finishRead(epoch);
                return;
            }
            if (d->available(position))
                return;
        }
        if (!d->download) {
            fail(QStringLiteral("File cache unavailable"));
            return;
        }
        const QPointer<FileReader> guard(this);
        const auto                 read = buffered ? &FileDownload::readBuffered : &FileDownload::read;
        d->ticket
            = (d->download.data()->*read)(d->base + position, std::min<quint64>(d->length - position, requested),
                                          [guard, position, epoch](QByteArray bytes, MediaChunkError error) {
                                              if (!guard)
                                                  return;
                                              {
                                                  QMutexLocker lock(&guard->d->mutex);
                                                  if (guard->d->aborted || guard->d->failed || guard->d->epoch != epoch)
                                                      return;
                                              }
                                              guard->d->ticket = 0;
                                              if (error.isEmpty() && bytes.isEmpty())
                                                  error = QStringLiteral("Incomplete file read");
                                              if (!error.isEmpty()) {
                                                  guard->fail(error);
                                                  return;
                                              }
                                              {
                                                  QMutexLocker lock(&guard->d->mutex);
                                                  guard->d->bufferStart = position;
                                                  guard->d->buffer      = std::move(bytes);
                                                  guard->d->changed.wakeAll();
                                              }
                                              emit guard->readyRead();
                                          });
        if (!d->ticket) {
            fail(QStringLiteral("Too many pending file reads"));
            return;
        }
    });
}

qint64 FileReader::readData(char *data, qint64 maxSize)
{
    if (maxSize <= 0)
        return 0;
    const auto   position = QIODevice::pos();
    const bool   owner    = QThread::currentThread() == thread();
    QMutexLocker lock(&d->mutex);
    if (d->aborted || d->failed || position < 0)
        return -1;
    if (quint64(position) >= d->length) {
        finishRead(d->epoch);
        return 0;
    }
    if (d->local) {
        const auto count = qint64(
            std::min<quint64>(std::min<quint64>(quint64(maxSize), 4 * 1024 * 1024), d->length - quint64(position)));
        if (!d->file.seek(qint64(d->base) + position) || d->file.read(data, count) != count) {
            QMetaObject::invokeMethod(
                this, [this] { fail(QStringLiteral("Cannot read complete file payload")); }, Qt::QueuedConnection);
            return -1;
        }
        if (quint64(position + count) == d->length)
            finishRead(d->epoch);
        return count;
    }
    const auto requested = std::min<quint64>(quint64(maxSize), d->length - quint64(position));
    quint64    copied    = 0;
    // A blocking consumer expects a short read only at EOF. In particular,
    // codec format probes may leave a tiny cached prefix: returning just that
    // prefix to libpng's next larger read looks like a truncated image.
    while (copied < requested) {
        const auto at = quint64(position) + copied;
        while (!d->available(at)) {
            d->wanted         = at;
            d->blockingDemand = !owner;
            d->wantedLength   = std::min<quint64>(requested - copied, 64 * 1024);
            QMetaObject::invokeMethod(this, [this] { schedule(); }, Qt::QueuedConnection);
            if (owner)
                return qint64(copied);
            d->changed.wait(&d->mutex);
            if (d->aborted || d->failed)
                return -1;
        }
        const auto count = std::min<quint64>(requested - copied, quint64(d->available(at)));
        std::memcpy(data + copied, d->buffer.constData() + (at - d->bufferStart), size_t(count));
        copied += count;
        d->wanted = quint64(position) + copied;
        if (owner)
            break;
    }
    if (d->wanted == d->length)
        finishRead(d->epoch);
    return qint64(copied);
}
