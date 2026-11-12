#pragma once

#include "filesourceselector.h"
#include "mediachunktypes.h"

#include <QObject>
#include <QPointer>

class PsiAccount;
class FileSharingItem;
class MediaChunkCache;
class FileShareDownloader;
namespace XMPP::Jingle::FileTransfer {
class File;
}

// One explicitly owned source-selection/fallback state per runtime item.
// Storage queries remain stateless and do not instantiate that state.
class FileSharingChunkSource : public QObject {
public:
    ~FileSharingChunkSource() override;
    static QString          identity(const FileSharingItem *item);
    static QString          sourceFor(PsiAccount *, const XMPP::Jingle::FileTransfer::File &);
    static MediaChunkCache *configure(const FileSharingItem *item);
    static void             addChecksums(const FileSharingItem *item, const XMPP::Jingle::FileTransfer::File &file);
    static QString          localFile(const FileSharingItem *item);
    using PathDone = std::function<void(QString, MediaChunkError)>;
    static MediaChunkAbort  openWhole(FileSharingItem *, QObject *receiver, quint64 maximumBytes, PathDone);
    static void             cancelFullDownloadFallback(FileSharingItem *item);
    static MediaChunkAbort  fetch(FileSharingItem *item, quint64 offset, quint64 length, MediaChunkCompletion done,
                                  QObject *owner, std::function<bool()> allowWhole = {});

private:
    explicit FileSharingChunkSource(FileSharingItem *item);
    static FileSharingChunkSource *state(FileSharingItem *item);
    static MediaChunkAbort sequential(FileSharingItem *item, const QString &source, quint64 offset, quint64 length,
                                      MediaChunkCompletion done);
    class SequentialDownload;
    class WholeDownload;
    FileSourceSelector           selector_;
    QPointer<SequentialDownload> sequential_;
    QPointer<WholeDownload>      whole_;
};
