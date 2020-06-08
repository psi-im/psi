/*
 * chatview_te.cpp - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "chatview_te.h"

#include "coloropt.h"
#include "common.h"
#include "iconset.h"
#include "messageview.h"
#include "msgmle.h"
#include "psioptions.h"
#include "psirichtext.h"
#include "qiteaudio.h"
#include "textutil.h"
#include "xmpp/jid/jid.h"

#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#ifdef CORRECTION_DEBUG
#include <QDebug>
#endif

//#define CORRECTION_DEBUG

static const char *  informationalColorOpt = "options.ui.look.colors.messages.informational";
static const QRegExp underlineFixRE("(<a href=\"addnick://psi/[^\"]*\"><span style=\")");
static const QRegExp removeTagsRE("<[^>]*>");

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent) :
    PsiTextView(parent), isMuc_(false), isEncryptionEnabled_(false), oldTrackBarPosition(0), dialog_(nullptr)
{
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    setReadOnly(true);
    setUndoRedoEnabled(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLooks(this);

#ifndef Q_OS_LINUX // linux has this feature built-in
    connect(this, SIGNAL(selectionChanged()), SLOT(autoCopy()));
    connect(this, SIGNAL(cursorPositionChanged()), SLOT(autoCopy()));
#endif
    actQuote_ = new QAction(tr("Quote"), this);
    actQuote_->setEnabled(false);
    actQuote_->setShortcut(QKeySequence(tr("Ctrl+S")));
    addAction(actQuote_);
    connect(actQuote_, &QAction::triggered, this, [this](bool) { emit quote(getPlainText()); });
    connect(this, &ChatView::selectionChanged, this, [this]() { actQuote_->setEnabled(textCursor().hasSelection()); });

    addLogIconsResources();
}

ChatView::~ChatView() { }

// something after we know isMuc and dialog is set
void ChatView::init() { }

QSize ChatView::sizeHint() const { return minimumSizeHint(); }

void ChatView::setDialog(QWidget *dialog) { dialog_ = dialog; }

void ChatView::setEncryptionEnabled(bool enabled) { isEncryptionEnabled_ = enabled; }

void ChatView::setSessionData(bool isMuc, bool isMucPrivate, const XMPP::Jid &jid, const QString name)
{
    isMuc_        = isMuc;
    isMucPrivate_ = isMucPrivate;
    jid_          = jid;
    name_         = name;
}

void ChatView::clear()
{
    PsiTextView::clear();
    addLogIconsResources();
}

void ChatView::contextMenuEvent(QContextMenuEvent *e)
{
    const QUrl anc = QUrl::fromEncoded(anchorAt(e->pos()).toLatin1());

    if (anc.scheme() == "addnick") {
        emit showNM(anc.path().mid(1));
        e->accept();
    } else {
        QMenu *menu = createStandardContextMenu(e->pos());
        menu->exec(e->globalPos());
        delete menu;
        e->accept();
    }
}

QMenu *ChatView::createStandardContextMenu(const QPoint &position)
{
    QMenu *menu = PsiTextView::createStandardContextMenu(position);
    menu->addAction(actQuote_);
    return menu;
}

void ChatView::addLogIconsResources()
{
    struct {
        const char *name;
        const char *icon;
    } icons[] = { { "log_icon_receive", "psi/notification_chat_receive" },
                  { "log_icon_send", "psi/notification_chat_send" },
                  { "log_icon_receive_encrypted", "psi/notification_chat_receive_encrypted" },
                  { "log_icon_send_encrypted", "psi/notification_chat_send_encrypted" },
                  { "log_icon_time", "psi/notification_chat_time" },
                  { "log_icon_info", "psi/notification_chat_info" },
                  { "log_icon_delivered", "psi/notification_chat_delivery_ok" },
                  { "log_icon_delivered_encrypted", "psi/notification_chat_delivery_ok_encrypted" },
                  { "log_icon_corrected", "psi/action_templates_edit" },
                  { "log_icon_history", "psi/history" } };

    useMessageIcons_ = PsiOptions::instance()->getOption("options.ui.chat.use-message-icons").toBool();
    int  scaledSize  = int(fontInfo().pixelSize() * EqTextIconK + .5);
    bool scale       = PsiOptions::instance()->getOption("options.ui.chat.scaled-message-icons").toBool();

    auto fs = QFontInfo(font()).pixelSize();
    for (auto &i : icons) {
        auto res = QUrl(QLatin1String("icon:") + i.name);
        if (useMessageIcons_) {
            auto icon = IconsetFactory::iconPixmap(i.icon, scaledSize);
            if (icon.height() > HugeIconTextViewK * fs || scale) {
                icon = icon.scaledToHeight(scaledSize, Qt::SmoothTransformation);
            }
            document()->addResource(QTextDocument::ImageResource, res, icon);
        } else {
            document()->addResource(QTextDocument::ImageResource, res, QVariant());
        }
    }
}

void ChatView::markReceived(QString id)
{
    if (useMessageIcons_) {
        auto delivered = document()->resource(
            QTextDocument::ImageResource,
            QUrl(QLatin1String("icon:")
                 + (isEncryptionEnabled_ ? "log_icon_delivered_encrypted" : "log_icon_delivered")));
        document()->addResource(QTextDocument::ImageResource, QUrl(QString("icon:delivery") + id), delivered);
        setLineWrapColumnOrWidth(lineWrapColumnOrWidth());
    }
}

bool ChatView::focusNextPrevChild(bool next) { return QWidget::focusNextPrevChild(next); }

void ChatView::keyPressEvent(QKeyEvent *e)
{
    /*    if(e->key() == Qt::Key_Escape)
            e->ignore();
    #ifdef Q_OS_MAC
        else if(e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier)
            e->ignore();
        else
    #endif
        else if(e->key() == Qt::Key_Return && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() &
    Qt::AltModifier)) ) e->ignore(); else if(e->key() == Qt::Key_H && (e->modifiers() & Qt::ControlModifier))
            e->ignore();
        else if(e->key() == Qt::Key_I && (e->modifiers() & Qt::ControlModifier))
            e->ignore(); */
    /*else*/ if (e->key() == Qt::Key_M && (e->modifiers() & Qt::ControlModifier) && !isReadOnly()) // newline
        append("\n");
    /*    else if(e->key() == Qt::Key_U && (e->modifiers() & Qt::ControlModifier) && !isReadOnly())
            setText(""); */
    else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
             && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() & Qt::AltModifier))) {
        e->ignore();
    } else {
        PsiTextView::keyPressEvent(e);
    }
}

