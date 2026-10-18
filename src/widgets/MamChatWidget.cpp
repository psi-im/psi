/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamChatWidget.h"
#include "MamMessageIdentity.h"
#include "ChatMessageText.h"
#include "psirichtext.h"

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
    auto *clock = new QTimer(this);
    clock->setInterval(1000);
    connect(clock, &QTimer::timeout, this, &MamChatWidget::updateTimestamps);
    clock->start();
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

    connect(this, &Mattermost::LongListWidget::visibleRangeChanged, this,
            [this](int first, int) {
        if (first <= 1 && !historyLoading_ && !historyExhausted_)
            emit olderMessagesRequested();
    });
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

bool sameDelivery(const MessageView &a, const MessageView &b)
{
    return PsiMam::sameDelivery(
        {a.messageId(), a.text(), a.isLocal(), a.dateTime()},
        {b.messageId(), b.text(), b.isLocal(), b.dateTime()});
}

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

void MamChatWidget::appendMessage(const MessageView &view)
{
    if (((view.type() == MessageView::MUCJoin || view.type() == MessageView::MUCPart) && view.isJoinLeaveHidden())
        || (view.type() == MessageView::Status && view.isStatusChangeHidden()))
        return;
    // Corrections change the existing logical identity and must preserve its
    // verified archive UID. The hidden legacy ChatView still receives the
    // protocol update during this incremental UI migration.
    if (view.type() == MessageView::Message && !view.replaceId().isEmpty()) {
        for (int i = items_.size() - 1; i >= 0; --i) {
            const auto &previous = items_.at(i).message;
            if (previous.type() == MessageView::Message
                && previous.messageId() == view.replaceId()
                && previous.isLocal() == view.isLocal()
                && (!groupChat_ || previous.nick() == view.nick())) {
                MessageView corrected = view;
                if (corrected.archiveId().isEmpty())
                    corrected.setArchiveId(previous.archiveId());
                items_[i].message = corrected;
                replaceItem(i);
                return;
            }
        }
    }

    // Receipt/reaction/retraction payloads are updates rather than standalone
    // text rows. Until authenticated per-sender state is migrated, the hidden
    // ChatView remains their compatibility implementation.
    if (view.type() == MessageView::Reactions
        || view.type() == MessageView::MessageRetraction)
        return;

    if (view.historyId() > 0) {
        prependArchivePage({qMakePair(view.archiveId(), view)});
        return;
    }

    // Archive UIDs and client stanza IDs are different identity namespaces.
    const QString key = !view.archiveId().isEmpty()
        ? QStringLiteral("mam:") + view.archiveId()
        : QStringLiteral("live:generated:") + QString::number(++generatedKey_);
    if (keys_.contains(key))
        return;

    // A live echo may arrive just after the same message's initial MAM page.
    // Correlation requires matching sender direction, stanza ID, original text
    // and a small timestamp window; never deduplicate solely on client ID.
    if (view.type() == MessageView::Message && !view.messageId().isEmpty()) {
        const int first = qMax(0, items_.size() - 256);
        for (int i = items_.size() - 1; i >= first; --i) {
            if (items_.at(i).key.startsWith(QLatin1String("mam:"))
                && (!groupChat_ || items_.at(i).message.nick() == view.nick())
                && sameDelivery(items_.at(i).message, view))
                return;
        }
    }

    keys_.insert(key);
    const int at = items_.size();
    items_.append({key, view});
    insertItems(at, 1);
    setRangeAvailable(at, at, true);
}

