/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QByteArray>
#include <QtGlobal>

namespace MediaPlaybackPolicy {
// GStreamer reads seekable devices on the GUI thread and treats a temporary
// zero read as EOF. Keep its loopback adapter over FileReader. FFmpeg can wait
// for the reader on its decoder thread; it is Qt's desktop default since 6.5.
inline bool deviceStreaming(int qtVersion, QByteArray backend)
{
    if (qtVersion < QT_VERSION_CHECK(6, 4, 0))
        return false;
    backend = backend.trimmed().toLower();
    return backend == "ffmpeg" || (backend.isEmpty() && qtVersion >= QT_VERSION_CHECK(6, 5, 0));
}
inline bool deviceStreaming() { return deviceStreaming(QT_VERSION, qgetenv("QT_MEDIA_BACKEND")); }
}
