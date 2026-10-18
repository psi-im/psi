/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QBuffer>
class FileShareDownloader : public QBuffer {
    Q_OBJECT
public:
    using QBuffer::QBuffer;
    void setSelfDelete(bool) { }
    bool open(OpenMode mode = ReadOnly) override { return QBuffer::open(mode); }
};