int MamChatWidget::prependArchivePage(const QList<QPair<QString, MessageView>> &page)
{
    const bool followTail = isAtEnd();
    QList<Row> added;
    QStringList pageOrder;
    for (const auto &entry : page) {
        if (entry.first.isEmpty() && entry.second.historyId() <= 0)
            continue;
        const auto key = !entry.first.isEmpty() ? QStringLiteral("mam:") + entry.first
            : QStringLiteral("edb:") + QString::number(entry.second.historyId());
        pageOrder.append(key);
        if (keys_.contains(key))
            continue;

        // Adopt the archive UID on an already displayed live counterpart.
        // This avoids creating a duplicate row while a network page was in
        // flight, and still remembers the UID for later overlapping pages.
        bool overlap = false;
        if (entry.second.type() == MessageView::Message) {
            const int first = entry.second.historyId() > 0 ? 0 : qMax(0, items_.size() - 256);
            for (int i = items_.size() - 1; i >= first; --i) {
                const auto &previous = items_.at(i).message;
                const bool sameHistory = entry.second.historyId() > 0
                    && previous.historyId() == entry.second.historyId();
                const bool sameLive = items_.at(i).key.startsWith(QLatin1String("live:"))
                    && (!groupChat_ || previous.nick() == entry.second.nick())
                    && sameDelivery(previous, entry.second);
                if (sameHistory || sameLive) {
                    // Promote the adopted live row into the MAM identity
                    // namespace. Otherwise a second live echo cannot find
                    // this archived counterpart and creates a duplicate.
                    const QString oldKey = items_[i].key;
                    keys_.remove(oldKey);
                    items_[i].key = key;
                    if (selectedKeys_.remove(oldKey))
                        selectedKeys_.insert(key);
                    if (selectionAnchorKey_ == oldKey)
                        selectionAnchorKey_ = key;
                    if (auto *liveWidget = itemWidget(i))
                        liveWidget->setProperty("_psiMamRowKey", key);
                    items_[i].message.setArchiveId(entry.first);
                    items_[i].message.setHistoryId(entry.second.historyId());
                    overlap = true;
                    break;
                }
            }
        }
        keys_.insert(key);
        if (!overlap)
            added.append({key, entry.second});
    }
    if (added.isEmpty())
        return 0;

    // EDB can contain messages absent from MAM. Insert archive rows among
    // these originals instead of putting a new server tail above older local
    // messages. Keep page order even when multiple server stamps are equal.
    int floor = 0;
    for (const auto &key : pageOrder) {
        const int existing = indexOfKey(key);
        if (existing >= 0) {
            floor = qMax(floor, existing + 1);
            continue;
        }
        const auto candidate = std::find_if(added.cbegin(), added.cend(), [&key](const Row &row) { return row.key == key; });
        if (candidate == added.cend())
            continue;
        const auto &row = *candidate;
        int at = floor;
        while (at < items_.size() && items_.at(at).message.dateTime() < row.message.dateTime())
            ++at;
        items_.insert(at, row);
        insertItems(at, 1, InsertViewportPolicy::PreserveVisibleContent);
        setRangeAvailable(at, at, true);
        floor = at + 1;
    }
    if (followTail)
        scrollToEnd();
    return added.size();
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
    // A plain click may leave an anchor without a selected range. Reset it
    // when changing conversations, including keys reused by the next model.
    selectionAnchorKey_.clear();
    mousePressed_ = false;
    selectingByDrag_ = false;
    swallowMouseRelease_ = false;
    pressedKey_.clear();
    generatedKey_ = 0;
    historyLoading_ = false;
    historyExhausted_ = false;
    setItemCount(0);
}

void MamChatWidget::setHistoryLoading(bool loading)
{
    historyLoading_ = loading;
    if (!loading && !historyExhausted_
        && contentHeight() <= viewport()->height()) {
        // If the page has only non-text MAM events (zero visible rows),
        // or cannot fill a viewport, there is no scroll gesture to trigger
        // another request. Continue until rows fill the view or MAM ends.
        // Queue the request to avoid re-entering the MAM completion handler.
        QTimer::singleShot(0, this, [this] {
            if (!historyLoading_ && !historyExhausted_
                && contentHeight() <= viewport()->height())
                emit olderMessagesRequested();
        });
    }
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
    row->setToolTip(fullTimestamp(mv.dateTime()));
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
        elements.insert(QStringLiteral("timestamp"), time);
        time->installEventFilter(this);
        if (theme_.avatarSize > 0) {
            auto *avatar = new QLabel(content);
            avatar->setObjectName(QStringLiteral("mamAvatar"));
            avatar->setFixedSize(theme_.avatarSize, theme_.avatarSize);
            updateAvatar(avatar, mv);
            elements.insert(QStringLiteral("avatar"), avatar);
            avatar->installEventFilter(this);
        }
        updateNicknameColor(author, mv);
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
    body->setToolTip(row->toolTip());
    body->viewport()->setToolTip(row->toolTip());

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
        return (seconds == 1 ? tr("1 second ago") : tr("%n seconds ago", nullptr, seconds));
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

void MamChatWidget::updateTimestamps()
{
    if (!isVisible())
        return;
    const auto now = QDateTime::currentDateTime();
    for (int i : materializedIndices()) {
        if (auto *row = itemWidget(i)) {
            if (auto *label = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"))) {
                const auto when = items_.at(i).message.dateTime();
                const auto text = theme_.layout == NativeChatTheme::Classic
                    ? QStringLiteral("[") + when.toLocalTime().toString(QStringLiteral("HH:mm:ss"))
                        + QStringLiteral("]")
                    : displayTime(when, now);
                if (label->text() != text)
                    label->setText(text);
            }
        }
    }
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
        emit paletteChanged();
    }
}

void MamChatWidget::setTheme(const NativeChatTheme &theme)
{
    theme_ = theme;
    for (int index : materializedIndices())
        replaceItem(index);
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
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).key == key)
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
    positionSelectionToolbar();
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
