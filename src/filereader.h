/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "mediachunktypes.h"
#include <QIODevice>
#include <memory>
#include <optional>

class FileDownload;

// One consumer's seekable view. Cursor operations have one caller at a time;
// its owner thread never waits for network data. A foreign consumer may wait,
// and abort() is safe from any thread. Stop that consumer before destruction.
class FileReader : public QIODevice {
    Q_OBJECT
public:
    FileReader(const QString &path, quint64 offset = 0, std::optional<quint64> length = {}, QObject *parent = nullptr);
    FileReader(FileDownload *download, quint64 offset = 0, std::optional<quint64> length = {},
               QObject *parent = nullptr);
    ~FileReader() override;

    bool                  open(OpenMode mode = ReadOnly) override;
    void                  close() override;
    void                  abort();
    // Pause network demand without aborting the seekable device or its decoder.
    void                  setSuspended(bool suspended);
    bool                  isSequential() const override { return false; }
    qint64                size() const override;
    bool                  seek(qint64 position) override;
    qint64                bytesAvailable() const override;
    bool                  atEnd() const override;
    MediaChunkError::Code errorCode() const;

signals:
    void failed();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    void schedule();
    void cancelPending();
    void release();
    void finishRead(quint64 epoch);
    void fail(const MediaChunkError &error);
    class Private;
    std::unique_ptr<Private> d;
};