/**
 * Copies any selected text to the clipboard
 * if autoCopy is enabled and ChatView is in read-only mode.
 */
void ChatView::autoCopy()
{
    if (isReadOnly() && PsiOptions::instance()->getOption("options.ui.automatically-copy-selected-text").toBool()) {
        copy();
    }
}

/**
 * Handle KeyPress events that happen in ChatEdit widget. This is used
 * to 'fix' the copy shortcut.
 * \param object object that should receive the event
 * \param event received event
 * \param chatEdit pointer to the dialog's ChatEdit widget that receives user input
 */
bool ChatView::handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit)
{
    if (object == chatEdit && event->type() == QEvent::ShortcutOverride
        && static_cast<QKeyEvent *>(event)->matches(QKeySequence::Copy)) {

        if (!chatEdit->textCursor().hasSelection() && this->textCursor().hasSelection()) {
            this->copy();
            return true;
        }
    }

    return false;
}

QString ChatView::formatTimeStamp(const QDateTime &time)
{
    // TODO: provide an option for user to customize
    // time stamp format
    return QString::asprintf("%02d:%02d:%02d", time.time().hour(), time.time().minute(), time.time().second());
}

QString ChatView::colorString(bool local, bool spooled) const
{
    if (spooled) {
        return ColorOpt::instance()->color(informationalColorOpt).name();
    }

    if (local) {
        return ColorOpt::instance()->color("options.ui.look.colors.messages.sent").name();
    }

    return ColorOpt::instance()->color("options.ui.look.colors.messages.received").name();
}

void ChatView::insertText(const QString &text, QTextCursor &insertCursor)
{
    bool doScrollToBottom = atBottom();

    // prevent scrolling back to selected text when
    // restoring selection
    int scrollbarValue = verticalScrollBar()->value();

    if (insertCursor.isNull()) {
        PsiTextView::appendText(text);
    } else {
        PsiTextView::insertText(text, insertCursor);
    }

    if (doScrollToBottom)
        scrollToBottom();
    else
        verticalScrollBar()->setValue(scrollbarValue);
}

void ChatView::appendText(const QString &text)
{
    QTextCursor c;
    insertText(text, c);
}

