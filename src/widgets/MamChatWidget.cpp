/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamChatWidget.h"
#include "MamMessageIdentity.h"
#include "ChatMessageText.h"
#include "psirichtext.h"
#include "iconset.h"

#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QDateTime>
#include <limits>
#include <QShortcut>
#include <QPalette>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTimer>
#include <QVBoxLayout>

MamChatWidget::MamChatWidget(QWidget *parent) : Mattermost::LongListWidget(parent)
{
    setProperty("psiMamOverlay", true);
    theme_.layout = NativeChatTheme::Modern;
    // Coalesced single-shot timer: no updates while a chat is hidden and
    // no per-second activity. Only the next visible relative-time boundary
    // schedules a wake-up.
    timestampTimer_ = new QTimer(this);
    timestampTimer_->setObjectName(QStringLiteral("mamTimestampRefresh"));
    timestampTimer_->setSingleShot(true);
    timestampTimer_->setTimerType(Qt::CoarseTimer);
    connect(timestampTimer_, &QTimer::timeout, this, &MamChatWidget::updateTimestamps);
    setDefaultItemHeight(72);
    setMaterializationLimit(160);
    setRequestBlockSize(40);
    setHoverHighlightEnabled(false);

    // MatterLeast-style floating whole-message selection controls.
    selectionToolbar_ = new QFrame(viewport());
    selectionToolbar_->setObjectName(QStringLiteral("mamSelectionToolbar"));
    selectionToolbar_->setFrameShape(QFrame::StyledPanel);
    selectionToolbar_->setAutoFillBackground(true);
    auto *tools = new QHBoxLayout(selectionToolbar_);
    tools->setContentsMargins(8, 4, 8, 4);
    tools->setSpacing(6);
    selectionCountLabel_ = new QLabel(selectionToolbar_);
    tools->addWidget(selectionCountLabel_);
    auto *copy = new QPushButton(tr("Copy"), selectionToolbar_);
    copy->setObjectName(QStringLiteral("mamCopySelected"));
    auto *cancel = new QPushButton(tr("Cancel"), selectionToolbar_);
    tools->addWidget(copy);
    tools->addWidget(cancel);
    connect(copy, &QPushButton::clicked, this, &MamChatWidget::copySelectedMessages);
    connect(cancel, &QPushButton::clicked, this, &MamChatWidget::cancelMessageSelection);
    selectionToolbar_->hide();
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setObjectName(QStringLiteral("mamCopyShortcut"));
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    copyShortcut->setEnabled(false); // Single-message QLabel Ctrl+C stays intact.
    connect(copyShortcut, &QShortcut::activated, this, &MamChatWidget::copySelectedMessages);
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &MamChatWidget::cancelMessageSelection);

    connect(this, &Mattermost::LongListWidget::userScrollStarted, this,
            [this] { followingInitialTail_ = false; });

    connect(this, &Mattermost::LongListWidget::visibleRangeChanged, this,
            [this](int first, int) {
        if (first <= 1)
            requestOlderIfNeeded(false);
        updateTimestamps();
    });
    connect(this, &Mattermost::LongListWidget::materializedRangeChanged,
            this, [this](int, int) { updateTimestamps(); });
}

