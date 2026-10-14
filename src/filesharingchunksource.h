#pragma once

#include "mediachunkcache.h"

class FileSharingItem;

namespace FileSharingChunkSource {
// A capability failure permits the HTTP adapter to fall back before sending headers.
inline const QString   rangeUnsupported = QStringLiteral("Source does not support bounded byte ranges");
MediaChunkCache::Abort fetch(FileSharingItem *item, quint64 offset, quint64 length, MediaChunkCache::Completion done,
                             QObject *owner);
}
