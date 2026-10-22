#pragma once

#include "mediachunktypes.h"
#include <QIODevice>
#include <QTimer>
#include <optional>

// A bounded transfer commits only after metadata, exact payload length and
// provider success agree. The provider retains protocol-specific validation.
class MediaChunkTransfer : public QObject {
public:
    struct Range {
        quint64 offset, length;
    };
    MediaChunkTransfer(QIODevice *device, quint64 size, quint64 offset, quint64 length, MediaChunkCompletion done,
                       MediaChunkAbort stop, QObject *parent = nullptr);
    ~MediaChunkTransfer() override;
    void metadata(std::optional<quint64> size, std::optional<Range> range);
    void succeeded();
    void failed(QString message);
    void integrityFailed(QString message);
    void disconnected();
    void cancel();

private:
    void                 pump();
    void                 finish(MediaChunkError error);
    QIODevice           *device_;
    quint64              size_, offset_, length_;
    QByteArray           bytes_;
    MediaChunkCompletion done_;
    MediaChunkAbort      stop_;
    QTimer               timer_;
    bool metadata_ = false, completed_ = false, reading_ = false, finished_ = false, disconnected_ = false;
};