namespace {
// QLayout's preferred height uses its preferred width. For a wrapped document
// that width can be much narrower than the timeline, inflating the whole row.
// LongListWidget measures at the viewport width, so report the same height here.
class ChatMessageRow : public QFrame {
public:
    QSize sizeHint() const override
    {
        return QSize(0, qMax(1, heightForWidth(qMax(1, width()))));
    }
};


bool validateLayout(const QJsonValue &node, QSet<QString> &seen, int depth = 0)
{
    if (depth > 8)
        return false;
    if (node.isString()) {
        const QString slot = node.toString();
        if (slot != QLatin1String("author") && slot != QLatin1String("timestamp")
            && slot != QLatin1String("avatar") && slot != QLatin1String("body"))
            return false;
        if (seen.contains(slot))
            return false;
        seen.insert(slot);
        return true;
    }
    if (!node.isObject())
        return false;
    const QJsonObject group = node.toObject();
    const QString direction = group.value(QStringLiteral("direction")).toString();
    const QJsonArray items = group.value(QStringLiteral("items")).toArray();
    if ((direction != QLatin1String("horizontal") && direction != QLatin1String("vertical"))
        || items.isEmpty() || items.size() > 12)
        return false;
    for (const auto &child : items)
        if (!validateLayout(child, seen, depth + 1))
            return false;
    return true;
}

QJsonObject resolvedLayout(const NativeChatTheme &theme)
{
    if (!theme.layoutSpec.isEmpty()) {
        const auto custom = QJsonDocument::fromJson(theme.layoutSpec.toUtf8());
        QSet<QString> seen;
        if (custom.isObject() && validateLayout(custom.object(), seen)
            && seen.contains(QStringLiteral("body")))
            return custom.object();
    }
    static const auto modern = QJsonDocument::fromJson(
        R"({"direction":"horizontal","items":["avatar",{"direction":"vertical","items":[{"direction":"horizontal","items":["author","timestamp"]},"body"]}]})").object();
    static const auto classic = QJsonDocument::fromJson(
        R"({"direction":"horizontal","items":["timestamp","author","body"]})").object();
    return theme.layout == NativeChatTheme::Classic ? classic : modern;
}

QBoxLayout *buildThemeLayout(const QJsonObject &node, const QHash<QString, QWidget *> &elements,
                              const NativeChatTheme &theme)
{
    const bool horizontal = node.value(QStringLiteral("direction")).toString()
        == QLatin1String("horizontal");
    QBoxLayout *layout = horizontal ? static_cast<QBoxLayout *>(new QHBoxLayout)
                                    : static_cast<QBoxLayout *>(new QVBoxLayout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(horizontal ? (theme.layout == NativeChatTheme::Classic ? 4 : 10) : 3);
    for (const auto &item : node.value(QStringLiteral("items")).toArray()) {
        if (item.isObject()) {
            auto *nested = buildThemeLayout(item.toObject(), elements, theme);
            if (!horizontal && item.toObject().value(QStringLiteral("direction"))
                                   == QLatin1String("horizontal")) {
                // A horizontal header inside a vertical message column needs
                // its own height-for-width boundary. Qt6 otherwise distributes
                // available height inside the nested layout, leaving a large
                // blank gap between the header and the message document.
                auto *header = new QWidget;
                header->setObjectName(QStringLiteral("mamHeaderGroup"));
                header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
                header->setLayout(nested);
                layout->addWidget(header, 0, Qt::AlignTop);
            } else {
                layout->addLayout(nested, horizontal ? 1 : 0);
                if (!horizontal)
                    layout->setAlignment(nested, Qt::AlignTop);
            }
        } else if (auto *widget = elements.value(item.toString(), nullptr)) {
            const bool bodyOnLine = horizontal && item.toString() == QLatin1String("body");
            layout->addWidget(widget, bodyOnLine ? 1 : 0, Qt::AlignTop);
        }
    }
    return layout;
}

}

void MamChatWidget::scrollToIndex(int index, Mattermost::LongListWidget::Alignment alignment)
{
    followingInitialTail_ = false;
    Mattermost::LongListWidget::scrollToIndex(index, alignment);
}

bool MamChatWidget::sameSender(const MessageView &a, const MessageView &b) const
{
    // The widget is scoped to one conversation. A direct-chat sender can use
    // several resources or display names, whereas a MUC nick is the sender.
    return a.isLocal() == b.isLocal()
        && (!groupChat_ || a.nick() == b.nick());
}

int MamChatWidget::findRelatedMessage(const MessageView &incoming, bool allowHistory) const
{
    if (incoming.type() != MessageView::Message)
        return -1;
    for (int i = items_.size() - 1; i >= 0; --i) {
        const auto &row = items_.at(i);
        const auto &previous = row.message;
        if (previous.type() != MessageView::Message || !sameSender(previous, incoming))
            continue;
        if (allowHistory && incoming.historyId() > 0
            && incoming.historyId() == previous.historyId())
            return i;

        // Explicit correction edges connect revisions even when the target
        // was fetched before or after the correction, or hours earlier.
        if (!incoming.replaceId().isEmpty()
            && row.stanzaIds.contains(incoming.replaceId()))
            return i;
        if (!incoming.messageId().isEmpty()
            && row.stanzaIds.contains(incoming.messageId())) {
            if (incoming.messageId() != previous.messageId())
                return i; // Already proven to be an earlier revision.
            // Bare client stanza IDs aren't verified archive identities.
            // Limit same-version matching to a nearby delivery interval.
            if (incoming.dateTime().isValid() && previous.dateTime().isValid()
                && qAbs(incoming.dateTime().secsTo(previous.dateTime())) <= 300)
                return i;
        }
    }
    return -1;
}

void MamChatWidget::mergeMessage(int index, const MessageView &incoming, const QString &sourceKey)
{
    Row &row = items_[index];
    const auto previous = row.message;
    const bool edited = previous.isEdited() || incoming.isEdited()
        || !previous.replaceId().isEmpty() || !incoming.replaceId().isEmpty();
    if (!previous.messageId().isEmpty())
        row.stanzaIds.insert(previous.messageId());
    if (!previous.replaceId().isEmpty())
        row.stanzaIds.insert(previous.replaceId());
    if (!incoming.messageId().isEmpty())
        row.stanzaIds.insert(incoming.messageId());
    if (!incoming.replaceId().isEmpty())
        row.stanzaIds.insert(incoming.replaceId());

    // An archive fetch after a live edit must not overwrite the latest
    // revision with the old text or the raw JID nick from history.
    const bool newer = incoming.dateTime().isValid()
        && (!previous.dateTime().isValid() || incoming.dateTime() > previous.dateTime());
    if (newer) {
        MessageView updated = incoming;
        updated.setEdited(edited);
        if (!groupChat_ && !previous.nick().isEmpty())
            updated.setNick(previous.nick());
        if (updated.archiveId().isEmpty())
            updated.setArchiveId(previous.archiveId());
        if (updated.historyId() <= 0)
            updated.setHistoryId(previous.historyId());
        row.message = updated;
        replaceItem(index);
    } else if (edited && !previous.isEdited()) {
        row.message.setEdited();
        replaceItem(index);
    }

    // Multiple MAM archive UIDs (one per edit) and any local EDB key belong
    // to this one row; never replace its stable selection/scroll anchor key.
    if (!sourceKey.isEmpty() && sourceKey != row.key) {
        keys_.insert(sourceKey);
        sourceAliases_.insert(sourceKey, row.key);
    }
}

void MamChatWidget::coalesceRevisionRows()
{
    // Connecting an earlier EDB original and a live correction may require
    // merging two rows when intermediate edits arrive out of order. Index
    // stanza IDs once instead of comparing every row against every other row
    // on a long history (linear in rows and revision IDs).
    QHash<QString, int> seen;
    for (int i = 0; i < items_.size();) {
        const Row &candidate = items_.at(i);
        if (candidate.message.type() != MessageView::Message) {
            ++i;
            continue;
        }
        const QString sender = (candidate.message.isLocal()
                                    ? QStringLiteral("local:")
                                    : QStringLiteral("remote:"))
            + (groupChat_ ? candidate.message.nick() : QString());
        const auto identityKey = [&sender](const QString &id) {
            return sender + QLatin1Char('\x1f') + id;
        };

        int keeper = -1;
        for (const QString &id : candidate.stanzaIds) {
            const auto it = seen.constFind(identityKey(id));
            if (it == seen.cend())
                continue;
            const int prior = it.value();
            if (prior < i
                && (items_.at(prior).message.isEdited() || candidate.message.isEdited())) {
                keeper = prior;
                break;
            }
        }

        if (keeper < 0) {
            for (const QString &id : candidate.stanzaIds)
                seen.insert(identityKey(id), i);
            ++i;
            continue;
        }

        const QString survivingKey = items_.at(keeper).key;
        const QString removedKey = candidate.key;
        Row merged = items_.at(keeper);
        merged.stanzaIds.unite(candidate.stanzaIds);
        if (candidate.message.dateTime().isValid()
            && (!merged.message.dateTime().isValid()
                || candidate.message.dateTime() > merged.message.dateTime()))
            merged.message = candidate.message;
        merged.message.setEdited();
        items_[keeper] = merged;

        for (auto it = sourceAliases_.begin(); it != sourceAliases_.end(); ++it)
            if (it.value() == removedKey)
                it.value() = survivingKey;
        sourceAliases_.insert(removedKey, survivingKey);
        if (selectedKeys_.remove(removedKey))
            selectedKeys_.insert(survivingKey);
        if (selectionAnchorKey_ == removedKey)
            selectionAnchorKey_ = survivingKey;
        if (pressedKey_ == removedKey)
            pressedKey_ = survivingKey;

        // These indices are all before i; removing i never invalidates them.
        for (const QString &id : merged.stanzaIds)
            seen.insert(identityKey(id), keeper);
        items_.removeAt(i);
        removeItems(i, 1);
        replaceItem(keeper);
        // Re-check the same index: it now contains the next logical row.
    }
}

void MamChatWidget::appendMessage(const MessageView &view)
{
    // The legacy message dispatch path also sees receipts, OMEMO wrappers,
    // and other protocol-only events with an empty display body. Rendering
    // them as chat messages produces blank avatar/timestamp rows.
    if (view.type() == MessageView::Message
        && view.formattedText().isEmpty() && view.formattedUserText().isEmpty()
        && view.references().isEmpty() && view.urls().isEmpty())
        return;
    if (((view.type() == MessageView::MUCJoin || view.type() == MessageView::MUCPart) && view.isJoinLeaveHidden())
        || (view.type() == MessageView::Status && view.isStatusChangeHidden()))
        return;
    if (view.type() == MessageView::Reactions
        || view.type() == MessageView::MessageRetraction)
        return;

    if (view.historyId() > 0) {
        prependArchivePage({qMakePair(view.archiveId(), view)});
        return;
    }

    // A newly sent local message is an explicit navigation intent: go to the
    // live tail even if the user was reading older history. Editing an old
    // message must NOT change the viewport.
    const bool sentNewMessage = view.type() == MessageView::Message
        && view.isLocal() && !view.isSpooled() && view.replaceId().isEmpty();
    if (view.type() == MessageView::Message) {
        const int existing = findRelatedMessage(view, false);
        if (existing >= 0) {
            mergeMessage(existing, view);
            if (sentNewMessage)
                scrollToEnd();
            return;
        }
    }

    const QString key = !view.archiveId().isEmpty()
        ? QStringLiteral("mam:") + view.archiveId()
        : QStringLiteral("live:generated:") + QString::number(++generatedKey_);
    if (keys_.contains(key))
        return;

    const bool followTail = followingInitialTail_ || isAtEnd() || sentNewMessage;
    keys_.insert(key);
    Row row {key, view, {}};
    if (!view.messageId().isEmpty())
        row.stanzaIds.insert(view.messageId());
    if (!view.replaceId().isEmpty())
        row.stanzaIds.insert(view.replaceId());
    const int at = items_.size();
    items_.append(row);
    insertItems(at, 1);
    setRangeAvailable(at, at, true);
    if (followTail)
        scrollToEnd();
}

int MamChatWidget::prependArchivePage(const QList<QPair<QString, MessageView>> &page)
{
    const bool followTail = followingInitialTail_ || isAtEnd();
    int added = 0;
    int floor = 0;
    bool containsCorrection = false;
    for (const auto &entry : page)
        containsCorrection |= entry.second.isEdited();
    // Reconcile revisions backwards FIRST: the live row might be revision 8,
    // while this historical page starts at revision 6. Doing only an
    // ascending pass inserts 6 before learning from 7 that it belongs to 8.
    QSet<QString> preMergedKeys;
    for (auto it = page.crbegin(); it != page.crend(); ++it) {
        const auto &incoming = it->second;
        if (incoming.type() == MessageView::Message
            && incoming.formattedText().isEmpty() && incoming.formattedUserText().isEmpty()
            && incoming.references().isEmpty() && incoming.urls().isEmpty())
            continue;
        if (incoming.type() != MessageView::Message
            || (it->first.isEmpty() && incoming.historyId() <= 0))
            continue;
        const QString key = !it->first.isEmpty()
            ? QStringLiteral("mam:") + it->first
            : QStringLiteral("edb:") + QString::number(incoming.historyId());
        if (keys_.contains(key))
            continue;
        const int existing = findRelatedMessage(incoming, true);
        if (existing < 0)
            continue;
        mergeMessage(existing, incoming, key);
        preMergedKeys.insert(key);
    }
    // Then insert genuinely new messages in chronological page order.
    // Pre-merged corrections must not move the ordering floor to a live
    // tail row, otherwise older unrelated messages land after the tail.
    for (const auto &entry : page) {
        const auto &incoming = entry.second;
        if (incoming.type() == MessageView::Message
            && incoming.formattedText().isEmpty() && incoming.formattedUserText().isEmpty()
            && incoming.references().isEmpty() && incoming.urls().isEmpty())
            continue;
        if (entry.first.isEmpty() && incoming.historyId() <= 0)
            continue;
        const QString key = !entry.first.isEmpty()
            ? QStringLiteral("mam:") + entry.first
            : QStringLiteral("edb:") + QString::number(incoming.historyId());
        if (keys_.contains(key)) {
            const int existing = indexOfKey(key);
            if (existing >= 0 && !preMergedKeys.contains(key))
                floor = qMax(floor, existing + 1);
            continue;
        }

        const int existing = findRelatedMessage(incoming, true);
        if (existing >= 0) {
            mergeMessage(existing, incoming, key);
            floor = qMax(floor, existing + 1);
            continue;
        }
        int at = floor;
        while (at < items_.size()
               && items_.at(at).message.dateTime() < incoming.dateTime())
            ++at;
        keys_.insert(key);
        Row row {key, incoming, {}};
        if (!incoming.messageId().isEmpty())
            row.stanzaIds.insert(incoming.messageId());
        if (!incoming.replaceId().isEmpty())
            row.stanzaIds.insert(incoming.replaceId());
        items_.insert(at, row);
        insertItems(at, 1, InsertViewportPolicy::PreserveVisibleContent);
        setRangeAvailable(at, at, true);
        floor = at + 1;
        ++added;
    }
    if (containsCorrection)
        coalesceRevisionRows();
    if (followTail && !page.isEmpty())
        scrollToEnd();
    return added;
}

void MamChatWidget::setMediaOpener(FileSharingDeviceOpener *opener)
{
    if (mediaOpener_ == opener)
        return;
    mediaOpener_ = opener;
    for (int i = 0; i < items_.size(); ++i)
        if (itemWidget(i))
            replaceItem(i);
}

void MamChatWidget::clearMessages()
{
    cancelMessageSelection();
    items_.clear();
    keys_.clear();
    sourceAliases_.clear();
    // A plain click may leave an anchor without a selected range. Reset it
    // when changing conversations, including keys reused by the next model.
    selectionAnchorKey_.clear();
    mousePressed_ = false;
    selectingByDrag_ = false;
    swallowMouseRelease_ = false;
    pressedKey_.clear();
    generatedKey_ = 0;
    followingInitialTail_ = true;
    automaticInitialFillRemaining_ = 1;
    olderRequestQueued_ = false;
    historyLoading_ = false;
    historyExhausted_ = false;
    setItemCount(0);
    timestampTimer_->stop();
}

void MamChatWidget::requestOlderIfNeeded(bool viewportShort)
{
    if (historyLoading_ || historyExhausted_ || olderRequestQueued_)
        return;
    if (followingInitialTail_ && automaticInitialFillRemaining_ <= 0)
        return;
    if (viewportShort && contentHeight() > viewport()->height())
        return;
    olderRequestQueued_ = true;
    QTimer::singleShot(0, this, [this, viewportShort] {
        olderRequestQueued_ = false;
        if (historyLoading_ || historyExhausted_
            || (viewportShort && contentHeight() > viewport()->height())
            || (followingInitialTail_ && automaticInitialFillRemaining_ <= 0))
            return;
        if (followingInitialTail_)
            --automaticInitialFillRemaining_;
        emit olderMessagesRequested();
    });
}

void MamChatWidget::setHistoryLoading(bool loading)
{
    historyLoading_ = loading;
    if (!loading)
        requestOlderIfNeeded(true);
}

void MamChatWidget::setHistoryExhausted(bool exhausted)
{
    historyExhausted_ = exhausted;
}

QWidget *MamChatWidget::createItemWidget(int index)
{
    const auto &mv = items_.at(index).message;
    auto *row = new ChatMessageRow;
    row->setFrameShape(QFrame::NoFrame);
    row->setObjectName(QStringLiteral("mamMessageRow"));
    row->setProperty("_psiMamRowKey", items_.at(index).key);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    row->setFont(font());
    row->setProperty("_psiMamInformational", mv.type() != MessageView::Message);
    // Only the timestamp owns the full date/time tooltip.
    row->installEventFilter(this);

    row->setStyleSheet(theme_.styleSheet);
    auto *outer = new QHBoxLayout(row);
    const bool modern = theme_.layout == NativeChatTheme::Modern;
    const bool message = mv.type() == MessageView::Message;
    outer->setContentsMargins(12, modern ? 8 : 2, 12, modern ? 8 : 2);
    outer->setSpacing(theme_.spacing);
    auto *selection = new QCheckBox(row);
    selection->setObjectName(QStringLiteral("mamMessageSelection"));
    selection->setToolTip(tr("Select message"));
    selection->setAccessibleName(tr("Select message"));
    selection->setVisible(!selectedKeys_.isEmpty());
    selection->setChecked(selectedKeys_.contains(items_.at(index).key));
    outer->addWidget(selection, 0, Qt::AlignTop);
    connect(selection, &QCheckBox::toggled, this, [this, row](bool checked) {
        // A prepend shifts indices, and a MAM overlap can adopt a new key.
        // Resolve the row's current identity at the time of the click.
        const QString key = row->property("_psiMamRowKey").toString();
        const int index = indexOfKey(key);
        if (index >= 0) {
            if (checked)
                selectionAnchorKey_ = key;
            setMessageSelected(index, checked);
        }
    });

    auto *content = new QWidget(row);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    outer->addWidget(content, 1, Qt::AlignTop);
    QHash<QString, QWidget *> elements;

    if (message) {
        const QString nick = mv.nick().isEmpty()
            ? (mv.isLocal() ? tr("Me") : tr("Message")) : mv.nick();
        auto *author = new QLabel(modern ? nick : nick + QLatin1Char(':'), content);
        author->setObjectName(QStringLiteral("mamAuthor"));
        author->setTextFormat(Qt::PlainText);
        author->setTextInteractionFlags(Qt::TextSelectableByMouse);
        author->setForegroundRole(QPalette::Text);
        QFont nameFont = author->font();
        nameFont.setBold(modern);
        author->setFont(nameFont);
        elements.insert(QStringLiteral("author"), author);
        author->installEventFilter(this);
        auto *time = new QLabel(modern ? displayTime(mv.dateTime())
                         : QStringLiteral("[") + mv.dateTime().toLocalTime().toString(
                               QStringLiteral("HH:mm:ss")) + QStringLiteral("]"), content);
        time->setObjectName(QStringLiteral("mamTimestamp"));
        time->setTextFormat(Qt::PlainText);
        time->setToolTip(fullTimestamp(mv.dateTime()));
        time->setForegroundRole(QPalette::Text);
        time->installEventFilter(this);
        if (mv.isEdited()) {
            // Keep the edit marker inside the timestamp slot, so plugins
            // can place the entire semantic header wherever they choose.
            auto *timeGroup = new QWidget(content);
            timeGroup->setObjectName(QStringLiteral("mamTimestampGroup"));
            auto *timeLayout = new QHBoxLayout(timeGroup);
            timeLayout->setContentsMargins(0, 0, 0, 0);
            timeLayout->setSpacing(7);
            timeLayout->addWidget(time);
            auto *pencil = new QLabel(timeGroup);
            pencil->setObjectName(QStringLiteral("mamEditedIndicator"));
            // Use the existing Psi system icon, not a font-dependent Unicode
            // glyph. Match the time font so the marker fits either layout.
            const int iconSize = qMax(12, QFontMetrics(time->font()).height());
            const QPixmap icon = IconsetFactory::iconPixmap(
                QStringLiteral("psi/action_templates_edit"), QSize(iconSize, iconSize));
            if (!icon.isNull())
                pencil->setPixmap(icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
            pencil->setToolTip(tr("The message was corrected"));
            pencil->setAccessibleName(tr("Edited message"));
            pencil->setForegroundRole(QPalette::Text);
            timeLayout->addWidget(pencil);
            elements.insert(QStringLiteral("timestamp"), timeGroup);
        } else {
            elements.insert(QStringLiteral("timestamp"), time);
        }
        if (theme_.avatarSize > 0) {
            auto *avatar = new QLabel(content);
            avatar->setObjectName(QStringLiteral("mamAvatar"));
            avatar->setCursor(Qt::PointingHandCursor);
            avatar->setFixedSize(theme_.avatarSize, theme_.avatarSize);
            updateAvatar(avatar, mv);
            elements.insert(QStringLiteral("avatar"), avatar);
            avatar->installEventFilter(this);
        }
        updateNicknameColor(author, mv);
        updateTimestampAppearance(time);
    }

    auto *body = new ChatMessageText(content);
    body->setObjectName(QStringLiteral("mamBody"));
    body->setMediaOpener(mediaOpener_);
    body->setFont(font());
    body->setForegroundRole(QPalette::Text);
    QString richText = mv.formattedText();
    if (!mv.formattedUserText().isEmpty())
        richText += (richText.isEmpty() ? QString() : QStringLiteral(" — ")) + mv.formattedUserText();
    body->setRichText(richText);
    elements.insert(QStringLiteral("body"), body);
    body->installEventFilter(this);
    body->viewport()->installEventFilter(this);

    if (modern && theme_.layoutSpec.isEmpty()) {
        // Keep Psi's original Modern presentation geometry intact. Qt6 treats
        // nested QBoxLayouts differently from Qt5 when the body has a custom
        // height-for-width and the header is not a separate QWidget.
        if (auto *avatar = elements.value(QStringLiteral("avatar"), nullptr))
            outer->insertWidget(1, avatar, 0, Qt::AlignTop);
        auto *column = new QVBoxLayout(content);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);
        if (message) {
            auto *header = new QWidget(content);
            header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(0, 0, 0, 0);
            headerLayout->setSpacing(10);
            headerLayout->addWidget(elements.value(QStringLiteral("author")));
            headerLayout->addWidget(elements.value(QStringLiteral("timestamp")));
            headerLayout->addStretch();
            column->addWidget(header, 0, Qt::AlignTop);
        }
        column->addWidget(body, 0, Qt::AlignTop);
    } else {
        // Classic and plugin themes can choose any nested row/column
        // arrangement without owning or replacing the message widget.
        const QJsonObject layout = resolvedLayout(theme_);
        content->setLayout(buildThemeLayout(layout, elements, theme_));
        QSet<QString> used;
        validateLayout(layout, used);
        for (auto it = elements.cbegin(); it != elements.cend(); ++it)
            if (!used.contains(it.key()))
                it.value()->hide();
    }
    if (!message) {
        QTextCursor cursor(body->document());
        cursor.select(QTextCursor::Document);
        QTextBlockFormat format;
        format.setAlignment(mv.type() == MessageView::Subject ? Qt::AlignLeft : Qt::AlignHCenter);
        cursor.mergeBlockFormat(format);
    }
    updateRowSelection(row, selectedKeys_.contains(items_.at(index).key));
    return row;
}

QString MamChatWidget::displayTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return {};
    const qint64 age = now.toMSecsSinceEpoch() - when.toMSecsSinceEpoch();
    if (age < 0 || age >= 24 * 60 * 60 * 1000)
        return when.toLocalTime().toString(QStringLiteral("HH:mm"));
    const int seconds = int(age / 1000);
    if (seconds < 60)
        return tr("just now");
    if (seconds < 3600)
        return (seconds / 60 == 1 ? tr("1 minute ago") : tr("%n minutes ago", nullptr, seconds / 60));
    return (seconds / 3600 == 1 ? tr("1 hour ago") : tr("%n hours ago", nullptr, seconds / 3600));
}

