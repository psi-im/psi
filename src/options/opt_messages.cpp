#include "opt_messages.h"

#include "opt_chat.h"
#include "opt_groupchat.h"
#include "opt_input.h"
#include "opt_messages_common.h"

OptionsTabMessages::OptionsTabMessages(QObject *parent)
: MetaOptionsTab(parent, "chat", "", tr("Messages"), tr("Messages options"), "psi/start-chat")
{
    addTab(new OptionsTabMsgCommon(this));
    addTab(new OptionsTabChat(this));
    addTab(new OptionsTabGroupchat(this));
    addTab(new OptionsTabInput(this));
}
