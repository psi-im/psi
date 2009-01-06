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
	QWhatsThis::add(d->ck_allowNonRoster,
		tr("If this is not checked, then incoming events from "
		"contacts not already in your list of contacts will be ignored."));
	
/*	QWhatsThis::add(d->rb_aSolid,
		tr("Does not animate or blink incoming event icons on the main window as they are received."));
	QWhatsThis::add(d->rb_aBlink,
		tr("Makes all incoming event icons blink on the main window as events are received."));
	QWhatsThis::add(d->rb_aAnimate,
		tr("Animates incoming event icons on the main window as events are recieved."));*/
	QWhatsThis::add(d->ck_autoAuth,
		tr("Makes the Barracuda client automatically accept all authorization requests from <b>anyone</b>."));
	QWhatsThis::add(d->ck_notifyAuth,
		tr("Makes the Barracuda client notify you when your authorization request was approved."));

	
	d->cb_bounce->setItemData(0, "never");
	d->cb_bounce->setItemData(1, "once");
	d->cb_bounce->setItemData(2, "forever");
	
#ifndef Q_WS_MAC
	d->cb_bounce->hide();
	d->lb_bounce->hide();
#endif

#ifdef Q_WS_MAC
	d->groupBox1->hide();
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
	PsiOptions::instance()->setOption("options.ui.message.auto-popup-headlines", d->ck_popupMsgs->isChecked());
	PsiOptions::instance()->setOption("options.ui.file-transfer.auto-popup", d->ck_popupMsgs->isChecked());
	PsiOptions::instance()->setOption("options.messages.ignore-non-roster-contacts", !d->ck_allowNonRoster->isChecked());
	PsiOptions::instance()->setOption("options.subscriptions.automatically-allow-authorization", d->ck_autoAuth->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.successful-subscription", d->ck_notifyAuth->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.bounce-dock", d->cb_bounce->itemData( d->cb_bounce->currentItem()));

	bool ppIsOn = false;
	if(d->ck_popupOnMessage->isChecked() || d->ck_popupOnOnline->isChecked() || d->ck_popupOnOffline->isChecked() || d->ck_popupOnStatus->isChecked()) {
		ppIsOn = true;
	}

	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.enabled", ppIsOn);
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-message", d->ck_popupOnMessage->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-chat", d->ck_popupOnMessage->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-headline", d->ck_popupOnMessage->isChecked());
	PsiOptions::instance()->setOption("options.ui.notifications.passive-popups.incoming-file-transfer", d->ck_popupOnMessage->isChecked());
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
	d->ck_allowNonRoster->setChecked( !PsiOptions::instance()->getOption("options.messages.ignore-non-roster-contacts").toBool() );
	d->ck_autoAuth->setChecked( PsiOptions::instance()->getOption("options.subscriptions.automatically-allow-authorization").toBool() );
	d->ck_notifyAuth->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.successful-subscription").toBool() );
	d->cb_bounce->setCurrentItem( d->cb_bounce->findData(PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString()) );

	d->ck_popupOnMessage->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-message").toBool() || PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.incoming-chat").toBool() );
	d->ck_popupOnOnline->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.online").toBool() );
	d->ck_popupOnOffline->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.offline").toBool() );
	d->ck_popupOnStatus->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.status.other-changes").toBool() );
}