QString MamChatWidget::fullTimestamp(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    const auto local = when.toLocalTime();
    return QLocale().toString(local.date(), QLocale::LongFormat) + QStringLiteral(" ")
        + local.toString(QStringLiteral("HH:mm:ss t"));
}

void MamChatWidget::updateTimestampAppearance(QLabel *label)
{
    // Always derive the timestamp font from the CHAT font, never the label's
    // previous font: recreating a row or switching themes must not progressively
    // shrink it. Classic keeps the full-size legacy [HH:mm:ss] presentation.
    QFont timeFont = font();
    const bool classic = theme_.layout == NativeChatTheme::Classic;
    if (!classic) {
        if (timeFont.pointSizeF() > 0)
            timeFont.setPointSizeF(qMax(7.0, timeFont.pointSizeF() - 1.0));
        else if (timeFont.pixelSize() > 0)
            timeFont.setPixelSize(qMax(9, timeFont.pixelSize() - 1));
    }
    label->setFont(timeFont);

    QColor color = palette().color(QPalette::Text);
    if (classic && groupChat_) {
        // The QTextEdit-based MUC Classic painted [time] and nickname with
        // the SAME nick color, including informational/uncolored history.
        if (auto *author = label->parentWidget()->findChild<QLabel *>(
                QStringLiteral("mamAuthor")))
            color = author->palette().color(QPalette::Text);
    } else if (!classic) {
        color.setAlpha(188); // The Modern time stays legible but unobtrusive.
    }
    QPalette colors = label->palette();
    colors.setColor(QPalette::Text, color);
    colors.setColor(QPalette::WindowText, color);
    label->setPalette(colors);
}

