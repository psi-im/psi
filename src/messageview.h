/*
 * messageview.h - message data for chatview
 * Copyright (C) 2010-2017  Sergey Ilinykh
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

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <QDateTime>
#include <QVariantMap>

#if QT_VERSION < QT_VERSION_CHECK(5,7,0)
# define SET_QFLAG(flags, flag, state) if (state) flags |= flag; else flags &= ~flag
#else
# define SET_QFLAG(flags, flag, state) flags.setFlag(flag, state)
#endif

class MessageView
{
public:
	enum Type {
		Message,
		System,
		Status,
		Subject,
		Urls,
		MUCJoin,
		MUCPart,
		NickChange,
		FileTransferRequest,
		FileTransferFinished
	};

	enum Flag {
		Emote   = 0x1,
		Alert   = 0x2,
		Local   = 0x4,
		Spooled = 0x8,
		AwaitingReceipt  = 0x10,
		HideStatusChange = 0x20,
		HideJoinLeave    = 0x40,
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	MessageView(Type);

	static MessageView fromPlainText(const QString &, Type);
	static MessageView fromHtml(const QString &, Type);

	static MessageView systemMessage(const QString &message) { return fromPlainText(message, System); }
	static MessageView urlsMessage(const QMap<QString, QString> &);
	static MessageView subjectMessage(const QString &subject,
									  const QString &prefix = QString());

	// accepts plain/text nick, plain/text status and rich/text statusText
	static MessageView statusMessage(const QString &nick, int status,
									 const QString &statusText = QString(), int priority = 0);

	// accepts plain/text nick, plain/text status and rich/text statusText
	static MessageView mucJoinMessage(const QString &nick, int status, const QString &message,
									 const QString &statusText = QString(), int priority = 0);
	static MessageView mucPartMessage(const QString &nick, const QString &message = QString(),
									 const QString &statusText = QString());
	static MessageView nickChangeMessage(const QString &nick, const QString &newNick);

	inline Type type() const { return _type; }
	inline const QString &text() const { return _text; }
	inline void setText(const QString &text) { _text = text; }
	inline const QString &userText() const { return _userText; }
	inline void setUserText(const QString &text) { _userText = text; }

	void setPlainText(const QString &);
	void setHtml(const QString &);
	QString formattedText() const;
	QString formattedUserText() const;
	bool hasStatus() const;

	inline const Flags &flags() const { return _flags; }

	inline void setAlert(bool state = true) { SET_QFLAG(_flags, Alert, state); }
	inline bool isAlert() const { return _flags & Alert; }
	inline void setLocal(bool state = true) { SET_QFLAG(_flags, Local, state); }
	inline bool isLocal() const { return _flags & Local; }
	inline void setEmote(bool state = true) { SET_QFLAG(_flags, Emote, state); }
	inline bool isEmote() const { return _flags & Emote; }
	inline void setSpooled(bool state = true) { SET_QFLAG(_flags, Spooled, state); }
	inline bool isSpooled() const { return _flags & Spooled; }
	inline void setAwaitingReceipt(bool b = true) { SET_QFLAG(_flags, AwaitingReceipt, b); }
	inline bool isAwaitingReceipt() const { return _flags & AwaitingReceipt; }
	inline void setStatusChangeHidden(bool b = true) { SET_QFLAG(_flags, HideStatusChange, b); }
	inline bool isStatusChangeHidden() const { return _flags & HideStatusChange; }
	inline void setJoinLeaveHidden(bool b = true) { SET_QFLAG(_flags, HideJoinLeave, b); }
	inline bool isJoinLeaveHidden() const { return _flags & HideJoinLeave; }

	inline void setStatus(int s) { _status = s; }
	inline int status() const { return _status; }
	inline void setStatusPriority(int s) { _statusPriority = s; }
	inline int statusPriority() const { return _statusPriority; }

	inline void setNick(const QString &nick) { _nick = nick; }
	inline const QString &nick() const { return _nick; }
	inline void setMessageId(const QString &id) { _messageId = id; }
	inline const QString &messageId() const { return _messageId; }
	inline void setUserId(const QString &id) { _userId = id; }
	inline const QString &userId() const { return _userId; }
	inline void setDateTime(const QDateTime &dt) { _dateTime = dt; }
	inline const QDateTime &dateTime() const { return _dateTime; }
	inline QMap<QString, QString> urls() const { return _urls; }
	inline void setReplaceId(const QString &id) { _replaceId = id; }
	inline const QString &replaceId() const { return _replaceId; }

	QVariantMap toVariantMap(bool isMuc, bool formatted = false) const;

private:
	Type _type;
	Flags _flags;
	int _status;
	int _statusPriority;
	QString _messageId;
	QString _userId; // TODO: convert to XMPP::Jid, only used in message corrections as of now
	QString _nick; // rich / as is
	QString _text; // always rich (plain text converted to rich)
	QString _userText; // rich
	QDateTime _dateTime;
	QMap<QString, QString> _urls;
	QString _replaceId;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MessageView::Flags)

#endif
