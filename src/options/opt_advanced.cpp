#include "opt_advanced.h"
#include "common.h"
#include "iconwidget.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

#include "ui_opt_advanced.h"
#include "psioptions.h"
#include "spellchecker.h"

class OptAdvancedUI : public QWidget, public Ui::OptAdvanced
{
public:
	OptAdvancedUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabAdvanced
//----------------------------------------------------------------------------

OptionsTabAdvanced::OptionsTabAdvanced(QObject *parent)
: OptionsTab(parent, "advanced", "", tr("Advanced"), tr("Options for advanced users"), "psi/advanced")
{
	w = 0;
}

OptionsTabAdvanced::~OptionsTabAdvanced()
{
}

QWidget *OptionsTabAdvanced::widget()
{
	if ( w )
		return 0;

	w = new OptAdvancedUI();
	OptAdvancedUI *d = (OptAdvancedUI *)w;

#ifdef Q_WS_X11	// auto-copy is a built-in feature on linux, we don't want user to use our own one
	d->ck_autocopy->hide();
#endif

	d->ck_spell->setEnabled(SpellChecker::instance()->available());

	QWhatsThis::add(d->ck_messageevents,
		tr("Enables the sending and requesting of message events such as "
		"'Contact is Typing', ..."));
	QWhatsThis::add(d->ck_inactiveevents,
		tr("Enables the sending of events when you end or suspend a "
		"conversation"));
	QWhatsThis::add(d->ck_rc,
		tr("Enables remote controlling your client from other locations"));
	QWhatsThis::add(d->ck_spell,
		tr("Check this option if you want your spelling to be checked"));
	QWhatsThis::add(d->ck_autocopy,
		tr("Check this option if you want the selected text in incoming messages and chat log to be automatically copied to clipboard"));
	QWhatsThis::add(d->ck_singleclick,
		tr("Normally, a double-click on a contact will invoke the default action."
		"  Check this option if you'd rather invoke with a single-click."));
	QWhatsThis::add(d->ck_jidComplete,
		tr("Enables as-you-type JID autocompletion in message dialog."));
	QWhatsThis::add(d->ck_grabUrls,
		tr("Automatically attaches URLs from clipboard to the messages when enabled"));
	QWhatsThis::add(d->cb_incomingAs,
		tr("<P>Specifies how to treat incoming events:</P>"
		"<P><B>Normal</B> - messages come as messages, chats come as chats.</P>"
		"<P><B>Messages</B> - All messages/chats come as messages, no matter what their original form was.</P>"
		"<P><B>Chats</B> - All messages/chats come as chats, no matter what their original form was.</P>"));
	QWhatsThis::add(d->ck_showSubjects,
		tr("Makes Psi show separate subject line in messages. Uncheck this if you want to save some screen space."));
	QWhatsThis::add(d->ck_showCounter,
		tr("Makes Psi show message length counter. Check this if you want to know how long is your message. Can be useful when you're using SMS transport."));
	QWhatsThis::add(d->ck_autoVCardOnLogin,
		tr("By default, Psi always checks your vCard on login. If you want to save some traffic, you can uncheck this option."));
	QWhatsThis::add(d->ck_rosterAnim,
		tr("Makes Psi animate contact names in the main window when they come online."));
	QWhatsThis::add(d->ck_scrollTo,
		tr("Makes Psi scroll the main window automatically so you can see new incoming events."));
	QWhatsThis::add(d->ck_ignoreHeadline,
		tr("Makes Psi ignore all incoming \"headline\" events,"
		" like system-wide news on MSN, announcements, etc."));

	connect(d->ck_messageevents,SIGNAL(toggled(bool)),d->ck_inactiveevents,SLOT(setEnabled(bool)));
	d->ck_inactiveevents->setEnabled(d->ck_messageevents->isChecked());

	return w;
}

void OptionsTabAdvanced::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptAdvancedUI *d = (OptAdvancedUI *)w;

	opt->messageEvents = d->ck_messageevents->isChecked();
	opt->inactiveEvents = d->ck_inactiveevents->isChecked();
	opt->useRC = d->ck_rc->isChecked();
	if ( SpellChecker::instance()->available() )
		PsiOptions::instance()->setOption("options.ui.spell-check.enabled",d->ck_spell->isChecked());
	opt->autoCopy = d->ck_autocopy->isChecked();
	opt->singleclick = d->ck_singleclick->isChecked();
	opt->jidComplete = d->ck_jidComplete->isChecked();
	opt->grabUrls    = d->ck_grabUrls->isChecked();
	opt->incomingAs = d->cb_incomingAs->currentItem();
	opt->showSubjects = d->ck_showSubjects->isChecked();
	opt->showCounter = d->ck_showCounter->isChecked();
	opt->autoVCardOnLogin = d->ck_autoVCardOnLogin->isChecked();
	opt->rosterAnim = d->ck_rosterAnim->isChecked();
	opt->scrollTo = d->ck_scrollTo->isChecked();
	opt->ignoreHeadline = d->ck_ignoreHeadline->isChecked();
}

void OptionsTabAdvanced::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptAdvancedUI *d = (OptAdvancedUI *)w;

	d->ck_messageevents->setChecked( opt->messageEvents );
	d->ck_inactiveevents->setChecked( opt->inactiveEvents );
	d->ck_rc->setChecked( opt->useRC );
	if ( !SpellChecker::instance()->available() )
		d->ck_spell->setChecked(false);
	else
		d->ck_spell->setChecked(PsiOptions::instance()->getOption("options.ui.spell-check.enabled").toBool());
	d->ck_autocopy->setChecked( opt->autoCopy );
	d->ck_singleclick->setChecked( opt->singleclick );
	d->ck_jidComplete->setChecked( opt->jidComplete );
	d->ck_grabUrls->setChecked( opt->grabUrls );
	d->cb_incomingAs->setCurrentItem( opt->incomingAs );
	d->ck_showSubjects->setChecked( opt->showSubjects );
	d->ck_showCounter->setChecked( opt->showCounter );
	d->ck_autoVCardOnLogin->setChecked( opt->autoVCardOnLogin );
	d->ck_rosterAnim->setChecked( opt->rosterAnim );
	d->ck_scrollTo->setChecked( opt->scrollTo );
	d->ck_ignoreHeadline->setChecked( opt->ignoreHeadline );
}