void MamChatWidget::updateTimestamps()
{
    timestampTimer_->stop();
    // Hidden tabs/windows and Classic's absolute timestamps require no clock.
    if (!isVisible() || window()->isMinimized()
        || theme_.layout == NativeChatTheme::Classic)
        return;

    const auto range = visibleRange();
    if (!range.isValid())
        return;
    const auto now = QDateTime::currentDateTime();
    const qint64 minute = 60 * 1000;
    const qint64 hour = 60 * minute;
    const qint64 day = 24 * hour;
    qint64 nextRefresh = std::numeric_limits<int>::max();

    // The prefetch/materialization buffer can be much larger than the
    // viewport. Touch only rows that actually intersect the visible area.
    for (int i = range.first; i <= range.last; ++i) {
        auto *row = itemWidget(i);
        if (!row || !row->isVisibleTo(viewport())
            || !QRect(row->mapTo(viewport(), QPoint()), row->size()).intersects(viewport()->rect()))
            continue;
        auto *label = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        if (!label)
            continue;
        const auto when = items_.at(i).message.dateTime();
        if (!when.isValid())
            continue;
        const auto text = displayTime(when, now);
        if (label->text() != text)
            label->setText(text);

        const qint64 age = when.msecsTo(now);
        qint64 untilNext = 0;
        if (age < 0)
            untilNext = -age; // Future timestamp becomes "just now".
        else if (age < hour)
            untilNext = minute - age % minute;
        else if (age < day)
            untilNext = hour - age % hour;
        // Beyond 24 hours we show a fixed clock time, with no timer.
        if (untilNext > 0)
            nextRefresh = qMin(nextRefresh, untilNext + 50);
    }
    if (nextRefresh < std::numeric_limits<int>::max())
        timestampTimer_->start(int(nextRefresh));
}

