#include "opt_chat.h"
#include "common.h"
#include "iconwidget.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

#include "ui_opt_chat.h"

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
{
	w = 0;
	bg_delChats = bg_defAct = 0;
}

OptionsTabChat::~OptionsTabChat()
{
	if ( bg_defAct )
		delete bg_defAct;
	if ( bg_delChats )
		delete bg_delChats;
}

QWidget *OptionsTabChat::widget()
{
	if ( w )
		return 0;

	w = new OptChatUI();
	OptChatUI *d = (OptChatUI *)w;

	bg_defAct = new QButtonGroup;
	bg_defAct->setExclusive( true );
	bg_defAct->insert( d->rb_defActMsg);
	bg_defAct->insert( d->rb_defActChat);

	bg_delChats = new QButtonGroup;
	bg_delChats->setExclusive( true );
	bg_delChats->insert( d->rb_delChatsClose);
	bg_delChats->insert( d->rb_delChatsHour);
	bg_delChats->insert( d->rb_delChatsDay);
	bg_delChats->insert( d->rb_delChatsNever);

	QWhatsThis::add(d->rb_defActMsg,
		tr("Make the default action open a normal message window."));
	QWhatsThis::add(d->rb_defActChat,
		tr("Make the default action open a chat window."));
	QWhatsThis::add(d->ck_chatSays,
		tr("<P>Changes the normal chat style from:</P>"
		"<P>[01:23:45] &lt;MyName&gt; Hi</P>"
		"<P>[01:23:56] &lt;YourName&gt; How are you?</P>"
		"<P>to:</P>"
		"<P>[01:23:45] MyName says:</P>"
		"<P>Hi</P>"
		"<P>[01:23:56] YourName says:</P>"
		"<P>How are you?</P>"));
	QWhatsThis::add(d->ck_chatSoftReturn,
		tr("<P>When checked, pressing Enter in a chat window will send your message."
		   "  You must use Shift+Enter in order to create a newline in the chat message."
		   "  If unchecked, messages are sent by pressing Alt-S or Control-Enter, just as they are with regular messages.</P>"));
	QWhatsThis::add(d->ck_alertOpenChats,
		tr("Normally, Psi will not alert you when a new chat message"
		" is received in a chat window that is already open."
		"  Check this option if you want to receive these alerts anyway."));
	QWhatsThis::add(d->ck_raiseChatWindow,
		tr("Makes Psi bring an open chat window to the front of your screen when you receive a new message."
		" It does not take the keyboard focus, so it will not interfere with your work."));
	QWhatsThis::add(d->ck_smallChats,
		tr("Makes Psi open chat windows in compact mode."));
	QWhatsThis::add(d->ck_tabChats,
		tr("Makes Psi open chats in a tabbed window."));
	QString s = tr("<P>Controls how long the chat log will be kept in memory after the"
		" chat window is closed.</P>");
	QWhatsThis::add(d->rb_delChatsClose, s +
		tr("<P>This option does not keep the chat log in memory.</P>"));
	QWhatsThis::add(d->rb_delChatsHour, s +
		tr("<P>This option keeps the chat log for 1 hour before deleting it.</P>"));
	QWhatsThis::add(d->rb_delChatsDay, s +
		tr("<P>This option keeps the chat log for 1 day before deleting it.</P>"));
	QWhatsThis::add(d->rb_delChatsNever, s +
		tr("<P>This options keeps the chat log forever.</P>"));

	return w;
}

void OptionsTabChat::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptChatUI *d = (OptChatUI *)w;

	opt->defaultAction   = bg_defAct->buttons().indexOf(bg_defAct->checkedButton());
	opt->chatSays        = d->ck_chatSays->isChecked();
	opt->chatSoftReturn  = d->ck_chatSoftReturn->isChecked();
	opt->alertOpenChats  = d->ck_alertOpenChats->isChecked();
	opt->raiseChatWindow = d->ck_raiseChatWindow->isChecked();
	opt->oldSmallChats 	 = opt->smallChats;
	opt->smallChats      = d->ck_smallChats->isChecked();
	opt->delChats        = bg_delChats->buttons().indexOf( bg_delChats->checkedButton() );
	opt->useTabs		 = d->ck_tabChats->isChecked();
	opt->chatLineEdit    = d->ck_autoResize->isChecked();
}

void OptionsTabChat::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptChatUI *d = (OptChatUI *)w;

	bg_defAct->buttons()[opt->defaultAction]->setChecked(true);
	d->ck_chatSays->setChecked( opt->chatSays );
	d->ck_chatSoftReturn->setChecked( opt->chatSoftReturn );
	d->ck_alertOpenChats->setChecked( opt->alertOpenChats );
	d->ck_raiseChatWindow->setChecked( opt->raiseChatWindow );
	d->ck_smallChats->setChecked( opt->smallChats );
	d->ck_tabChats->setChecked( opt->useTabs );
	d->ck_autoResize->setChecked( opt->chatLineEdit );
	bg_delChats->buttons()[opt->delChats]->setChecked(true);
}