void ChatView::dispatchMessage(const MessageView &mv)
{
    const QString &replaceId = mv.replaceId();
    if ((mv.type() == MessageView::Message || mv.type() == MessageView::Subject)
        && ChatViewCommon::updateLastMsgTime(mv.dateTime()) && replaceId.isEmpty()) {
        QString color = ColorOpt::instance()->color(informationalColorOpt).name();
        appendText(
            QString(useMessageIcons_ ? "<img src=\"icon:log_icon_time\" />" : "")
            + QString("<font color=\"%1\">*** %2</font>").arg(color, mv.dateTime().date().toString(Qt::ISODate)));
    }

    switch (mv.type()) {
    case MessageView::Message: {
        int         scrollPos      = verticalScrollBar()->value();
        bool        doScrollBottom = atBottom();
        bool        isReplace      = !replaceId.isEmpty();
        QTextCursor cursor         = textCursor(), replaceCursor;
        auto        sel            = PsiRichText::saveSelection(this, cursor);
        cursor.clearSelection();
        setTextCursor(cursor);
        if (isReplace) {
            cursor.setPosition(0);
            replaceCursor = PsiRichText::findMarker(cursor, replaceId + "_" + mv.userId());
            isReplace     = !replaceCursor.isNull(); // marker not found
        }
        if (isReplace) {
            cursor   = replaceCursor;
            auto fin = PsiRichText::findMarker(
                cursor, QString()); // empty marker means end. it's iserted after each regular message
            // the anchor is already on valid position, we only to move the end of selection quite a bit
            if (fin.isNull()) {
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            } else {
                cursor.setPosition(
                    fin.anchor(),
                    QTextCursor::KeepAnchor); // returne cursor is a selection of marker. so we need anchor
            }
            // qDebug("text to remove: %s", qPrintable(cursor.selectedText()));
        } else {
            cursor.movePosition(QTextCursor::End); // no luck with replace, then insert into the end of doc
        }
        PsiRichText::insertMarker(cursor, mv.messageId() + "_" + mv.userId());
        setTextCursor(cursor); // make sure the message is rendered here and nowhere else
        if (isMuc_) {
            renderMucMessage(mv, cursor);
        } else {
            renderMessage(mv, cursor);
        }
        cursor = textCursor(); // take it again. since render function do not modify it
        if (isReplace) {
            QTextImageFormat imageFormat;
            imageFormat.setName("icon:log_icon_corrected");
            imageFormat.setToolTip(tr("The message was corrected"));
            cursor.insertImage(imageFormat);
            // PsiRichText::insertIcon(cursor, QLatin1String("psi/action_templates_edit"), tr("The message was
            // corrected"));
        } else {
            // qDebug("end marker at %d", cursor.position());
            PsiRichText::insertMarker(cursor, QString()); // end marker
        }
        cursor.movePosition(QTextCursor::End); // ensure everything else is inserted into the end
        PsiRichText::restoreSelection(this, cursor, sel);
        setTextCursor(cursor);
        if (doScrollBottom) {
            scrollToBottom();
        } else {
            verticalScrollBar()->setValue(scrollPos);
        }
        break;
    }
    case MessageView::Subject:
        if (isMuc_) {
            renderMucSubject(mv);
        } else {
            renderSubject(mv);
        }
        break;
    case MessageView::Urls:
        renderUrls(mv);
        break;
    case MessageView::MUCJoin:
    case MessageView::MUCPart:
    case MessageView::FileTransferRequest:
    case MessageView::FileTransferFinished:
    default: // System/Status
        renderSysMessage(mv);
    }
}

QString ChatView::replaceMarker(const MessageView &mv) const
{
    return "<a name=\"msgid_" + TextUtil::escape(mv.messageId() + "_" + mv.userId()) + "\"> </a>";
}