void MamChatWidget::changeEvent(QEvent *event)
{
    Mattermost::LongListWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        for (int index : materializedIndices()) {
            auto *row = itemWidget(index);
            if (!row)
                continue;
            row->setFont(font());
            if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"))) {
                QFont authorFont = font();
                authorFont.setBold(theme_.layout == NativeChatTheme::Modern);
                author->setFont(authorFont);
            }
            if (auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
                updateTimestampAppearance(time);
            if (auto *body = row->findChild<ChatMessageText *>())
                body->setFont(font());
        }
        if (!items_.isEmpty())
            itemsChanged(0, items_.size() - 1);
    }
    else if (event->type() == QEvent::PaletteChange) {
        // QAbstractScrollArea can retain the old resolved viewport palette.
        viewport()->setPalette({});
        refreshAvatars();
        refreshSelection();
        refreshNicknameColors();
        for (int index : materializedIndices())
            if (auto *row = itemWidget(index))
                if (auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp")))
                    updateTimestampAppearance(time);
        emit paletteChanged();
    }
}

void MamChatWidget::setTheme(const NativeChatTheme &theme)
{
    theme_ = theme;
    for (int index : materializedIndices())
        replaceItem(index);
    updateTimestamps();
}

void MamChatWidget::setNicknameColorResolver(std::function<QColor(const MessageView &)> resolver)
{
    nicknameColorResolver_ = std::move(resolver);
    refreshNicknameColors();
}

