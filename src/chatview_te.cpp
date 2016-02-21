/*
 * chatview_te.cpp - subclass of PsiTextView to handle various hotkeys
 * Copyright (C) 2001-2010  Justin Karneges, Michail Pishchagin, Rion
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "chatview_te.h"

#include "msgmle.h"
#include "messageview.h"
#include "psioptions.h"
#include "coloropt.h"
#include "textutil.h"
#include "psirichtext.h"
#include "common.h"
#include "iconset.h"

#include <QWidget>
#include <QTextOption>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QUrl>

static const char *informationalColorOpt = "options.ui.look.colors.messages.informational";

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent)
	: PsiTextView(parent)
	, isMuc_(false)
	, oldTrackBarPosition(0)
	, dialog_(0)
{
	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

	setReadOnly(true);
	setUndoRedoEnabled(false);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setLooks(this);

#ifndef HAVE_X11	// linux has this feature built-in
	connect(this, SIGNAL(selectionChanged()), SLOT(autoCopy()));
	connect(this, SIGNAL(cursorPositionChanged()), SLOT(autoCopy()));
#endif

	useMessageIcons_ = PsiOptions::instance()->getOption("options.ui.chat.use-message-icons").toBool();
	if (useMessageIcons_) {
		int logIconsSize = fontInfo().pixelSize()*0.93;
		if (PsiOptions::instance()->getOption("options.ui.chat.scaled-message-icons").toBool()) {
			logIconReceive = IconsetFactory::iconPixmap("psi/notification_chat_receive").scaledToHeight(logIconsSize, Qt::SmoothTransformation);
			logIconSend = IconsetFactory::iconPixmap("psi/notification_chat_send").scaledToHeight(logIconsSize, Qt::SmoothTransformation);
			logIconDelivered = IconsetFactory::iconPixmap("psi/notification_chat_delivery_ok").scaledToHeight(logIconsSize, Qt::SmoothTransformation);
			logIconTime = IconsetFactory::iconPixmap("psi/notification_chat_time").scaledToHeight(logIconsSize, Qt::SmoothTransformation);
			logIconInfo = IconsetFactory::iconPixmap("psi/notification_chat_info").scaledToHeight(logIconsSize, Qt::SmoothTransformation);
		} else {
			logIconReceive = IconsetFactory::iconPixmap("psi/notification_chat_receive");
			logIconSend = IconsetFactory::iconPixmap("psi/notification_chat_send");
			logIconDelivered = IconsetFactory::iconPixmap("psi/notification_chat_delivery_ok");
			logIconTime = IconsetFactory::iconPixmap("psi/notification_chat_time");
			logIconInfo = IconsetFactory::iconPixmap("psi/notification_chat_info");
		}
		addLogIconsResources();
	}

}

ChatView::~ChatView()
{

}

// something after we know isMuc and dialog is set
void ChatView::init()
{

}

QSize ChatView::sizeHint() const
{
	return minimumSizeHint();
}

void ChatView::setDialog(QWidget* dialog)
{
	dialog_ = dialog;
}

void ChatView::setSessionData(bool isMuc, const QString &jid, const QString name)
{
	isMuc_ = isMuc;
	jid_ = jid;
	name_ = name;
}

void ChatView::clear()
{
	PsiTextView::clear();
	addLogIconsResources();
}

void ChatView::contextMenuEvent(QContextMenuEvent *e)
{
	const QUrl anc = QUrl::fromEncoded(anchorAt(e->pos()).toLatin1());

	if ( anc.scheme() == "addnick" ) {
		showNM(anc.path().mid(1));
		e->accept();
	}
	else {
		PsiTextView::contextMenuEvent(e);
	}
}

void ChatView::addLogIconsResources()
{
	document()->addResource(QTextDocument::ImageResource, QUrl("icon:log_icon_receive"), logIconReceive);
	document()->addResource(QTextDocument::ImageResource, QUrl("icon:log_icon_send"), logIconSend);
	document()->addResource(QTextDocument::ImageResource, QUrl("icon:log_icon_time"), logIconTime);
	document()->addResource(QTextDocument::ImageResource, QUrl("icon:log_icon_info"), logIconInfo);
	document()->addResource(QTextDocument::ImageResource, QUrl("icon:log_icon_delivered"), logIconDelivered);
}

void ChatView::markReceived(QString id)
{
	if (useMessageIcons_) {
		document()->addResource(QTextDocument::ImageResource, QUrl(QString("icon:delivery") + id), logIconDelivered);
		setLineWrapColumnOrWidth(lineWrapColumnOrWidth());
	}
}

bool ChatView::focusNextPrevChild(bool next)
{
	return QWidget::focusNextPrevChild(next);
}

void ChatView::keyPressEvent(QKeyEvent *e)
{
/*	if(e->key() == Qt::Key_Escape)
		e->ignore();
#ifdef Q_OS_MAC
	else if(e->key() == Qt::Key_W && e->modifiers() & Qt::ControlModifier)
		e->ignore();
	else
#endif
	else if(e->key() == Qt::Key_Return && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() & Qt::AltModifier)) )
		e->ignore();
	else if(e->key() == Qt::Key_H && (e->modifiers() & Qt::ControlModifier))
		e->ignore();
	else if(e->key() == Qt::Key_I && (e->modifiers() & Qt::ControlModifier))
		e->ignore(); */
	/*else*/ if(e->key() == Qt::Key_M && (e->modifiers() & Qt::ControlModifier) && !isReadOnly()) // newline
		append("\n");
