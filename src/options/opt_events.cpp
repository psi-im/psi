#include "opt_events.h"
#include "common.h"
#include "iconwidget.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlineedit.h>

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

void OptionsTabEvents::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptEventsUI *d = (OptEventsUI *)w;
	opt->popupMsgs  = d->ck_popupMsgs->isChecked();
	opt->popupChats = d->ck_popupMsgs->isChecked();
	opt->popupHeadlines = d->ck_popupHeadlines->isChecked();
	opt->popupFiles = d->ck_popupFiles->isChecked();
	opt->noAwayPopup = !d->ck_allowAwayPopup->isChecked();
	opt->noUnlistedPopup = !d->ck_allowUnlistedPopup->isChecked();
	opt->raise = d->ck_raise->isChecked();
	opt->ignoreNonRoster = d->ck_ignoreNonRoster->isChecked();
	opt->alertStyle = d->cb_animation->currentIndex();
	opt->autoAuth = d->ck_autoAuth->isChecked();
	opt->notifyAuth = d->ck_notifyAuth->isChecked();
	opt->bounceDock = (Options::BounceDockSetting) d->cb_bounce->currentItem();

	opt->ppIsOn = d->ck_popupOn->isChecked();
	opt->ppMessage = d->ck_popupOnMessage->isChecked();
	opt->ppChat    = d->ck_popupOnMessage->isChecked();
	opt->ppHeadline = d->ck_popupOnHeadline->isChecked();
	opt->ppFile    = d->ck_popupOnFile->isChecked();
	opt->ppOnline  = d->ck_popupOnOnline->isChecked();
	opt->ppOffline = d->ck_popupOnOffline->isChecked();
	opt->ppStatus  = d->ck_popupOnStatus->isChecked();
}

void OptionsTabEvents::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptEventsUI *d = (OptEventsUI *)w;
	d->ck_popupMsgs->setChecked( opt->popupMsgs || opt->popupChats );
	d->ck_popupHeadlines->setChecked( opt->popupHeadlines );
	d->ck_popupFiles->setChecked( opt->popupFiles );
	d->ck_allowAwayPopup->setChecked( !opt->noAwayPopup );
	d->ck_allowUnlistedPopup->setChecked( !opt->noUnlistedPopup );
	d->ck_raise->setChecked( opt->raise );
	d->ck_ignoreNonRoster->setChecked( opt->ignoreNonRoster );
	d->cb_animation->setCurrentItem(opt->alertStyle);
	d->ck_autoAuth->setChecked( opt->autoAuth );
	d->ck_notifyAuth->setChecked( opt->notifyAuth );
	d->cb_bounce->setCurrentItem( opt->bounceDock );

	d->ck_popupOn->setChecked( opt->ppIsOn );
	d->ck_popupOnMessage->setChecked( opt->ppMessage || opt->ppChat );
	d->ck_popupOnHeadline->setChecked( opt->ppHeadline );
	d->ck_popupOnFile->setChecked( opt->ppFile );
	d->ck_popupOnOnline->setChecked( opt->ppOnline );
	d->ck_popupOnOffline->setChecked( opt->ppOffline );
	d->ck_popupOnStatus->setChecked( opt->ppStatus );
}
