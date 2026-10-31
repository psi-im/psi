/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "../../plugins/include/nativechatrenderer.h"
#include "../../plugins/include/nativechatthemeprovider.h"
#include "LongListWidget.h"
#include "MamHistorySource.h"
#include "messageview.h"
#include <QPixmap>
#include <QPointer>
#include <functional>
#include <memory>
#include <optional>

#include <QHash>
#include <QList>
#include <QSet>

class FileSharingDeviceOpener;
class ChatMessageText;
class QTemporaryDir;
class QFrame;
class MessagePreview;
class QLabel;
class QResizeEvent;
class QWheelEvent;
class QShowEvent;
class QHideEvent;
class QTimer;
class QPropertyAnimation;
class QPushButton;
class QMenu;
class QGraphicsOpacityEffect;

class MamChatWidget final : public Mattermost::LongListWidget {
    Q_OBJECT
    // The log around the messages comes from the theme. A style sheet can
    // override it: MamChatWidget { qproperty-hoverHighlight: true; }
    Q_PROPERTY(bool hoverHighlight READ hoverHighlight WRITE setStyleHoverHighlight)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setStyleBackgroundColor)
    // A file or resource path, or "none" to drop the theme's image.
    Q_PROPERTY(QString backgroundImage READ backgroundImage WRITE setStyleBackgroundImage)
    // "tile" or "scale".
    Q_PROPERTY(QString backgroundImageMode READ backgroundImageMode WRITE setStyleBackgroundImageMode)
public:
    explicit MamChatWidget(QWidget *parent = nullptr);

    // This is the presentation/model seam. No network requests or SQL in the widget.
    void appendMessage(const MessageView &view);
    int  prependArchivePage(const QList<QPair<QString, MessageView>> &page);
    void clearMessages();
    void setHistoryLoading(bool loading);
    void setHistoryExhausted(bool exhausted);
    // Continue an explicit pixel demand after a completed source page.
    void maybeContinueInitialHistoryFill();
    void stopHistoryFill();
    bool historyExhausted() const { return historyExhausted_; }
    int  messageCount() const { return items_.size(); }
    const MamHistorySource &historySource() const { return historySource_; }
    void                    restoreArchiveSnapshot(int count, const QString &lastUid);
    void observeArchivePage(const QStringList &uids, int count, int firstIndex, const QString &before = {},
                            const QString &after = {}, bool tailChain = true, bool stable = true);
    void cancelArchiveRanges();
    void completeArchiveRange(const MamHistorySource::Request &, bool success, const QString &error = {},
                              bool queryComplete = false);
    // Dynamic MAM stanza budget inferred from measured message heights.
    // Includes receipt/marker allowance but never exceeds HistorySync's 40.
    int  suggestedOlderHistoryPageSize() const;
    // An explicit seek is a navigation intent, not initial open-at-end.
    void scrollToIndex(int                                   index,
                       Mattermost::LongListWidget::Alignment alignment
                       = Mattermost::LongListWidget::Alignment::EnsureVisible);

    void           setGroupChat(bool enabled) { groupChat_ = enabled; }
    // Without a renderer, the theme is laid out from its description alone.
    // A renderer must outlive the rows it builds (see nativechatrenderer.h).
    void           setTheme(const NativeChatTheme &theme, const NativeChatMessageRenderer *renderer = nullptr);
    void           setAvatarResolver(std::function<QPixmap(const MessageView &)> resolver);
    void           setNicknameColorResolver(std::function<QColor(const MessageView &)> resolver);
    void           refreshNicknameColors();
    void           refreshAvatars();
    void           setMediaOpener(FileSharingDeviceOpener *opener);
    void           setInformationalColor(const QColor &color);
    void           setQuoteColor(const QColor &color);
    static QString displayTime(const QDateTime &when, const QDateTime &now = QDateTime::currentDateTime());
    static QString timestampText(const QDateTime &when, const NativeChatPartOptions &options,
                                 const QDateTime &now = QDateTime::currentDateTime());
    static QString fullTimestamp(const QDateTime &when);
    // Most recent actually visible incoming message requesting a read marker.
    QString takeVisibleMarkableIncomingId();
    void    restoreDisplayedIncoming(const QString &id, const QDateTime &time = {}, qint64 historyId = 0);

    // Whole-message selection uses logical model keys, not materialized QWidget pointers.
    int  selectedMessageCount() const { return selectedKeys_.size(); }
    void selectMessageRange(int first, int last);
    void setMessageSelected(int index, bool selected);
    void copySelectedMessages();
    void cancelMessageSelection();
    void toggleReaction(const QString &messageId, const QString &emoji);

signals:
    void contactMenuRequested(const QString &nick, const QPoint &globalPosition);
    void replyRequested(const ChatReply &reply);
    void forwardMessageRequested(const QString &messageId, const QString &sender, const QString &text);
    void olderMessagesRequested();
    void archiveRangeRequested(const MamHistorySource::Request &request);
    void archiveRangeCancelled();
    void archiveSeekUnavailable(const QString &reason);
    void paletteChanged();
    void nicknameClicked(const QString &nick);
    void avatarClicked(const QString &nick, bool local);
    void outgoingReactions(const QString &messageId, const QSet<QString> &reactions);
    void outgoingMessageRetraction(const QString &messageId);
    void emojiChooserRequested(const QString &messageId, const QPoint &globalPosition);

