#include "opt_chat.h"
#include "common.h"
#include "iconwidget.h"

#include <QWhatsThis>
#include <QCheckBox>
#include <QComboBox>
#include <QButtonGroup>
#include <QRadioButton>

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
	bg_defAct->addButton( d->rb_defActMsg);
	bg_defAct->addButton( d->rb_defActChat);

	bg_delChats = new QButtonGroup;
	bg_delChats->setExclusive( true );
	bg_delChats->addButton( d->rb_delChatsClose);
	bg_delChats->addButton( d->rb_delChatsHour);
	bg_delChats->addButton( d->rb_delChatsDay);
	bg_delChats->addButton( d->rb_delChatsNever);

	connect(d->ck_tabChats, SIGNAL(toggled(bool)), d->cb_tabGrouping, SLOT(setEnabled(bool)));

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
	d->ck_tabChats->setWhatsThis(
		tr("Makes Psi open chats in a tabbed window."));
	QString s = tr("<P>Controls how long the chat log will be kept in memory after the"
		" chat window is closed.</P>");
	d->ck_showPreviews->setWhatsThis(
		tr("Show under links to some media content preview of the content."
		" It's also possible to play audio and video right in chat."));
	d->rb_delChatsClose->setWhatsThis(s +
		tr("<P>This option does not keep the chat log in memory.</P>"));
	d->rb_delChatsHour->setWhatsThis(s +
		tr("<P>This option keeps the chat log for 1 hour before deleting it.</P>"));
	d->rb_delChatsDay->setWhatsThis(s +
		tr("<P>This option keeps the chat log for 1 day before deleting it.</P>"));
	d->rb_delChatsNever->setWhatsThis(s +
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
	PsiOptions::instance()->setOption("options.ui.chat.switch-tab-on-new-messages", d->ck_switchTabOnMessage->isChecked());
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
	QString tabGrouping;
	int idx = d->cb_tabGrouping->currentIndex();
	switch (idx) {
		case 0:
			tabGrouping = "C";
			break;
		case 1:
			tabGrouping = "M";
			break;
		case 2:
			tabGrouping = "C:M";
			break;
		case 3:
			tabGrouping = "CM";
			break;
		case 4:
			tabGrouping = "ACM";
			break;
	}
	if (!tabGrouping.isEmpty()) {
		PsiOptions::instance()->setOption("options.ui.tabs.grouping", tabGrouping);
	} else {
		if (d->cb_tabGrouping->count() == 6) {
			d->cb_tabGrouping->removeItem(5);
		}
	}

	PsiOptions::instance()->setOption("options.ui.chat.use-expanding-line-edit", d->ck_autoResize->isChecked());

	PsiOptions::instance()->setOption("options.ui.tabs.use-tab-shortcuts", d->ck_tabShortcuts->isChecked());

	PsiOptions::instance()->setOption("options.ui.chat.show-previews", d->ck_showPreviews->isChecked());

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
	d->ck_switchTabOnMessage->setChecked( PsiOptions::instance()->getOption("options.ui.chat.switch-tab-on-new-messages").toBool() );
	d->ck_smallChats->setChecked( PsiOptions::instance()->getOption("options.ui.chat.use-small-chats").toBool() );
	d->ck_tabChats->setChecked( PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool() );
	d->cb_tabGrouping->setEnabled(PsiOptions::instance()->getOption("options.ui.tabs.use-tabs").toBool());
	QString tabGrouping = PsiOptions::instance()->getOption("options.ui.tabs.grouping").toString();
	bool custom = false;
	if (tabGrouping == "C") {
		d->cb_tabGrouping->setCurrentIndex(0);
	} else if (tabGrouping == "M") {
		d->cb_tabGrouping->setCurrentIndex(1);
	} else if (tabGrouping == "C:M") {
		d->cb_tabGrouping->setCurrentIndex(2);
	} else if (tabGrouping == "CM") {
		d->cb_tabGrouping->setCurrentIndex(3);
	} else if (tabGrouping == "ACM") {
		d->cb_tabGrouping->setCurrentIndex(4);
	} else {
		if (d->cb_tabGrouping->count() == 6) {
			d->cb_tabGrouping->setCurrentIndex(5);
		} else {
			d->cb_tabGrouping->setCurrentIndex(-1);
		}
		custom = true;
	}
	if (!custom && d->cb_tabGrouping->count() == 6) {
		d->cb_tabGrouping->removeItem(5);
	}
	d->ck_autoResize->setChecked( PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool() );
	d->ck_tabShortcuts->setChecked( PsiOptions::instance()->getOption("options.ui.tabs.use-tab-shortcuts").toBool() );
	d->ck_showPreviews->setChecked( PsiOptions::instance()->getOption("options.ui.chat.show-previews").toBool() );
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
