#pragma once

#include "mediachunktypes.h"

class FileSharingItem;
class MediaChunkCache;
namespace XMPP::Jingle::FileTransfer {
class File;
}

namespace FileSharingChunkSource {
QString          identity(FileSharingItem *item);
MediaChunkCache *configure(FileSharingItem *item);
void             addMetadata(FileSharingItem *item, const XMPP::Jingle::FileTransfer::File &file);
QString          localFile(FileSharingItem *item);
QString          error(FileSharingItem *item);
MediaChunkAbort fetch(FileSharingItem *item, quint64 offset, quint64 length, MediaChunkCompletion done, QObject *owner);
}
