#include "opt_events.h"
#include "common.h"
#include "iconwidget.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include "psioptions.h"

#include "ui_opt_events.h"

class OptEventsUI : public QWidget, public Ui::OptEvents
{
public:
	OptEventsUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabEvents
//----------------------------------------------------------------------------

OptionsTabEvents::OptionsTabEvents(QObject *parent)
: OptionsTab(parent, "events", "", tr("Events"), tr("The events behaviour"), "psi/events")
{
	w = 0;
}

QWidget *OptionsTabEvents::widget()
{
	if ( w )
		return 0;

	w = new OptEventsUI();
	OptEventsUI *d = (OptEventsUI *)w;

	QWhatsThis::add(d->ck_popupMsgs,
		tr("Makes new incoming message windows pop up automatically when received."));
	QWhatsThis::add(d->ck_popupHeadlines,
		tr("Makes new incoming headlines pop up automatically when received."));
	QWhatsThis::add(d->ck_popupFiles,
		tr("Makes new incoming file requests pop up automatically when received."));
	QWhatsThis::add(d->ck_allowAwayPopup,
		tr("Normally, Psi will not autopopup events when you are away.  "
		"Set this option if you want them to popup anyway."));
	QWhatsThis::add(d->ck_allowUnlistedPopup,
		tr("Normally, Psi will not autopopup events from users not in your roster.  "
		"Set this option if you want them to popup anyway."));
	QWhatsThis::add(d->ck_raise,
		tr("Makes new incoming events bring the main window to the foreground."));
	QWhatsThis::add(d->ck_ignoreNonRoster,
		tr("Makes Psi ignore all incoming events from contacts"
		" not already in your list of contacts."));
	QWhatsThis::add(d->cb_animation,
		tr("What kind of animation should psi use for incoming event icons on the main window?"));
	
	d->cb_animation->setItemData ( 0, "no");
	d->cb_animation->setItemData ( 1, "blink");
	d->cb_animation->setItemData ( 2, "animate");
/*	QWhatsThis::add(d->rb_aSolid,
		tr("Does not animate or blink incoming event icons on the main window as they are received."));
	QWhatsThis::add(d->rb_aBlink,
		tr("Makes all incoming event icons blink on the main window as events are received."));
	QWhatsThis::add(d->rb_aAnimate,
		tr("Animates incoming event icons on the main window as events are recieved."));*/
	QWhatsThis::add(d->ck_autoAuth,
		tr("Makes Psi automatically accept all authorization requests from <b>anyone</b>."));
	QWhatsThis::add(d->ck_notifyAuth,
		tr("Makes Psi notify you when your authorization request was approved."));

	
	d->cb_bounce->setItemData(0, "never");
	d->cb_bounce->setItemData(1, "once");
	d->cb_bounce->setItemData(2, "forever");
	
#ifndef Q_WS_MAC
	d->cb_bounce->hide();
	d->lb_bounce->hide();
#endif
/*
	list_alerts.insert(0,d->rb_aSolid);
	list_alerts.insert(1,d->rb_aBlink);
	list_alerts.insert(2,d->rb_aAnimate);
*/
	return w;
}

void OptionsTabEvents::applyOptions()
{
	if ( !w )
		return;

	OptEventsUI *d = (OptEventsUI *)w;
	PsiOptions::instance()->setOption("options.ui.message.auto-popup", d->ck_popupMsgs->isChecked());
	PsiOptions::instance()->setOption("options.ui.chat.auto-popup", d->ck_popupMsgs->isChecked());
	PsiOptions::instance()->setOption("options.ui.message.auto-popup-headlines", d->ck_popupHeadlines->isChecked());
	PsiOptions::instance()->setOption("options.ui.file-transfer.auto-popup", d->ck_popupFiles->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.popup-dialogs.suppress-while-away", !d->ck_allowAwayPopup->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.popup-dialogs.suppress-when-not-on-roster", !d->ck_allowUnlistedPopup->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.raise-on-new-event", d->ck_raise->isChecked());
	PsiOptions::instance()->setOption("options.messages.ignore-non-roster-contacts", d->ck_ignoreNonRoster->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.alert-style", d->cb_animation->itemData(d->cb_animation->currentIndex()));
	PsiOptions::instance()->setOption("options.subscriptions.automatically-allow-authorization", d->ck_autoAuth->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.successful-subscription", d->ck_notifyAuth->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.bounce-dock", d->cb_bounce->itemData( d->cb_bounce->currentItem()));

	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.enabled", d->ck_popupOn->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-message", d->ck_popupOnMessage->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-chat", d->ck_popupOnMessage->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-headline", d->ck_popupOnHeadline->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-file-transfer", d->ck_popupOnFile->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.status.online", d->ck_popupOnOnline->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.status.offline", d->ck_popupOnOffline->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.status.other-changes", d->ck_popupOnStatus->isChecked());
}

void OptionsTabEvents::restoreOptions()
{
	if ( !w )
		return;

	OptEventsUI *d = (OptEventsUI *)w;
	d->ck_popupMsgs->setChecked( PsiOptions::instance()->getOption("options.ui.message.auto-popup").toBool() || PsiOptions::instance()->getOption("options.ui.chat.auto-popup").toBool() );
	d->ck_popupHeadlines->setChecked( PsiOptions::instance()->getOption("options.ui.message.auto-popup-headlines").toBool() );
	d->ck_popupFiles->setChecked( PsiOptions::instance()->getOption("options.ui.file-transfer.auto-popup").toBool() );
	d->ck_allowAwayPopup->setChecked( !PsiOptions::instance()->getOption("options.ui.notifications.popup-dialogs.suppress-while-away").toBool() );
	d->ck_allowUnlistedPopup->setChecked( !PsiOptions::instance()->getOption("options.ui.notifications.popup-dialogs.suppress-when-not-on-roster").toBool() );
	d->ck_raise->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.raise-on-new-event").toBool() );
	d->ck_ignoreNonRoster->setChecked( PsiOptions::instance()->getOption("options.messages.ignore-non-roster-contacts").toBool() );
	d->cb_animation->setCurrentItem(d->cb_animation->findData(PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString()));
	d->ck_autoAuth->setChecked( PsiOptions::instance()->getOption("options.subscriptions.automatically-allow-authorization").toBool() );
	d->ck_notifyAuth->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.successful-subscription").toBool() );
	d->cb_bounce->setCurrentItem( d->cb_bounce->findData(PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString()) );

	d->ck_popupOn->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.enabled").toBool() );
	d->ck_popupOnMessage->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-message").toBool() || PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-chat").toBool() );
	d->ck_popupOnHeadline->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-headline").toBool() );
	d->ck_popupOnFile->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-file-transfer").toBool() );
	d->ck_popupOnOnline->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.online").toBool() );
	d->ck_popupOnOffline->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.offline").toBool() );
	d->ck_popupOnStatus->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.other-changes").toBool() );
}