void MamChatWidget::refreshNicknameColors()
{
    for (int index : materializedIndices()) {
        if (auto *row = itemWidget(index))
            if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor")))
                updateNicknameColor(author, items_.at(index).message);
    }
}

void MamChatWidget::updateNicknameColor(QLabel *author, const MessageView &message)
{
    if (!nicknameColorResolver_)
        return;
    const QColor color = nicknameColorResolver_(message);
    QPalette palette = author->palette();
    palette.setColor(QPalette::Text, color.isValid() ? color : this->palette().color(QPalette::Text));
    palette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
    author->setPalette(palette);
    if (auto *time = author->parentWidget()->findChild<QLabel *>(
            QStringLiteral("mamTimestamp")))
        updateTimestampAppearance(time);
}

void MamChatWidget::setAvatarResolver(std::function<QPixmap(const MessageView &)> resolver)
{
    avatarResolver_ = std::move(resolver);
    refreshAvatars();
}

void MamChatWidget::updateAvatar(QLabel *label, const MessageView &message)
{
    const auto source = avatarResolver_ ? avatarResolver_(message) : QPixmap();
    const int size = label->width();
    const auto ratio = devicePixelRatioF();
    QPixmap picture(qRound(size * ratio), qRound(size * ratio));
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), 6, 6);
    painter.setClipPath(clip);
    if (source.isNull()) {
        painter.fillRect(QRect(0, 0, size, size), palette().brush(QPalette::Highlight));
        painter.setPen(palette().color(QPalette::HighlightedText));
        QFont initial = font();
        initial.setPixelSize(qMax(12, size / 2));
        painter.setFont(initial);
        const QString nick = message.nick().isEmpty()
            ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, nick.left(1).toUpper());
    } else {
        const auto scaled = source.scaled(picture.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRect crop((scaled.width() - picture.width()) / 2, (scaled.height() - picture.height()) / 2,
                         picture.width(), picture.height());
        painter.drawPixmap(QRect(0, 0, size, size), scaled, crop);
    }
    painter.end();
    label->setPixmap(picture);
}

