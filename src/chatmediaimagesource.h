/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "chatmedia.h"
#include "historyarchivevalues.h"
#include "widgets/ImageViewer.h"
#include <QDateTime>
#include <QPointer>
#include <memory>

class FileSharingManager;
class PsiAccount;
class MamChatWidget;

// Adapter between the account-scoped, paginated history API and the generic
// image viewer. No directory scanning or eager FileSharingItem allocation.
class ChatMediaImageSource final : public ImageNavigationSource {
public:
    ChatMediaImageSource(FileSharingManager *manager, PsiAccount *account,
                         const HistoryArchive::Key &key, qint64 eventId,
                         const QDateTime &date, int ordinal, const QString &sourceId);
    void adjacent(int direction, Result result) override;
    static void install(MamChatWidget *chat, PsiAccount *account, const HistoryArchive::Key &key);

private:
    struct Walk;
    void requestPage(const std::shared_ptr<Walk> &walk, ChatMedia::Cursor cursor);
    void consume(const std::shared_ptr<Walk> &walk);
    void load(const std::shared_ptr<Walk> &walk, const ChatMedia::Entry &entry,
              class FileSharingItem *item);

    QPointer<FileSharingManager> manager_;
    QPointer<PsiAccount> account_;
    QPointer<ChatMedia> browser_;
    ChatMedia::Cursor cursor_;
    QString sourceId_;
    bool hasOriginalEvent_ = false;
    bool anchorResolved_ = false;
};