/*	else if(e->key() == Qt::Key_U && (e->modifiers() & Qt::ControlModifier) && !isReadOnly())
		setText(""); */
	else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && ((e->modifiers() & Qt::ControlModifier) || (e->modifiers() & Qt::AltModifier))) {
		e->ignore();
	}
	else {
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
	if (object == chatEdit && event->type() == QEvent::ShortcutOverride &&
		((QKeyEvent*)event)->matches(QKeySequence::Copy)) {

		if (!chatEdit->textCursor().hasSelection() &&
			this->textCursor().hasSelection()) {
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
	return QString().sprintf("%02d:%02d:%02d", time.time().hour(), time.time().minute(), time.time().second());;
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

void ChatView::appendText(const QString &text)
{
	bool doScrollToBottom = atBottom();

	// prevent scrolling back to selected text when
	// restoring selection
	int scrollbarValue = verticalScrollBar()->value();

	PsiTextView::appendText(text);

	if (doScrollToBottom)
		scrollToBottom();
	else
		verticalScrollBar()->setValue(scrollbarValue);
}

void ChatView::dispatchMessage(const MessageView &mv)
{
	if ((mv.type() == MessageView::Message || mv.type() == MessageView::Subject)
			&& ChatViewCommon::updateLastMsgTime(mv.dateTime()))
	{
		QString color = ColorOpt::instance()->color(informationalColorOpt).name();
		appendText(QString(useMessageIcons_?"<img src=\"icon:log_icon_time\" />":"") +
				   QString("<font color=\"%1\">*** %2</font>").arg(color).arg(mv.dateTime().date().toString(Qt::ISODate)));
	}
	switch (mv.type()) {
		case MessageView::Message:
			if (isMuc_) {
				renderMucMessage(mv);
			} else {
				renderMessage(mv);
			}
			break;
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
		default: // System/Status
			renderSysMessage(mv);
	}
}

void ChatView::renderMucMessage(const MessageView &mv)
{
	const QString timestr = formatTimeStamp(mv.dateTime());
	QString alerttagso, alerttagsc, nickcolor;
	QString textcolor = palette().color(QPalette::Active, QPalette::Text).name();
	QString icon = useMessageIcons_?
					(QString("<img src=\"%1\" />").arg(mv.isLocal()?"icon:log_icon_delivered":"icon:log_icon_receive")):"";
	if(mv.isAlert()) {
		textcolor = PsiOptions::instance()->getOption("options.ui.look.colors.messages.highlighting").value<QColor>().name();
		alerttagso = "<b>";
		alerttagsc = "</b>";
	}

	if(mv.isSpooled() && !PsiOptions::instance()->getOption("options.ui.muc.colored-history").toBool()) {
		nickcolor = ColorOpt::instance()->color(informationalColorOpt).name();
	} else {
		nickcolor = getMucNickColor(mv.nick(), mv.isLocal());
	}
	QString nick = QString("<a href=\"addnick://psi/") + QUrl::toPercentEncoding(mv.nick()) +
				   "\" style=\"color: "+nickcolor+"; text-decoration: none; \">"+TextUtil::escape(mv.nick())+"</a>";

	if(mv.isEmote()) {
		appendText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(nick) + alerttagso + mv.formattedText() + alerttagsc + "</font>");
	}
	else {
		if(PsiOptions::instance()->getOption("options.ui.chat.use-chat-says-style").toBool()) {
			appendText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] ").arg(timestr) + QString("%1 says:").arg(nick) + "</font><br>" + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + mv.formattedText() + alerttagsc + "</font>");
		}
		else {
			appendText(icon + QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] &lt;").arg(timestr) + nick + QString("&gt;</font> ") + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + mv.formattedText() + alerttagsc +"</font>");
		}
	}

	if(mv.isLocal()) {
		scrollToBottom();
	}
}

void ChatView::renderMessage(const MessageView &mv)
{
	QString timestr = formatTimeStamp(mv.dateTime());
	QString color = colorString(mv.isLocal(), mv.isSpooled());
	if (useMessageIcons_ && mv.isAwaitingReceipt()) {
		document()->addResource(QTextDocument::ImageResource, QUrl(QString("icon:delivery") + mv.messageId()), logIconSend);
	}
	QString icon = useMessageIcons_?
		(QString("<img src=\"%1\" />").arg(mv.isLocal()?
		(mv.isAwaitingReceipt()?QString("icon:delivery") + mv.messageId():"icon:log_icon_send"):"icon:log_icon_receive")):"";
	if (mv.isEmote()) {
		appendText(icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(TextUtil::escape(mv.nick())) + mv.formattedText() + "</span>");
	} else {
		if (PsiOptions::instance()->getOption("options.ui.chat.use-chat-says-style").toBool()) {
			appendText(icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1] ").arg(timestr) + tr("%1 says:").arg(TextUtil::escape(mv.nick())) + "</span><br>" + mv.formattedText());
		}
		else {
			appendText(icon + QString("<span style=\"color: %1\">").arg(color) + QString("[%1] &lt;").arg(timestr) + TextUtil::escape(mv.nick()) + QString("&gt;</span> ") + mv.formattedText());
		}
	}

	if (mv.isLocal()) {
		deferredScroll();
	}
}

void ChatView::renderSysMessage(const MessageView &mv)
{
	QString timestr = formatTimeStamp(mv.dateTime());
	QString ut = mv.formattedUserText();
	QString color = ColorOpt::instance()->color(informationalColorOpt).name();
	QString userTextColor = ColorOpt::instance()->color("options.ui.look.colors.messages.usertext").name();
	appendText(QString(useMessageIcons_?"<img src=\"icon:log_icon_info\" />":"") +
			   QString("<font color=\"%1\">[%2] *** ").arg(color, timestr) +
			   mv.formattedText() +
			   (ut.isEmpty()?"":QString(": <span style=\"color:%1;\">%2</span>")
									  .arg(userTextColor, ut)) +
			   (mv.type() == MessageView::Status && mv.statusPriority() ?
					QString(" [%1]").arg(mv.statusPriority()) :  "") +
			   "</font>");
}

void ChatView::renderSubject(const MessageView &mv)
{
	appendText(QString(useMessageIcons_?"<img src=\"icon:log_icon_info\" />":"") + "<b>" + tr("Subject:") + "</b> " + QString("%1").arg(mv.formattedUserText()));
}

void ChatView::renderMucSubject(const MessageView &mv)
{
	QString timestr = formatTimeStamp(mv.dateTime());
	QString ut = mv.formattedUserText();
	QString color = ColorOpt::instance()->color(informationalColorOpt).name();
	QString userTextColor = ColorOpt::instance()->color("options.ui.look.colors.messages.usertext").name();
	appendText(QString(useMessageIcons_?"<img src=\"icon:log_icon_info\" />":"") +
			   QString("<font color=\"%1\">[%2] *** ").arg(color, timestr) + mv.formattedText() +
						(ut.isEmpty()?"":":<br>") + "</font>" +
						(ut.isEmpty()?"":QString(" <span style=\"color:%1;font-weight:bold\">%2</span>").arg(userTextColor, ut)));
}

void ChatView::renderUrls(const MessageView &mv)
{
	QMap<QString, QString> urls = mv.urls();
	foreach (const QString &key, urls.keys()) {
		appendText(QString("<b>") + tr("URL:") + "</b> " + QString("%1").arg(TextUtil::linkify(TextUtil::escape(key))));
		if (!urls.value(key).isEmpty()) {
			appendText(QString("<b>") + tr("Desc:") + "</b> " + QString("%1").arg(urls.value(key)));
		}
	}
}

void ChatView::slotScroll() {
	scrollToBottom();
}

void ChatView::deferredScroll()
{
	QTimer::singleShot(250, this, SLOT(slotScroll()));
}

void ChatView::scrollUp()
{
	verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->pageStep() / 2);
}

