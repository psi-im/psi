#include "opt_chat.h"

#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "ui_opt_chat.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QWhatsThis>

class OptChatUI : public QWidget, public Ui::OptChat
{
public:
    OptChatUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabChat
//----------------------------------------------------------------------------

OptionsTabChat::OptionsTabChat(QObject *parent)
: OptionsTab(parent, "chat", "", tr("Chat"), tr("Configure the chat dialog"), "psi/start-chat")
, w(nullptr)
, bg_defAct(nullptr)
{
}

OptionsTabChat::~OptionsTabChat()
{
    if ( bg_defAct )
        delete bg_defAct;
}

QWidget *OptionsTabChat::widget()
{
    if ( w )
        return nullptr;

    w = new OptChatUI();
    OptChatUI *d = static_cast<OptChatUI *>(w);

    bg_defAct = new QButtonGroup;
    bg_defAct->setExclusive( true );
    bg_defAct->addButton( d->rb_defActMsg);
    bg_defAct->addButton( d->rb_defActChat);

    d->rb_defActMsg->setWhatsThis(
        tr("Make the default action open a normal message window."));
    d->rb_defActChat->setWhatsThis(
        tr("Make the default action open a chat window."));
    d->ck_chatSoftReturn->setWhatsThis(
        tr("<P>When checked, pressing Enter in a chat window will send your message."
           "  You must use Shift+Enter in order to create a newline in the chat message."
           "  If unchecked, messages are sent by pressing Alt-S or Control-Enter, just as they are with regular messages.</P>"));
    d->ck_alertOpenChats->setWhatsThis(
        tr("Normally, Psi will not alert you when a new chat message"
        " is received in a chat window that is already open."
        "  Check this option if you want to receive these alerts anyway."));
    d->ck_raiseChatWindow->setWhatsThis(
        tr("Makes Psi bring an open chat window to the front of your screen when you receive a new message."
        " It does not take the keyboard focus, so it will not interfere with your work."));
    d->ck_switchTabOnMessage->setWhatsThis(
        tr("Makes Psi switch tab on active tabbed window when you receive a new message."
        " It does not take the keyboard focus, so it will not interfere with your work."));
    d->ck_smallChats->setWhatsThis(
        tr("Makes Psi open chat windows in compact mode."));
    QString s = tr("<P>Controls how long the chat log will be kept in memory after the"
        " chat window is closed.</P>");

    return w;
}

void OptionsTabChat::applyOptions()
{
    if ( !w )
        return;

    OptChatUI *d = static_cast<OptChatUI *>(w);

    PsiOptions::instance()->setOption("options.messages.default-outgoing-message-type", bg_defAct->buttons().indexOf(bg_defAct->checkedButton()) == 0 ? "message" : "chat");
    PsiOptions::instance()->setOption("options.ui.chat.alert-for-already-open-chats", d->ck_alertOpenChats->isChecked());
    PsiOptions::instance()->setOption("options.ui.chat.raise-chat-windows-on-new-messages", d->ck_raiseChatWindow->isChecked());
    PsiOptions::instance()->setOption("options.ui.chat.switch-tab-on-new-messages", d->ck_switchTabOnMessage->isChecked());
    PsiOptions::instance()->setOption("options.ui.chat.use-small-chats", d->ck_smallChats->isChecked());

    // Soft return.
    // Only update this if the value actually changed, or else custom presets
    // might go lost.
    bool soft = ShortcutManager::instance()->shortcuts("chat.send").contains(QKeySequence(Qt::Key_Return));
    if (soft != d->ck_chatSoftReturn->isChecked()) {
        QVariantList vl;
        if (d->ck_chatSoftReturn->isChecked()) {
            vl << qVariantFromValue(QKeySequence(Qt::Key_Enter)) << qVariantFromValue(QKeySequence(Qt::Key_Return));
        }
        else  {
            vl << qVariantFromValue(QKeySequence(Qt::Key_Enter+Qt::CTRL)) << qVariantFromValue(QKeySequence(Qt::CTRL+Qt::Key_Return));
        }
        PsiOptions::instance()->setOption("options.shortcuts.chat.send",vl);
    }
    PsiOptions::instance()->setOption("options.ui.chat.history.preload-history-size", d->sb_msgHistCount->value());
}

void OptionsTabChat::restoreOptions()
{
    if ( !w )
        return;

    OptChatUI *d = static_cast<OptChatUI *>(w);

    bg_defAct->buttons()[PsiOptions::instance()->getOption("options.messages.default-outgoing-message-type").toString() == "message" ? 0 : 1]->setChecked(true);
    d->ck_alertOpenChats->setChecked( PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool() );
    d->ck_raiseChatWindow->setChecked( PsiOptions::instance()->getOption("options.ui.chat.raise-chat-windows-on-new-messages").toBool() );
    d->ck_switchTabOnMessage->setChecked( PsiOptions::instance()->getOption("options.ui.chat.switch-tab-on-new-messages").toBool() );
    d->ck_smallChats->setChecked( PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool() );

    d->ck_chatSoftReturn->setChecked(ShortcutManager::instance()->shortcuts("chat.send").contains(QKeySequence(Qt::Key_Return)));
    d->sb_msgHistCount->setValue(PsiOptions::instance()->getOption("options.ui.chat.history.preload-history-size").toInt());
}
