#include "opt_roster.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"


#include "ui_opt_roster.h"

class OptRosterUI : public QWidget, public Ui::OptRoster
{
public:
	OptRosterUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabRoster
//----------------------------------------------------------------------------

OptionsTabRoster::OptionsTabRoster(QObject *parent)
	: OptionsTab(parent, "roster", "", tr("Roster"), tr("Roster window options"), "psi/roster_icon")
	, w(0)
{
}

OptionsTabRoster::~OptionsTabRoster()
{
}

QWidget *OptionsTabRoster::widget()
{
	if ( w )
		return 0;

	w = new OptRosterUI();
	OptRosterUI *d = (OptRosterUI *)w;

	d->ck_alwaysOnTop->setWhatsThis(
		tr("Makes the main Psi window always be in front of other windows."));
	d->ck_autoRosterSize->setWhatsThis(
		tr("Makes the main Psi window resize automatically to fit all contacts."));
	d->ck_useleft->setWhatsThis(
		tr("Normally, right-clicking with the mouse on a contact will activate the context-menu."
		"  Check this option if you'd rather use a left-click."));
	d->ck_showMenubar->setWhatsThis(
		tr("Shows the menubar in the application window."));

	connect(d->ck_showClientIcons, SIGNAL(toggled(bool)), d->cb_showAllClientIcons, SLOT(setEnabled(bool)));
#ifdef Q_OS_MAC
	d->ck_alwaysOnTop->hide();
	d->ck_showMenubar->hide();
#endif

	return w;
}

void OptionsTabRoster::applyOptions()
{
	if ( !w )
		return;

	OptRosterUI *d = (OptRosterUI *)w;

	PsiOptions::instance()->setOption("options.ui.contactlist.always-on-top", d->ck_alwaysOnTop->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.automatically-resize-roster", d->ck_autoRosterSize->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.use-left-click", d->ck_useleft->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-menubar", d->ck_showMenubar->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.disable-scrollbar", d->ck_disableScrollbar->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-avatar-frame", d->ck_roster_avatar->isChecked());
	PsiOptions::instance()->setOption("options.contactlist.autohide-interval", d->sb_hideInterval->value());

	//enabled icons
	PsiOptions::instance()->setOption("options.ui.contactlist.avatars.show", d->ck_showAvatarIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-mood-icons", d->ck_showMoodIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-activity-icons", d->ck_showActivityIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-geolocation-icons", d->ck_showGeoLocationIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-tune-icons", d->ck_showTuneIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-client-icons", d->ck_showClientIcons->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-all-client-icons", (d->cb_showAllClientIcons->currentIndex() == 0)?true:false);

	//avatars settings
	PsiOptions::instance()->setOption("options.ui.contactlist.avatars.size", d->sb_avatarsSize->value());
	PsiOptions::instance()->setOption("options.ui.contactlist.avatars.radius", d->sb_avatarsRadius->value());
}

void OptionsTabRoster::restoreOptions()
{
	if ( !w )
		return;

	OptRosterUI *d = (OptRosterUI *)w;

	d->ck_alwaysOnTop->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool() );
	d->ck_autoRosterSize->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.automatically-resize-roster").toBool() );
	d->ck_useleft->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.use-left-click").toBool() );
	d->ck_useleft->setVisible(false);
	d->ck_showMenubar->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool() );
	d->ck_disableScrollbar->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.disable-scrollbar").toBool() );
	d->ck_roster_avatar->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-avatar-frame").toBool() );
	d->sb_hideInterval->setValue( PsiOptions::instance()->getOption("options.contactlist.autohide-interval").toInt() );

	//enabled icons
	d->ck_showAvatarIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.avatars.show").toBool() );
	d->ck_showMoodIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-mood-icons").toBool() );
	d->ck_showActivityIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-activity-icons").toBool() );
	d->ck_showGeoLocationIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-geolocation-icons").toBool() );
	d->ck_showTuneIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-tune-icons").toBool() );
	d->ck_showClientIcons->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-client-icons").toBool() );
	d->cb_showAllClientIcons->setEnabled(PsiOptions::instance()->getOption("options.ui.contactlist.show-client-icons").toBool());
	d->cb_showAllClientIcons->setCurrentIndex(PsiOptions::instance()->getOption("options.ui.contactlist.show-all-client-icons").toBool()?0:1);
	//avatars settings
	d->sb_avatarsSize->setValue( PsiOptions::instance()->getOption("options.ui.contactlist.avatars.size").toInt() );
	d->sb_avatarsRadius->setValue( PsiOptions::instance()->getOption("options.ui.contactlist.avatars.radius").toInt() );
}