void ChatView::scrollDown()
{
	verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->pageStep() / 2);
}

void ChatView::doTrackBar()
{
	// save position, because our manipulations could change it
	int scrollbarValue = verticalScrollBar()->value();

	QTextCursor cursor = textCursor();
	cursor.beginEditBlock();
	PsiRichText::Selection selection = PsiRichText::saveSelection(this, cursor);

	//removeTrackBar(cursor);
	if (oldTrackBarPosition) {
		cursor.setPosition(oldTrackBarPosition, QTextCursor::KeepAnchor);
		QTextBlockFormat blockFormat = cursor.blockFormat();
		blockFormat.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
		cursor.clearSelection();
		cursor.setBlockFormat(blockFormat);
	}

	//addTrackBar(cursor);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	oldTrackBarPosition = cursor.position();
	QTextBlockFormat blockFormat = cursor.blockFormat();
	blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QVariant(true));
	cursor.clearSelection();
	cursor.setBlockFormat(blockFormat);

	PsiRichText::restoreSelection(this, cursor, selection);
	cursor.endEditBlock();
	setTextCursor(cursor);

	verticalScrollBar()->setValue(scrollbarValue);
}

bool ChatView::internalFind(QString str, bool startFromBeginning)
{
	if (startFromBeginning) {
		QTextCursor cursor = textCursor();
		cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
		cursor.clearSelection();
		setTextCursor(cursor);
	}

	bool found = find(str);
	if(!found) {
		if (!startFromBeginning)
			return internalFind(str, true);

		return false;
	}

	return true;
}

ChatView * ChatView::textWidget()
{
	return this;
}
