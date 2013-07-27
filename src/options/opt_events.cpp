#include "opt_events.h"
#include "common.h"
#include "iconwidget.h"

#include <QWhatsThis>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
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
	, w(0)
{
}

QWidget *OptionsTabEvents::widget()
{
	if ( w )
		return 0;

	w = new OptEventsUI();
	OptEventsUI *d = (OptEventsUI *)w;

	d->ck_popupMsgs->setWhatsThis(
		tr("Makes new incoming message windows pop up automatically when received."));
	d->ck_popupHeadlines->setWhatsThis(
		tr("Makes new incoming headlines pop up automatically when received."));
	d->ck_popupFiles->setWhatsThis(
		tr("Makes new incoming file requests pop up automatically when received."));
	d->ck_allowAwayPopup->setWhatsThis(
		tr("Normally, Psi will not autopopup events when you are away.  "
		"Set this option if you want them to popup anyway."));
	d->ck_allowUnlistedPopup->setWhatsThis(
		tr("Normally, Psi will not autopopup events from users not in your roster.  "
		"Set this option if you want them to popup anyway."));
	d->ck_raise->setWhatsThis(
		tr("Makes new incoming events bring the main window to the foreground."));
	d->ck_ignoreNonRoster->setWhatsThis(
		tr("Makes Psi ignore all incoming events from contacts"
		" not already in your list of contacts."));
	d->cb_animation->setWhatsThis(
		tr("What kind of animation should psi use for incoming event icons on the main window?"));

	d->cb_animation->setItemData ( 0, "no");
	d->cb_animation->setItemData ( 1, "blink");
	d->cb_animation->setItemData ( 2, "animate");
/*	d->rb_aSolid->setWhatsThis(
		tr("Does not animate or blink incoming event icons on the main window as they are received."));
	d->rb_aBlink->setWhatsThis(
		tr("Makes all incoming event icons blink on the main window as events are received."));
	d->rb_aAnimate->setWhatsThis(
		tr("Animates incoming event icons on the main window as events are recieved."));*/
	d->ck_autoAuth->setWhatsThis(
		tr("Makes Psi automatically accept all authorization requests from <b>anyone</b>."));
	d->ck_notifyAuth->setWhatsThis(
		tr("Makes Psi notify you when your authorization request was approved."));


	d->cb_bounce->setItemData(0, "never");
	d->cb_bounce->setItemData(1, "once");
	d->cb_bounce->setItemData(2, "forever");

#ifndef Q_OS_MAC
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
	PsiOptions::instance()->setOption("options.ui.notifications.bounce-dock", d->cb_bounce->itemData( d->cb_bounce->currentIndex()));
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
	d->cb_animation->setCurrentIndex(d->cb_animation->findData(PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString()));
	d->ck_autoAuth->setChecked( PsiOptions::instance()->getOption("options.subscriptions.automatically-allow-authorization").toBool() );
	d->ck_notifyAuth->setChecked( PsiOptions::instance()->getOption("options.ui.notifications.successful-subscription").toBool() );
	d->cb_bounce->setCurrentIndex( d->cb_bounce->findData(PsiOptions::instance()->getOption("options.ui.notifications.bounce-dock").toString()) );
}