public:
    bool    hoverHighlight() const { return isHoverHighlightEnabled(); }
    QColor  backgroundColor() const;
    QString backgroundImage() const { return backgroundImagePath_; }
    QString backgroundImageMode() const;
    void    setStyleHoverHighlight(bool enabled);
    // Classic message icons (direction, delivery, encryption) before the
    // time, for themes that put the header in the text.
    void    setMessageIconsEnabled(bool enabled);
    // The legacy text log's colours for the header of sent and received
    // messages in a conversation (not a group chat).
    void    setDirectionColors(const QColor &sent, const QColor &received);
    void    setStyleBackgroundColor(const QColor &color);
    void    setStyleBackgroundImage(const QString &path);
    void    setStyleBackgroundImageMode(const QString &mode);

protected:
    QWidget *createItemWidget(int index) override;
    QString  itemIdentity(const QWidget *widget) const override;
    int      indexOfItemIdentity(const QString &identity) const override;
    bool     isModelItemAvailable(int index) const override;
    int      estimatedItemHeight(int index) const override;
    bool     eventFilter(QObject *target, QEvent *event) override;
    void     wheelEvent(QWheelEvent *event) override;
    void     resizeEvent(QResizeEvent *event) override;
    void     showEvent(QShowEvent *event) override;
    void     hideEvent(QHideEvent *event) override;
    void     changeEvent(QEvent *event) override;
    void     paintEvent(QPaintEvent *event) override;
    QColor   hoverHighlightColor() const override;

