/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "chatmedia.h"
#include "mediachunktypes.h"
#include "widgets/MediaViewer.h"
#include <QPointer>
#include <optional>

class FileSharingManager;
class FileSharingItem;
class PsiAccount;
class MamChatWidget;

// Presentation policy for a unique-resource gallery. SQL owns filtering,
// ordering and counts; the file layer owns transfers and their cancellation.
class ChatMediaViewerSource final : public MediaNavigationSource {
public:
    ChatMediaViewerSource(FileSharingManager *, PsiAccount *, const HistoryArchive::Key &, QString source,
                          QImage preview = {}, QString path = {}, qint64 videoPosition = 0,
                          QList<QPointer<FileSharingItem>> loadedFiles = {});
    ~ChatMediaViewerSource() override;
    void        start() override;
    void        adjacent(int direction) override;
    bool        enqueueAdjacent(int direction) override;
    void        setFilter(MediaFilter) override;
    void        retry() override;
    bool        saveAs(QWidget *) override;
    static void install(MamChatWidget *, PsiAccount *, const HistoryArchive::Key &);
    void setBorrowedPlayer(QString sourceId, QWidget *player, std::function<bool()> restore);

private:
    struct LiveItem {
        QPointer<FileSharingItem> file;
        QString sourceId, name, mime;
        int type = HistoryMediaRefs::Other;
        int availability = MediaFilter::Remote;
    };
    void                            refresh();
    void                            refreshLive();
    void                            selectLive(int index);
    void                            prepareLiveFallback(quint64 epoch);
    void                            useLiveFallback();
    void                            requestNavigationPage(int steps, const ChatMedia::Entry &anchor, quint64 epoch);
    void                            completeNavigation(std::optional<ChatMedia::Entry> candidate, quint64 epoch, bool ok);
    void                            choose(ChatMedia::Page, bool ok, int direction, bool replace);
    void                            select(const ChatMedia::Entry &, FileSharingItem *directItem = nullptr);
    void                            cancelContent();
    void                            fail(MediaChunkError);
    void                            publish();
    QPointer<FileSharingManager>    manager_;
    QPointer<PsiAccount>            account_;
    QPointer<ChatMedia>             browser_;
    QPointer<FileSharingItem>       item_;
    std::optional<ChatMedia::Entry> entry_;
    QList<QPointer<FileSharingItem>> loadedFiles_;
    QList<LiveItem>                  liveFiltered_;
    bool                             liveMode_ = false, initialFilterAdjusted_ = false;
    View                            view_;
    QString                         source_;
    QString                         borrowedSourceId_;
    MediaChunkAbort                 opening_, decoding_;
    QMetaObject::Connection         integrity_;
    quint64                         queryEpoch_ = 0, contentEpoch_ = 0;
    qint64                          initialPosition_ = 0;
    int                             direction_ = 1, skips_ = 0, queuedSteps_ = 0;
    bool                            started_ = false, refreshPending_ = false, navigationActive_ = false;
};