void MamChatWidget::refreshAvatars()
{
    for (int i : materializedIndices()) {
        if (auto *row = itemWidget(i))
            if (auto *avatar = row->findChild<QLabel *>(QStringLiteral("mamAvatar")))
                updateAvatar(avatar, items_.at(i).message);
    }
}

int MamChatWidget::indexOfKey(const QString &key) const
{
    if (key.isEmpty())
        return -1;
    const QString canonical = sourceAliases_.value(key, key);
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).key == canonical)
            return i;
    }
    return -1;
}

QFrame *MamChatWidget::eventRow(QObject *target) const
{
    QWidget *child = qobject_cast<QWidget *>(target);
    while (child && child != viewport()) {
        if (auto *frame = qobject_cast<QFrame *>(child)) {
            if (frame->objectName() == QLatin1String("mamMessageRow"))
                return frame;
        }
        child = child->parentWidget();
    }
    return nullptr;
}

void MamChatWidget::selectMessageRange(int first, int last)
{
    if (first < 0 || last < 0 || first >= items_.size() || last >= items_.size())
        return;
    if (first > last)
        qSwap(first, last);
    selectedKeys_.clear();
    for (int i = first; i <= last; ++i)
        selectedKeys_.insert(items_.at(i).key);
    refreshSelection();
}

void MamChatWidget::setMessageSelected(int index, bool selected)
{
    if (index < 0 || index >= items_.size())
        return;
    const QString &key = items_.at(index).key;
    if (selected)
        selectedKeys_.insert(key);
    else
        selectedKeys_.remove(key);
    refreshSelection();
}

void MamChatWidget::cancelMessageSelection()
{
    if (selectedKeys_.isEmpty() && !selectionToolbar_->isVisible())
        return;
    selectedKeys_.clear();
    selectionAnchorKey_.clear();
    selectingByDrag_ = false;
    refreshSelection();
}

void MamChatWidget::copySelectedMessages()
{
    if (selectedKeys_.isEmpty())
        return;
    // Model-backed copying works even when LongListWidget evicted the row UI.
    QStringList blocks;
    for (const auto &row : items_) {
        if (!selectedKeys_.contains(row.key))
            continue;
        const auto &message = row.message;
        QTextDocument document;
        QString rich = message.formattedText();
        if (!message.formattedUserText().isEmpty())
            rich += (rich.isEmpty() ? QString() : QStringLiteral(" — ")) + message.formattedUserText();
        PsiRichText::setText(&document, rich);
        const QString nick = message.nick().isEmpty()
            ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
        const QString timestamp = message.dateTime().isValid()
            ? message.dateTime().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();
        blocks.append(QStringLiteral("%1\t[%2]\n%3")
                          .arg(nick, timestamp, PsiRichText::convertToPlainText(&document).trimmed()));
    }
    if (!blocks.isEmpty())
        QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n\n")));
}

void MamChatWidget::setInformationalColor(const QColor &color)
{
    informationalColor_ = color;
    refreshSelection();
}

void MamChatWidget::updateRowSelection(QFrame *row, bool selected)
{
    const bool selecting = !selectedKeys_.isEmpty();
    if (auto *check = row->findChild<QCheckBox *>(QStringLiteral("mamMessageSelection"))) {
        const QSignalBlocker blocker(check);
        check->setChecked(selected);
        check->setVisible(selecting);
    }
    const QColor base = viewport()->palette().color(QPalette::Base);
    const QColor accent = viewport()->palette().color(QPalette::Highlight);
    const QColor surface = selected
        ? QColor((base.red() * 3 + accent.red()) / 4,
                 (base.green() * 3 + accent.green()) / 4,
                 (base.blue() * 3 + accent.blue()) / 4) : base;
    QPalette palette = viewport()->palette();
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::Base, surface); // QTextDocument's surface, too.
    row->setPalette(palette);
    row->setAutoFillBackground(true);
    if (auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"))) {
        author->setTextInteractionFlags(selecting ? Qt::NoTextInteraction : Qt::TextSelectableByMouse);
        if (selecting)
            author->setSelection(0, 0);
    }
    if (auto *body = row->findChild<ChatMessageText *>()) {
        QPalette textPalette = palette;
        if (row->property("_psiMamInformational").toBool())
            textPalette.setColor(QPalette::Text, informationalColor_.isValid()
                ? informationalColor_ : palette.color(QPalette::Link));
        body->setPalette(textPalette);
        body->setWholeMessageSelectionMode(selecting);
    }
    row->update();
}

