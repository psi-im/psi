/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamChatWidget.h"
#include "MamMessageIdentity.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QPalette>
#include <QSizePolicy>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

MamChatWidget::MamChatWidget(QWidget *parent) : Mattermost::LongListWidget(parent)
{
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
bool sameDelivery(const MessageView &a, const MessageView &b)
{
    return PsiMam::sameDelivery(
        {a.messageId(), a.text(), a.isLocal(), a.dateTime()},
        {b.messageId(), b.text(), b.isLocal(), b.dateTime()});
}
}

void MamChatWidget::appendMessage(const MessageView &view)
{
    // Corrections change the existing logical identity and must preserve its
    // verified archive UID. The hidden legacy ChatView still receives the
    // protocol update during this incremental UI migration.
    if (view.type() == MessageView::Message && !view.replaceId().isEmpty()) {
        for (int i = items_.size() - 1; i >= 0; --i) {
            const auto &previous = items_.at(i).message;
            if (previous.type() == MessageView::Message
                && previous.messageId() == view.replaceId()
                && previous.isLocal() == view.isLocal()) {
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
    QList<Row> added;
    for (const auto &entry : page) {
        if (entry.first.isEmpty())
            continue; // Only verified archive UIDs can anchor pagination.
        const auto key = QStringLiteral("mam:") + entry.first;
        if (keys_.contains(key))
            continue;

        // Adopt the archive UID on an already displayed live counterpart.
        // This avoids creating a duplicate row while a network page was in
        // flight, and still remembers the UID for later overlapping pages.
        bool overlap = false;
        if (entry.second.type() == MessageView::Message
            && !entry.second.messageId().isEmpty()) {
            const int first = qMax(0, items_.size() - 256);
            for (int i = items_.size() - 1; i >= first; --i) {
                if (items_.at(i).key.startsWith(QLatin1String("live:"))
                    && sameDelivery(items_.at(i).message, entry.second)) {
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

    // LongListWidget preserves the concrete viewport anchor across prepends.
    for (int i = added.size() - 1; i >= 0; --i)
        items_.prepend(added.at(i));
    insertItems(0, added.size(), InsertViewportPolicy::PreserveVisibleContent);
    setRangeAvailable(0, added.size() - 1, true);
    return added.size();
}

void MamChatWidget::clearMessages()
{
    items_.clear();
    keys_.clear();
    cancelMessageSelection();
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
    auto *row = new QFrame;
    row->setFrameShape(QFrame::NoFrame);
    row->setObjectName(QStringLiteral("mamMessageRow"));
    row->setProperty("_psiMamRowKey", items_.at(index).key);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    row->installEventFilter(this);

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 5, 14, 6);
    layout->setSpacing(3);

    auto *header = new QLabel(row);
    const auto timestamp = mv.dateTime().isValid()
        ? mv.dateTime().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QString();
    const QString nick = mv.nick().isEmpty()
        ? (mv.isLocal() ? tr("Me") : tr("Message")) : mv.nick();
    header->setTextFormat(Qt::PlainText); // Nicknames and JIDs are untrusted text.
    header->setText(QStringLiteral("%1    %2").arg(nick, timestamp));
    header->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->setForegroundRole(QPalette::Mid);
    layout->addWidget(header);
    header->installEventFilter(this);

    auto *body = new QLabel(row);
    body->setTextFormat(Qt::RichText);
    body->setText(mv.formattedUserText().isEmpty()
                      ? mv.formattedText() : mv.formattedUserText());
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    body->setOpenExternalLinks(true);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(body);
    body->installEventFilter(this);
    if (selectedKeys_.contains(items_.at(index).key)) {
        QPalette pal = row->palette();
        const QColor base = viewport()->palette().color(QPalette::Base);
        const QColor accent = pal.color(QPalette::Highlight);
        pal.setColor(QPalette::Window, QColor((base.red() * 3 + accent.red()) / 4,
                                            (base.green() * 3 + accent.green()) / 4,
                                            (base.blue() * 3 + accent.blue()) / 4));
        row->setPalette(pal);
        row->setAutoFillBackground(true);
    }
    return row;
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
        const QString rich = message.formattedUserText().isEmpty()
            ? message.formattedText() : message.formattedUserText();
        document.setHtml(rich);
        const QString nick = message.nick().isEmpty()
            ? (message.isLocal() ? tr("Me") : tr("Message")) : message.nick();
        const QString timestamp = message.dateTime().isValid()
            ? message.dateTime().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();
        blocks.append(QStringLiteral("%1\t[%2]\n%3")
                          .arg(nick, timestamp, document.toPlainText().trimmed()));
    }
    if (!blocks.isEmpty())
        QApplication::clipboard()->setText(blocks.join(QStringLiteral("\n\n")));
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
            const bool selected = selectedKeys_.contains(items_.at(i).key);
            const QColor base = viewport()->palette().color(QPalette::Base);
            const QColor accent = row->palette().color(QPalette::Highlight);
            QPalette palette = row->palette();
            palette.setColor(QPalette::Window, selected
                ? QColor((base.red() * 3 + accent.red()) / 4,
                         (base.green() * 3 + accent.green()) / 4,
                         (base.blue() * 3 + accent.blue()) / 4) : base);
            row->setPalette(palette);
            row->setAutoFillBackground(selected);
            if (selected) {
                for (auto *label : row->findChildren<QLabel *>())
                    label->setSelection(0, 0); // Clear partial QLabel selection.
            }
            row->update();
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
                }
            }
        }
    }
    return Mattermost::LongListWidget::eventFilter(target, event);
}
