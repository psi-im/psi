#pragma once

#include "mediachunktypes.h"

class FileSharingItem;

namespace FileSharingChunkSource {
MediaChunkAbort fetch(FileSharingItem *item, quint64 offset, quint64 length, MediaChunkCompletion done, QObject *owner);
}