private:
    // Theme values, unless a style sheet overrides them.
    void                      applyLogLook();
    QPalette                  logPalette() const;
    std::optional<bool>       styleHoverHighlight_;
    QColor                    styleBackgroundColor_;
    QString                   styleBackgroundImage_;
    QString                   styleBackgroundImageMode_;
    QString                   backgroundImagePath_;
    QPixmap                   backgroundImage_;
    NativeChatTheme::ImageMode backgroundImageMode_ = NativeChatTheme::Tile;
    QPixmap                   scaledBackground_;
    bool                      messageIcons_ = true;
    QColor                    sentColor_;
    QColor                    receivedColor_;
    std::pair<QColor, QColor> inTextHeaderColors(const MessageView &message,
                                                 const NativeChatPartOptions &options) const;

    void requestOlderMessages(const char *gesture);
    void scheduleInitialHistoryFillCheck();
    qint64 estimatedMessagePixels() const;
    qreal estimatedAverageMessageHeight() const;
    void schedulePaletteRefresh();
    void refreshPalette();
    bool paletteRefreshPending_ = false;
    using Row                     = MamHistorySource::Row;
    QString             reactionTarget(const MessageView &) const;
    QString             reactionKey(const Row &) const;
    int                 legacyReactionTarget(const QString &messageId, const QDateTime &before) const;
    bool                canSendLegacyReaction(int index) const;
    QString             retractionTarget(const MessageView &) const;
    bool                applyProtocolEvent(const MessageView &);
    bool                isRetracted(const Row &) const;
    void                    showMessageActions(QFrame *row);
    void                    hideMessageActions();
    void                    positionMessageActions();
    void                    setQuickReactionsExpanded(bool expanded);
    QString                 actionKey_;
    QFrame                 *messageActions_  = nullptr;
    QWidget                *quickReactions_  = nullptr;
    QPushButton            *chooseReaction_  = nullptr;
    QPushButton            *moreActions_     = nullptr;
    QMenu                  *messageMenu_     = nullptr;
    QTimer                 *actionHideTimer_ = nullptr;
    QTimer                 *quickHideTimer_  = nullptr;
    QPropertyAnimation     *quickAnimation_  = nullptr;
    QGraphicsOpacityEffect *actionsOpacity_  = nullptr;
    QPropertyAnimation     *actionsFade_     = nullptr;
    struct ReactionState {
        MessageView message { MessageView::Reactions };
    };
    QHash<QString, QHash<QString, ReactionState>> reactionEvents_;
    QHash<QString, QList<MessageView>>            retractions_;
    QSet<QString>                                 receipts_;
    QHash<QString, QPair<bool, QString>>          security_; // message id -> (encrypted, protocol)
    // Stable row identity, not an index: prepending MAM pages shifts indices.
    QString                                       lastDisplayedRowKey_;
    QString                                       lastMarkedIncomingRowKey_;
    QDateTime                                     lastMarkedIncomingTime_;
    qint64                                        lastMarkedIncomingHistoryId_ = 0;
    QSet<QString>                                 unresolvedSentDisplayedIds_;
    QList<QString>                                unresolvedDisplayedIds_;
    int                                           displayedTargetIndex() const;
    void                                          resolveDisplayedMarkers();
    void                                          rememberDisplayedIncoming(int index);
    void                                          updateTimestamps();
    NativeChatMessageInfo                         messageInfo(int index) const;
    // Fonts and colours of Psi's own parts, from the row's NativeChatPartOptions.
    void                                          applyPartStyle(QWidget *row, int index);
    void                                          restyleRows();
    void                                          updateRowSelection(QFrame *row, bool selected);
    void                                          updateRowPalette(QFrame *row, bool selected);
    QString plainMessage(const Row &) const;
    QString copiedMessage(const Row &) const;
    QMenu *createMessageContextMenu(const QString &key, QWidget *target, const QPoint &globalPosition);
    int  replyTargetIndex(const ChatReply &) const;
    bool resolveReply(const ChatReply &, QString &author, QString &html) const;
    void navigateToReply(const ChatReply &);
    void showReply(MessagePreview *preview, const ChatReply &reply) const;
    // A message's text as its row shows it, as rich text.
    QString richMessage(const Row &row) const;
    void refreshReplyPreviews(bool force = false);
    void                                          setHoveredRow(QFrame *row);
    QColor                                        informationalColor_;
    QColor                                        quoteColor_;
    void                                          updateAvatar(QLabel *label, const MessageView &message);
    NativeChatTheme                               theme_;
    const NativeChatMessageRenderer              *renderer_ = nullptr;
    std::unique_ptr<NativeChatMessageRenderer>    ownedRenderer_;
    // Rows remember the generation that built them; only those of the current
    // renderer are passed back to it.
    quint64                                       rendererGeneration_ = 0;
    bool                                          groupChat_   = false;
    FileSharingDeviceOpener                      *mediaOpener_ = nullptr;
    // Keep opened temporary attachments on disk for the lifetime of this chat.
    QList<std::shared_ptr<QTemporaryDir>>         openedTemporaryFiles_;
    std::function<QPixmap(const MessageView &)>   avatarResolver_;
    std::function<QColor(const MessageView &)>    nicknameColorResolver_;
    int                                           indexOfKey(const QString &key) const;
    void                                          refreshSelection();
    void                                          positionSelectionToolbar();
    QFrame                                       *eventRow(QObject *target) const;

    MamHistorySource historySource_;
    QList<Row>      &items_ = historySource_.rows_;
    QWidget         *rowWidget(int knownIndex) const;
    bool             updateDragSelection(const QPoint &viewportPos);
    void             finishDragSelection();
    QVector<int>     materializedRows() const;
    Range            knownVisibleRange() const;
    void             replaceRow(int knownIndex);
    void insertHistoryRows(int at, int count, InsertViewportPolicy policy = InsertViewportPolicy::PreserveIntent);
    void removeHistoryRows(int at, int count);
    void syncSourceLayout(bool enableVirtual = false);
    void requestArchiveRange(int first, int last, RequestReason, quint64 generation);
    bool rangeDemandAllowed_ = false;
    bool archiveRangeActive_ = false, archiveRangePaused_ = false;
    MamHistorySource::Request               activeArchiveRange_;
    quint64                                 archiveIntentSerial_ = 0, activeArchiveIntent_ = 0;
    QSet<QString>                           archiveNavigationKeys_;
    qint64                                  archiveNavigationPixels_ = 0, archiveNavigationGoal_ = 0;
    QVector<QPair<int, int>>                archiveRangeWaiters_;
    mutable QHash<QString, QPair<int, int>> measuredMessageHeights_; // width, learned height
    QSet<QString>           keys_;
    QHash<QString, QString> sourceAliases_; // EDB/MAM source key -> stable row key
    bool                    sameSender(const MessageView &a, const MessageView &b) const;
    int                     findRelatedMessage(const MessageView &incoming, bool allowHistory) const;
    void                    mergeMessage(int index, const MessageView &incoming, const QString &sourceKey = {});
    void                    coalesceRevisionRows();
    bool                    historyLoading_   = false;
    bool                    historyExhausted_ = false;
    bool                    historyRequestPending_ = false;
    quint64                 historyRequestSerial_ = 0;
    bool                    initialFillEligible_ = false;
    bool                    initialFillFinished_ = false;
    bool                    initialFillCheckPending_ = false;
    qint64                                  historyFillTargetPixels_ = -1;
    bool                                    historyFillPaused_       = false;
    bool                                    historyFillReady_        = false;
    // Opening a chat always starts at the live tail, even while Qt measures
    // new rows or MAM/EDB prepends history. A real user scroll takes ownership.
    bool    followingInitialTail_ = true;
    quint64 generatedKey_         = 0;

    QSet<QString> selectedKeys_;
    QString       selectionAnchorKey_;
    QString       pressedKey_;
    bool          mousePressed_        = false;
    bool          selectingByDrag_     = false;
    bool          swallowMouseRelease_ = false;
    QPointer<ChatMessageText> pressedImageBody_;
    QPoint                  pressedImagePoint_;
    QFrame       *selectionToolbar_    = nullptr;
    QLabel       *selectionCountLabel_ = nullptr;
    QTimer       *timestampTimer_      = nullptr;
};