void ChatView::renderMucMessage(const MessageView &mv, QTextCursor &insertCursor)
{
    const QString timestr = formatTimeStamp(mv.dateTime());
    QString       alerttagso, alerttagsc, nickcolor;
    QString       textcolor = palette().color(QPalette::Active, QPalette::Text).name();
    QString       icon;
    if (useMessageIcons_) {
        QString sRes;
        if (mv.isLocal()) {
            if (isEncryptionEnabled_)
                sRes = "icon:log_icon_delivered_encrypted";
            else
                sRes = "icon:log_icon_delivered";
        } else {
            if (isEncryptionEnabled_)
                sRes = "icon:log_icon_receive_encrypted";
            else
                sRes = "icon:log_icon_receive";
        }
        icon = QString("<img src=\"%1\" />").arg(sRes);
    }
    if (mv.isAlert()) {
        textcolor
            = PsiOptions::instance()->getOption("options.ui.look.colors.messages.highlighting").value<QColor>().name();
        alerttagso = "<b>";
        alerttagsc = "</b>";
    }

    if (mv.isSpooled() && !PsiOptions::instance()->getOption("options.ui.muc.colored-history").toBool()) {
        nickcolor = ColorOpt::instance()->color(informationalColorOpt).name();
    } else {
        nickcolor = getMucNickColor(mv.nick(), mv.isLocal());
    }
    QString nick = QString("<a href=\"addnick://psi/") + QUrl::toPercentEncoding(mv.nick())
        + "\" style=\"color: " + nickcolor + "; text-decoration: none; \">" + TextUtil::escape(mv.nick()) + "</a>";

    QString inner = alerttagso + mv.formattedText() + replaceMarker(mv) + alerttagsc;

    if (mv.isEmote()) {
        insertText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1]").arg(timestr)
                       + QString(" *%1 ").arg(nick) + inner + "</font>",
                   insertCursor);
    } else {
        if (PsiOptions::instance()->getOption("options.ui.chat.use-chat-says-style").toBool()) {
            insertText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] ").arg(timestr)
                           + QString("%1 says:").arg(nick) + "</font><br>"
                           + QString("<font color=\"%1\">").arg(textcolor) + inner + "</font>",
                       insertCursor);
        } else {
            insertText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] &lt;").arg(timestr) + nick
                           + QString("&gt;</font> ") + QString("<font color=\"%1\">").arg(textcolor) + inner
                           + "</font>",
                       insertCursor);
        }
    }

    if (mv.isLocal() && PsiOptions::instance()->getOption("options.ui.chat.auto-scroll-to-bottom").toBool()) {
        scrollToBottom();
    }
}

void ChatView::renderMessage(const MessageView &mv, QTextCursor &insertCursor)
{
    QString timestr = formatTimeStamp(mv.dateTime());
    QString color   = colorString(mv.isLocal(), false);
    if (useMessageIcons_ && mv.isAwaitingReceipt()) {
        auto sendIcon = document()->resource(
            QTextDocument::ImageResource,
            QUrl(QLatin1String("icon:") + (isEncryptionEnabled_ ? "log_icon_send_encrypted" : "log_icon_send")));
        document()->addResource(QTextDocument::ImageResource, QUrl(QString("icon:delivery") + mv.messageId()),
                                sendIcon);
    }
    QString icon;
    if (useMessageIcons_) {
        QString sRes;
        if (mv.isSpooled())
            sRes = "icon:log_icon_history";
        else if (mv.isLocal()) {
            if (mv.isAwaitingReceipt())
                sRes = QString("icon:delivery") + mv.messageId();
            else if (isEncryptionEnabled_)
                sRes = "icon:log_icon_receive_encrypted";
            else
                sRes = "icon:log_icon_send";
        } else {
            if (isEncryptionEnabled_)
                sRes = "icon:log_icon_receive_encrypted";
            else
                sRes = "icon:log_icon_receive";
        }
        icon = QString("<img src=\"%1\" />").arg(sRes);
    }
    QString str;

    QString inner = mv.formattedText() + replaceMarker(mv);
    if (mv.isEmote()) {
        str = icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1]").arg(timestr)
            + QString(" *%1 ").arg(TextUtil::escape(mv.nick())) + inner + "</span>";
    } else {
        if (PsiOptions::instance()->getOption("options.ui.chat.use-chat-says-style").toBool()) {
            str = icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1] ").arg(timestr)
                + tr("%1 says:").arg(TextUtil::escape(mv.nick())) + "</span><br>";
        } else {
            str = icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1] &lt;").arg(timestr)
                + TextUtil::escape(mv.nick()) + QString("&gt;</span> ");
        }
        if (mv.isSpooled())
            str.append(QString("<span style=\"color: %1\">%2</span>")
                           .arg(ColorOpt::instance()->color("options.ui.look.colors.messages.usertext").name(), inner));
        else
            str.append(inner);
    }
    insertText(str, insertCursor);

    if (mv.isLocal() && PsiOptions::instance()->getOption("options.ui.chat.auto-scroll-to-bottom").toBool()) {
        deferredScroll();
    }
}

