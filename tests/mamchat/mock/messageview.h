/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

// Test-only MessageView contract. Keeps this widget test independent from
// PsiAccount, PsiOptions and the full application while compiling the actual
// MamChatWidget.cpp, LongListWidget.cpp and their real Qt rows.
#include <QDateTime>
#include <QSet>
#include <QString>

class MessageView {
public:
    enum Type {
        Message,
        System,
        Reactions,
        MessageRetraction,
        MUCJoin,
        MUCPart,
        Status,
        Subject,
        Urls,
        NickChange,
        FileTransferRequest,
        FileTransferFinished,
        DeliveryReceipt,
        DisplayedMarker,
        MessageSecurity
    };
    explicit MessageView(Type type = Message) : type_(type) { }

    bool           isJoinLeaveHidden() const { return false; }
    bool           isStatusChangeHidden() const { return false; }
    Type           type() const { return type_; }
    const QString &messageId() const { return id_; }
    void           setMessageId(const QString &id) { id_ = id; }
    void           setMarkable(bool value = true) { markable_ = value; }
    bool           isMarkable() const { return markable_; }
    void           setHistoryId(qint64 id) { historyId_ = id; }
    qint64         historyId() const { return historyId_; }
    void           setRevisionTime(const QDateTime &time) { revisionTime_ = time; }
    QDateTime      revisionTime() const { return revisionTime_.isValid() ? revisionTime_ : when_; }
    void           setReactionTargetId(const QString &id) { target_ = id; }
    QString        reactionTargetId() const { return target_; }
    void           setRetractionTargetId(const QString &id) { retractTarget_ = id; }
    QString        retractionTargetId() const { return retractTarget_; }
    void           setUserId(const QString &id) { userId_ = id; }
    QString        userId() const { return userId_; }
    void           setReactionsId(const QString &id) { reactionsId_ = id; }
    QString        reactionsId() const { return reactionsId_; }
    void           setReactions(const QSet<QString> &value) { reactions_ = value; }
    QSet<QString>  reactions() const { return reactions_; }
    const QString &archiveId() const { return archiveId_; }
    void           setArchiveId(const QString &id) { archiveId_ = id; }
    const QString &replaceId() const { return replaceId_; }
    void           setReplaceId(const QString &id)
    {
        replaceId_ = id;
        if (!id.isEmpty())
            edited_ = true;
    }
    void             setEdited(bool edited = true) { edited_ = edited; }
    void             setEncrypted(bool encrypted, const QString &protocol = {})
    {
        securityKnown_      = true;
        encrypted_          = encrypted;
        encryptionProtocol_ = encrypted ? protocol : QString();
    }
    bool           isSecurityKnown() const { return securityKnown_; }
    bool           isEncrypted() const { return encrypted_; }
    const QString &encryptionProtocol() const { return encryptionProtocol_; }
    bool             isEdited() const { return edited_; }
    const QString   &retractionId() const { return retractionId_; }
    void             setRetractionId(const QString &id) { retractionId_ = id; }
    bool             isLocal() const { return local_; }
    void             setLocal(bool local) { local_ = local; }
    void             setSpooled(bool value) { spooled_ = value; }
    bool             isSpooled() const { return spooled_; }
    const QDateTime &dateTime() const { return when_; }
    void             setDateTime(const QDateTime &dateTime) { when_ = dateTime; }
    const QString   &nick() const { return nick_; }
    void             setNick(const QString &nick) { nick_ = nick; }
    const QString   &text() const { return body_; }
    void             setPlainText(const QString &body) { body_ = body; }
    QString          formattedText() const { return body_; }
    QString          formattedUserText() const { return userText_; }
    void             setUserText(const QString &text) { userText_ = text; }

private:
    QDateTime     revisionTime_;
    QString       target_, retractTarget_, userId_, reactionsId_;
    QSet<QString> reactions_;
    qint64        historyId_ = 0;
    Type          type_;
    QString       id_;
    QString       archiveId_;
    QString       replaceId_;
    QString       retractionId_;
    QString       body_;
    QString       userText_;
    QString       nick_;
    QDateTime     when_;
    bool          local_   = false;
    bool          spooled_ = false;
    bool          edited_  = false;
    bool          markable_ = false;
    bool          securityKnown_ = false;
    bool          encrypted_     = false;
    QString       encryptionProtocol_;
};
