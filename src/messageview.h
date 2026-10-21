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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H
#include "chatreply.h"

#include "filesharingitem.h"
#include "iris/xmpp_message.h"

#include <QDateTime>
#include <QVariantMap>
#include <QSharedDataPointer>

class MessageView {
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
        FileTransferFinished,
        Reactions,
        MessageRetraction,
        DeliveryReceipt,
        DisplayedMarker,
        MessageSecurity // Outgoing message: whether it went out encrypted.
    };

    enum Flag {
        Emote            = 0x1,
        Alert            = 0x2,
        Local            = 0x4,
        Spooled          = 0x8,
        AwaitingReceipt  = 0x10,
        HideStatusChange = 0x20,
        HideJoinLeave    = 0x40,
        Edited           = 0x80,
        SecurityKnown    = 0x100,
        Encrypted        = 0x200,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    MessageView(Type);

    static MessageView fromPlainText(const QString &, Type);
    static MessageView fromHtml(const QString &, Type);

    static MessageView systemMessage(const QString &message) { return fromPlainText(message, System); }
    static MessageView urlsMessage(const QMap<QString, QString> &);
    static MessageView subjectMessage(const QString &subject, const QString &prefix = QString());

    // accepts plain/text nick, plain/text status and rich/text statusText
    static MessageView statusMessage(const QString &nick, int status, const QString &statusText = QString(),
                                     int priority = 0);

    // accepts plain/text nick, plain/text status and rich/text statusText
    static MessageView mucJoinMessage(const QString &nick, int status, const QString &message,
                                      const QString &statusText = QString(), int priority = 0);
    static MessageView mucPartMessage(const QString &nick, const QString &message = QString(),
                                      const QString &statusText = QString());
    static MessageView nickChangeMessage(const QString &nick, const QString &newNick);
    static MessageView reactionsMessage(const QString &nick, const QString &targetMessageId,
                                        const QSet<QString> &reactions);
    static MessageView retractionMessage(const QString &targetMessageId);
    // Protocol events update an existing row; they never create a text row.
    static MessageView receiptMessage(const QString &targetMessageId)
    {
        MessageView mv(DeliveryReceipt);
        mv.setMessageId(targetMessageId);
        return mv;
    }
    static MessageView displayedMarkerMessage(const QString &targetMessageId)
    {
        MessageView mv(DisplayedMarker);
        mv.setMessageId(targetMessageId);
        return mv;
    }
    static bool    protocolMessage(const XMPP::Message &, bool local, MessageView &);
    static QString reactionTarget(const XMPP::Message &);
    static QString retractionTarget(const XMPP::Message &);
    void setReplyMetadata(const XMPP::Message &);
    const ChatReply &reply() const;
    void setReply(const ChatReply &);
    const QString &replyFormattedText() const;
    bool hasReplyFallback() const;

    inline Type           type() const { return _type; }
    inline const QString &text() const { return _text; }
    inline void           setText(const QString &text) { _text = text; }
    inline const QString &userText() const { return _userText; }
    inline void           setUserText(const QString &text) { _userText = text; }

    void    setPlainText(const QString &);
    void    setHtml(const QString &);
    QString formattedText() const;
    QString formattedUserText() const;
    bool    hasStatus() const;

    inline const Flags &flags() const { return _flags; }

    inline void setAlert(bool state = true) { _flags.setFlag(Alert, state); }
    inline bool isAlert() const { return bool(_flags & Alert); }
    inline void setLocal(bool state = true) { _flags.setFlag(Local, state); }
    inline bool isLocal() const { return bool(_flags & Local); }
    inline void setEmote(bool state = true) { _flags.setFlag(Emote, state); }
    inline bool isEmote() const { return bool(_flags & Emote); }
    inline void setSpooled(bool state = true) { _flags.setFlag(Spooled, state); }
    inline bool isSpooled() const { return bool(_flags & Spooled); }
    inline void setEdited(bool edited = true) { _flags.setFlag(Edited, edited); }
    // Whether the message travelled encrypted. Unknown until set: a freshly
    // sent message learns it only when its encryption job finishes.
    inline void setEncrypted(bool encrypted, const QString &protocol = {})
    {
        _flags.setFlag(SecurityKnown);
        _flags.setFlag(Encrypted, encrypted);
        _encryptionProtocol = encrypted ? protocol : QString();
    }
    inline bool           isSecurityKnown() const { return bool(_flags & SecurityKnown); }
    inline bool           isEncrypted() const { return bool(_flags & Encrypted); }
    inline const QString &encryptionProtocol() const { return _encryptionProtocol; }
    static MessageView    securityMessage(const QString &messageId, bool encrypted, const QString &protocol)
    {
        MessageView mv(MessageSecurity);
        mv.setMessageId(messageId);
        mv.setEncrypted(encrypted, protocol);
        return mv;
    }
    inline bool isEdited() const { return bool(_flags & Edited); }
    inline void setAwaitingReceipt(bool b = true) { _flags.setFlag(AwaitingReceipt, b); }
    inline bool isAwaitingReceipt() const { return bool(_flags & AwaitingReceipt); }
    inline void setMarkable(bool markable = true) { _markable = markable; }
    inline bool isMarkable() const { return _markable; }
    inline void setStatusChangeHidden(bool b = true) { _flags.setFlag(HideStatusChange, b); }
    inline bool isStatusChangeHidden() const { return bool(_flags & HideStatusChange); }
    inline void setJoinLeaveHidden(bool b = true) { _flags.setFlag(HideJoinLeave, b); }
    inline bool isJoinLeaveHidden() const { return bool(_flags & HideJoinLeave); }

    inline void setStatus(int s) { _status = s; }
    inline int  status() const { return _status; }
    inline void setStatusPriority(int s) { _statusPriority = s; }
    inline int  statusPriority() const { return _statusPriority; }

    inline void                            setNick(const QString &nick) { _nick = nick; }
    inline const QString                  &nick() const { return _nick; }
    inline void                            setMessageId(const QString &id) { _messageId = id; }
    inline const QString                  &messageId() const { return _messageId; }
    // Verified archive UID from a MAM result; never substitute for stanza id.
    inline void                            setArchiveId(const QString &id) { _archiveId = id; }
    inline const QString                  &archiveId() const { return _archiveId; }
    void setHistoryId(qint64 id) { _historyId = id; }
    qint64 historyId() const { return _historyId; }
    void                                   setRevisionTime(const QDateTime &time) { _revisionTime = time; }
    QDateTime revisionTime() const { return _revisionTime.isValid() ? _revisionTime : _dateTime; }
    void      setReactionTargetId(const QString &id) { _reactionTargetId = id; }
    QString   reactionTargetId() const { return _reactionTargetId; }
    void      setRetractionTargetId(const QString &id) { _retractionTargetId = id; }
    QString   retractionTargetId() const { return _retractionTargetId; }
    inline void                            setUserId(const QString &id) { _userId = id; }
    inline const QString                  &userId() const { return _userId; }
    inline void                            setDateTime(const QDateTime &dt) { _dateTime = dt; }
    inline const QDateTime                &dateTime() const { return _dateTime; }
    inline QMap<QString, QString>          urls() const { return _urls; }
    inline void                            setReplaceId(const QString &id)
    {
        _replaceId = id;
        if (!id.isEmpty())
            setEdited();
    }
    inline const QString                  &replaceId() const { return _replaceId; }
    inline void                            setQuoteId(const QString &id) { _quoteId = id; }
    inline const QString                  &quoteId() const { return _quoteId; }
    inline void                            setRetractionId(const QString &id) { _retractionId = id; }
    inline const QString                  &retractionId() const { return _retractionId; }
    inline void                            addReference(FileSharingItem *fsi) { _references.append(fsi); }
    inline const QList<FileSharingItem *> &references() const { return _references; }
    inline void                            setReactionsId(const QString &id) { _reactionsId = id; }
    inline const QString                  &reactionsId() const { return _reactionsId; }
    inline void                            setReactions(const QSet<QString> &r) { _reactions = r; }
    inline const QSet<QString>            &reactions() const { return _reactions; }

private:
    Type                     _type;
    Flags                    _flags;
    int                      _status;
    int                      _statusPriority;
    QString                  _messageId;
    QString                  _archiveId;
    qint64                   _historyId = 0;
    QString                  _userId;   // TODO: convert to XMPP::Jid, only used in message corrections as of now
    QString                  _nick;     // rich / as is
    QString                  _text;     // always rich (plain text converted to rich)
    QString                  _userText; // rich
    QDateTime                _dateTime;
    QDateTime                _revisionTime;
    QString                  _reactionTargetId;
    QString                  _retractionTargetId;
    QMap<QString, QString>   _urls;
    QString                  _replaceId;
    QString                  _quoteId;
    QString                  _retractionId;
    QString                  _reactionsId;
    QList<FileSharingItem *> _references;
    QSet<QString>            _reactions;
    // Ordinary history rows pay only for a null pointer. Reply presentation
    // detaches on mutation, like the other implicitly shared Qt values here.
    struct ReplyData : QSharedData {
        ChatReply reply;
        QString formatted;
        bool hasFallback = false;
    };
    QSharedDataPointer<ReplyData> _replyData;
    bool                     _markable = false;
    QString                  _encryptionProtocol;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MessageView::Flags)

#endif // MESSAGEVIEW_H