void MamChatWidget::refreshSelection()
{
    // Only paint materialized rows. Selection itself is stored independently
    // and automatically applied by createItemWidget after virtualization.
    const auto range = materializedRange();
    if (range.isValid()) {
        for (int i = range.first; i <= range.last; ++i) {
            auto *row = qobject_cast<QFrame *>(itemWidget(i));
            if (!row || row->objectName() != QLatin1String("mamMessageRow"))
                continue;
            updateRowSelection(row, selectedKeys_.contains(items_.at(i).key));
        }
    }
    if (auto *shortcut = findChild<QShortcut *>(QStringLiteral("mamCopyShortcut")))
        shortcut->setEnabled(!selectedKeys_.isEmpty());
    if (selectedKeys_.isEmpty()) {
        selectionToolbar_->hide();
        return;
    }
    selectionCountLabel_->setText(tr("%1 selected").arg(selectedKeys_.size()));
    positionSelectionToolbar();
    selectionToolbar_->show();
    selectionToolbar_->raise();
}

void MamChatWidget::positionSelectionToolbar()
{
    if (!selectionToolbar_)
        return;
    selectionToolbar_->adjustSize();
    selectionToolbar_->move(qMax(8, (viewport()->width() - selectionToolbar_->width()) / 2), 8);
}

void MamChatWidget::resizeEvent(QResizeEvent *event)
{
    Mattermost::LongListWidget::resizeEvent(event);
    // The first layout pass may run while the room is still hidden; restoring
    // its old geometric anchor must never cancel the initial at-end intent.
    if (followingInitialTail_ && !items_.isEmpty())
        scrollToEnd();
    positionSelectionToolbar();
}

void MamChatWidget::showEvent(QShowEvent *event)
{
    Mattermost::LongListWidget::showEvent(event);
    // Revisit after Qt has measured the splitter/viewport on first display.
    // Any intervening user scroll has already released the initial tail.
    QTimer::singleShot(0, this, [this] {
        if (followingInitialTail_ && !items_.isEmpty())
            scrollToEnd();
        updateTimestamps();
    });
}

void MamChatWidget::hideEvent(QHideEvent *event)
{
    timestampTimer_->stop();
    Mattermost::LongListWidget::hideEvent(event);
}

bool MamChatWidget::eventFilter(QObject *target, QEvent *event)
{
    if (event && (event->type() == QEvent::MouseButtonPress
                  || event->type() == QEvent::MouseMove
                  || event->type() == QEvent::MouseButtonRelease)) {
        if (QFrame *row = eventRow(target)) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const QString key = row->property("_psiMamRowKey").toString();
            const int index = indexOfKey(key);
            if (index >= 0) {
                if (event->type() == QEvent::MouseButtonPress
                    && mouse->button() == Qt::LeftButton) {
                    const bool extended = mouse->modifiers().testFlag(Qt::ShiftModifier);
                    const bool toggle = mouse->modifiers().testFlag(Qt::ControlModifier);
                    if (extended || toggle || !selectedKeys_.isEmpty()) {
                        const int anchor = indexOfKey(selectionAnchorKey_);
                        if (extended && anchor >= 0)
                            selectMessageRange(anchor, index);
                        else
                            setMessageSelected(index, !selectedKeys_.contains(key));
                        if (!extended || selectionAnchorKey_.isEmpty())
                            selectionAnchorKey_ = key;
                        swallowMouseRelease_ = true;
                        mousePressed_ = false;
                        return true;
                    }
                    // Do not steal drag-selection of text *within* a message.
                    pressedKey_ = key;
                    selectionAnchorKey_ = key;
                    mousePressed_ = true;
                } else if (event->type() == QEvent::MouseMove && mousePressed_) {
                    // The press/release lifecycle belongs to this view. Qt5's
                    // synthetic mouse-move delivery can omit buttons() even
                    // while the row holds a live left-button press.

                    auto *source = qobject_cast<QWidget *>(target);
                    const QPoint where = source->mapTo(viewport(), mouse->pos());
                    const QRect rect(row->mapTo(viewport(), QPoint()), row->size());
                    if (!selectingByDrag_ && where.y() >= rect.top()
                        && where.y() <= rect.bottom())
                        return Mattermost::LongListWidget::eventFilter(target, event);
                    const int anchor = indexOfKey(pressedKey_);
                    const int now = indexAtViewportPosition(where.y());
                    if (anchor >= 0 && now >= 0) {
                        selectingByDrag_ = true;
                        selectMessageRange(anchor, now);
                        return true;
                    }
                } else if (event->type() == QEvent::MouseButtonRelease
                           && mouse->button() == Qt::LeftButton) {
                    mousePressed_ = false;
                    const bool consumed = selectingByDrag_ || swallowMouseRelease_;
                    selectingByDrag_ = false;
                    swallowMouseRelease_ = false;
                    if (consumed)
                        return true;
                    if (selectedKeys_.isEmpty()
                        && target->objectName() == QLatin1String("mamAvatar")) {
                        auto *avatar = qobject_cast<QLabel *>(target);
                        if (avatar && avatar->rect().contains(mouse->pos())) {
                            const auto &sender = items_.at(index).message;
                            emit avatarClicked(sender.nick(), sender.isLocal());
                            return true;
                        }
                    }
                    if (groupChat_ && selectedKeys_.isEmpty()
                        && target->objectName() == QLatin1String("mamAuthor")) {
                        auto *author = qobject_cast<QLabel *>(target);
                        if (author && !author->hasSelectedText() && author->rect().contains(mouse->pos())) {
                            emit nicknameClicked(items_.at(index).message.nick());
                        }
                    }
                }
            }
        }
    }
    return Mattermost::LongListWidget::eventFilter(target, event);
}
