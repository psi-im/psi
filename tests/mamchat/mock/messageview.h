/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

// Test-only MessageView contract. Keeps this widget test independent from
// PsiAccount, PsiOptions and the full application while compiling the actual
// MamChatWidget.cpp, LongListWidget.cpp and their real Qt rows.
#include <QDateTime>
#include <QString>

class MessageView {
public:
    enum Type { Message, System, Reactions, MessageRetraction, MUCJoin, MUCPart, Status, Subject, Urls, NickChange, FileTransferRequest, FileTransferFinished };
    explicit MessageView(Type type = Message) : type_(type) { }

    bool isJoinLeaveHidden() const { return false; }
    bool isStatusChangeHidden() const { return false; }
    Type type() const { return type_; }
    const QString &messageId() const { return id_; }
    void setMessageId(const QString &id) { id_ = id; }
    void setHistoryId(qint64 id) { historyId_ = id; }
    qint64 historyId() const { return historyId_; }
    const QString &archiveId() const { return archiveId_; }
    void setArchiveId(const QString &id) { archiveId_ = id; }
    const QString &replaceId() const { return replaceId_; }
    void setReplaceId(const QString &id) { replaceId_ = id; }
    const QString &retractionId() const { return retractionId_; }
    void setRetractionId(const QString &id) { retractionId_ = id; }
    bool isLocal() const { return local_; }
    void setLocal(bool local) { local_ = local; }
    const QDateTime &dateTime() const { return when_; }
    void setDateTime(const QDateTime &dateTime) { when_ = dateTime; }
    const QString &nick() const { return nick_; }
    void setNick(const QString &nick) { nick_ = nick; }
    const QString &text() const { return body_; }
    void setPlainText(const QString &body) { body_ = body; }
    QString formattedText() const { return body_; }
    QString formattedUserText() const { return userText_; }
    void setUserText(const QString &text) { userText_ = text; }
private:
    qint64 historyId_ = 0;
    Type type_;
    QString id_;
    QString archiveId_;
    QString replaceId_;
    QString retractionId_;
    QString body_;
    QString userText_;
    QString nick_;
    QDateTime when_;
    bool local_ = false;
};