void ChatView::renderSysMessage(const MessageView &mv)
{
    QString timestr = formatTimeStamp(mv.dateTime());
    QString ut      = mv.formattedUserText();

    if ((mv.type() == MessageView::MUCJoin || mv.type() == MessageView::MUCPart) && mv.isJoinLeaveHidden()) {
        return; // not necessary here. maybe for other chatviews
    }

    if (mv.type() == MessageView::Status && mv.isStatusChangeHidden()) {
        return;
    }

    if (mv.type() == MessageView::MUCJoin && mv.isStatusChangeHidden()) {
        ut.clear();
    }

    bool isPriority = false;
    if (isMuc_) {
        isPriority = mv.type() == MessageView::Status
            && PsiOptions::instance()->getOption("options.ui.muc.status-with-priority").toBool() && mv.statusPriority();
    } else {
        isPriority = mv.type() == MessageView::Status
            && PsiOptions::instance()->getOption("options.ui.chat.status-with-priority").toBool()
            && mv.statusPriority();
    }

    QString color         = ColorOpt::instance()->color(informationalColorOpt).name();
    QString userTextColor = ColorOpt::instance()->color("options.ui.look.colors.messages.usertext").name();
    appendText(QString(useMessageIcons_ ? "<img src=\"icon:log_icon_info\" />" : "")
               + QString("<font color=\"%1\">[%2] *** ").arg(color, timestr) + mv.formattedText()
               + (ut.isEmpty() ? "" : QString(": <span style=\"color:%1;\">%2</span>").arg(userTextColor, ut))
               + (isPriority ? QString(" [%1]").arg(mv.statusPriority()) : "") + "</font>");
}

void ChatView::renderSubject(const MessageView &mv)
{
    appendText(QString(useMessageIcons_ ? "<img src=\"icon:log_icon_info\" />" : "") + "<b>" + tr("Subject:") + "</b> "
               + QString("%1").arg(mv.formattedUserText()));
}

void ChatView::renderMucSubject(const MessageView &mv)
{
    QString timestr       = formatTimeStamp(mv.dateTime());
    QString ut            = mv.formattedUserText();
    QString color         = ColorOpt::instance()->color(informationalColorOpt).name();
    QString userTextColor = ColorOpt::instance()->color("options.ui.look.colors.messages.usertext").name();
    appendText(
        QString(useMessageIcons_ ? "<img src=\"icon:log_icon_info\" />" : "")
        + QString("<font color=\"%1\">[%2] *** ").arg(color, timestr) + mv.formattedText()
        + (ut.isEmpty() ? "" : ":<br>") + "</font>"
        + (ut.isEmpty() ? "" : QString(" <span style=\"color:%1;font-weight:bold\">%2</span>").arg(userTextColor, ut)));
}

void ChatView::renderUrls(const MessageView &mv)
{
    QMap<QString, QString> urls = mv.urls();
    for (const QString &key : urls.keys()) {
        appendText(QString("<b>") + tr("URL:") + "</b> " + QString("%1").arg(TextUtil::linkify(TextUtil::escape(key))));
        if (!urls.value(key).isEmpty()) {
            appendText(QString("<b>") + tr("Desc:") + "</b> " + QString("%1").arg(urls.value(key)));
        }
    }
}

void ChatView::slotScroll() { scrollToBottom(); }

void ChatView::deferredScroll() { QTimer::singleShot(250, this, SLOT(slotScroll())); }

void ChatView::scrollUp()
{
    verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->pageStep() / 2);
}

void ChatView::scrollDown()
{
    verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->pageStep() / 2);
}

void ChatView::updateAvatar(const XMPP::Jid &jid, ChatViewCommon::UserType utype)
{
    Q_UNUSED(jid)
    Q_UNUSED(utype)
}

void ChatView::doTrackBar()
{
    // save position, because our manipulations could change it
    int scrollbarValue = verticalScrollBar()->value();

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    PsiRichText::Selection selection = PsiRichText::saveSelection(this, cursor);

    // removeTrackBar(cursor);
    if (oldTrackBarPosition) {
        cursor.setPosition(oldTrackBarPosition, QTextCursor::KeepAnchor);
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
        cursor.clearSelection();
        cursor.setBlockFormat(blockFormat);
    }

    // addTrackBar(cursor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    oldTrackBarPosition          = cursor.position();
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QVariant(true));
    cursor.clearSelection();
    cursor.setBlockFormat(blockFormat);

    PsiRichText::restoreSelection(this, cursor, selection);
    cursor.endEditBlock();
    setTextCursor(cursor);

    verticalScrollBar()->setValue(scrollbarValue);
}

ChatView *ChatView::textWidget() { return this; }

QWidget *ChatView::realTextWidget()
{
    QWidget *child = childAt(frameRect().center());
    if (child)
        return child;
    return this;
}
