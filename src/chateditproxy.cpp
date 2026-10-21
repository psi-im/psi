/*
 * chateditproxy.cpp - abstraction to change ChatEdit type in runtime
 * Copyright (C) 2007  Michail Pishchagin
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

#include "chateditproxy.h"
#include "coloropt.h"

#include "msgmle.h"
#include "psioptions.h"
#include "widgets/MessagePreview.h"
#include "iris/xmpp_message.h"

#include <QVBoxLayout>

static const QLatin1String expandingLineEdit("options.ui.chat.use-expanding-line-edit");

ChatEditProxy::ChatEditProxy(QWidget *parent) :
    QWidget(parent), lineEditEnabled_(false), textEdit_(nullptr), layout_(nullptr)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);
    const auto quoteColor = ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.quote"));
    replyPreview_ = new MessagePreview(quoteColor, quoteColor, this);
    replyPreview_->setLinkColor(ColorOpt::instance()->color(QStringLiteral("options.ui.look.colors.messages.link")));
    replyPreview_->setObjectName(QStringLiteral("composerReplyPreview"));
    replyPreview_->setClosable(true);
    layout_->addWidget(replyPreview_);
    replyPreview_->hide();
    connect(replyPreview_, &MessagePreview::dismissed, this, &ChatEditProxy::clearReply);
    connect(ColorOpt::instance(), &ColorOpt::changed, this, [this](const QString &option) {
        if (option == QLatin1String("options.ui.look.colors.messages.quote")) {
            const auto color = ColorOpt::instance()->color(option);
            replyPreview_->setColors(color, color);
        } else if (option == QLatin1String("options.ui.look.colors.messages.link")) {
            replyPreview_->setLinkColor(ColorOpt::instance()->color(option));
        }
    });

    connect(PsiOptions::instance(), &PsiOptions::optionChanged, this, &ChatEditProxy::optionsChanged);
    optionsChanged(expandingLineEdit);
}

/**
 * Creates new QTextEdit basing on ChatEditProxy's properties.
 */
ChatEdit *ChatEditProxy::createTextEdit()
{
    if (lineEditEnabled())
        return new LineEdit(this);
    return new ChatEdit(this);
}

void ChatEditProxy::setReply(const ChatReply &reply)
{
    if (!reply.isValid())
        return;
    reply_ = reply;
    replyPreview_->setContent(tr("Reply to %1").arg(reply.author), reply.text);
    replyPreview_->show();
    chatEdit()->resetCorrection();
    chatEdit()->setFocus();
}

void ChatEditProxy::clearReply()
{
    reply_ = {};
    replyPreview_->hide();
}

void ChatEditProxy::applyReply(XMPP::Message &message)
{
    const auto reply = reply_.isValid() ? reply_ : sentReplies_.value(message.replaceId());
    applyMessageReply(message, reply);
    if (!message.id().isEmpty()) {
        if (sentReplies_.size() >= 128)
            sentReplies_.erase(sentReplies_.begin());
        sentReplies_.insert(message.id(), reply);
    }
}

/**
 * Moves the QTextDocument and QTextCursor data from \a oldTextEdit
 * to \a newTextEdit.
 *
 * NB: Make sure that all QSyntaxHighlighters are detached prior to calling
 * this function.
 */
void ChatEditProxy::moveData(QTextEdit *newTextEdit, QTextEdit *oldTextEdit) const
{
    QTextDocument *doc    = oldTextEdit->document();
    QTextCursor    cursor = oldTextEdit->textCursor();

    doc->setParent(newTextEdit);
    oldTextEdit->setDocument(nullptr);

    newTextEdit->setDocument(doc);
    newTextEdit->setTextCursor(cursor);
}

/**
 * Creates new QTextEdit and moves data to it from the old one.
 * Text, selection and cursor position are left intact.
 */
void ChatEditProxy::updateLayout()
{
    ChatEdit *newEdit = createTextEdit();

    if (textEdit_) {
        // all syntaxhighlighters should be removed while we move
        // the documents around, and should be reattached afterwards
        textEdit_->setCheckSpelling(false);
        newEdit->setCheckSpelling(false);

        moveData(newEdit, textEdit_);

        newEdit->setCheckSpelling(ChatEdit::checkSpellingGloballyEnabled());
    }

    delete textEdit_;
    textEdit_ = newEdit;
    layout_->addWidget(textEdit_);
    emit textEditCreated();
}

/**
 * Update ChatEdit widget according to current options.
 */
void ChatEditProxy::optionsChanged(const QString &option)
{
    if (option == expandingLineEdit) {
        lineEditEnabled_ = PsiOptions::instance()->getOption(expandingLineEdit).toBool();
        updateLayout();
    }
}
