#include "opt_chat.h"
#include "common.h"
#include "iconwidget.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

#include "ui_opt_chat.h"
#include "shortcutmanager.h"
#include "psioptions.h"

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

void OptionsTabChat::applyOptions()
{
	if ( !w )
		return;

	OptChatUI *d = (OptChatUI *)w;

	PsiOptions::instance()->setOption("options.messages.default-outgoing-message-type", bg_defAct->buttons().indexOf(bg_defAct->checkedButton()) == 0 ? "message" : "chat");
	PsiOptions::instance()->setOption("options.ui.chat.alert-for-already-open-chats", d->ck_alertOpenChats->isChecked());
	PsiOptions::instance()->setOption("options.ui.chat.raise-chat-windows-on-new-messages", d->ck_raiseChatWindow->isChecked());
	PsiOptions::instance()->setOption("options.ui.chat.use-small-chats", d->ck_smallChats->isChecked());
	
	QString delafter;
	switch (bg_delChats->buttons().indexOf( bg_delChats->checkedButton() )) {
		case 0:
			delafter = "instant";
			break;
		case 1:
			delafter = "hour";
			break;
		case 2:
			delafter = "day";
			break;
		case 3:
			delafter = "never";
			break;
	}
	PsiOptions::instance()->setOption("options.ui.chat.delete-contents-after", delafter);
	PsiOptions::instance()->setOption("options.ui.tabs.use-tabs", d->ck_tabChats->isChecked());
	PsiOptions::instance()->setOption("options.ui.chat.use-expanding-line-edit", d->ck_autoResize->isChecked());
	
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
}

void OptionsTabChat::restoreOptions()
{
	if ( !w )
		return;

	OptChatUI *d = (OptChatUI *)w;

	bg_defAct->buttons()[PsiOptions::instance()->getOption("options.messages.default-outgoing-message-type").toString() == "message" ? 0 : 1]->setChecked(true);
	d->ck_alertOpenChats->setChecked( PsiOptions::instance()->getOption("options.ui.chat.alert-for-already-open-chats").toBool() );
	d->ck_raiseChatWindow->setChecked( PsiOptions::instance()->getOption("options.ui.chat.raise-chat-windows-on-new-messages").toBool() );
	d->ck_smallChats->setChecked( PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool() );
	d->ck_tabChats->setChecked( PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool() );
	d->ck_autoResize->setChecked( PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool() );
	QString delafter = PsiOptions::instance()->getOption("options.ui.chat.delete-contents-after").toString();
	if (delafter == "instant") {
		d->rb_delChatsClose->setChecked(true);
	} else if (delafter == "hour") {
		d->rb_delChatsHour->setChecked(true);
	} else if (delafter == "day") {
		d->rb_delChatsDay->setChecked(true);
	} else if (delafter == "never") {
		d->rb_delChatsNever->setChecked(true);
	}
	d->ck_chatSoftReturn->setChecked(ShortcutManager::instance()->shortcuts("chat.send").contains(QKeySequence(Qt::Key_Return)));
}
