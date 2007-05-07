#include "opt_application.h"
#include "common.h"
#include "iconwidget.h"

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <q3groupbox.h>
#include <QList>

#include "ui_opt_application.h"

class OptApplicationUI : public QWidget, public Ui::OptApplication
{
public:
	OptApplicationUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabApplication
//----------------------------------------------------------------------------

OptionsTabApplication::OptionsTabApplication(QObject *parent)
: OptionsTab(parent, "application", "", tr("Application"), tr("General application options"), "psi/logo_16")
{
	w = 0;
}

OptionsTabApplication::~OptionsTabApplication()
{
}

QWidget *OptionsTabApplication::widget()
{
	if ( w )
		return 0;

	w = new OptApplicationUI();
	OptApplicationUI *d = (OptApplicationUI *)w;

	d->ck_alwaysOnTop->setWhatsThis(
		tr("Makes the main Psi window always be in front of other windows."));
	d->ck_autoRosterSize->setWhatsThis(
		tr("Makes the main Psi window resize automatically to fit all contacts."));
	d->ck_keepSizes->setWhatsThis(
		tr("Makes Psi remember window size and positions for chats and messages."
		"  If disabled, the windows will always appear in their default positions and sizes."));
	d->ck_useleft->setWhatsThis(
		tr("Normally, right-clicking with the mouse on a contact will activate the context-menu."
		"  Check this option if you'd rather use a left-click."));
	d->ck_showMenubar->setWhatsThis(
		tr("Shows the menubar in the application window."));

	// docklet
	d->ck_docklet->setWhatsThis(
		tr("Makes Psi use a docklet icon, also known as system tray icon."));
	d->ck_dockDCstyle->setWhatsThis(
		tr("Normally, single-clicking on the Psi docklet icon brings the main window to"
		" the foreground.  Check this option if you would rather use a double-click."));
	d->ck_dockHideMW->setWhatsThis(
		tr("Starts Psi with only the docklet icon visible."));
	d->ck_dockToolMW->setWhatsThis(
		tr("Prevents Psi from taking up a slot on the taskbar and makes the main "
		"window use a small titlebar."));

#ifdef Q_WS_MAC
	d->ck_alwaysOnTop->hide();
	d->ck_showMenubar->hide();
	d->gb_docklet->hide();
#endif

	return w;
}

void OptionsTabApplication::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	opt->alwaysOnTop = d->ck_alwaysOnTop->isChecked();
	opt->autoRosterSize = d->ck_autoRosterSize->isChecked();
	opt->keepSizes   = d->ck_keepSizes->isChecked();
	opt->useleft = d->ck_useleft->isChecked();
	opt->hideMenubar = !d->ck_showMenubar->isChecked();

	// docklet
	opt->useDock = d->ck_docklet->isChecked();
	opt->dockDCstyle = d->ck_dockDCstyle->isChecked();
	opt->dockHideMW = d->ck_dockHideMW->isChecked();
	opt->dockToolMW = d->ck_dockToolMW->isChecked();

	// data transfer
	opt->dtPort = d->le_dtPort->text().toInt();
	opt->dtExternal = d->le_dtExternal->text().trimmed();
}

void OptionsTabApplication::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	d->ck_alwaysOnTop->setChecked( opt->alwaysOnTop );
	d->ck_autoRosterSize->setChecked( opt->autoRosterSize );
	d->ck_keepSizes->setChecked( opt->keepSizes );
	d->ck_showMenubar->setChecked( !opt->hideMenubar );
	d->ck_useleft->setChecked( opt->useleft );

	// docklet
	d->ck_docklet->setChecked( opt->useDock );
	d->ck_dockDCstyle->setChecked( opt->dockDCstyle );
	d->ck_dockHideMW->setChecked( opt->dockHideMW );
	d->ck_dockToolMW->setChecked( opt->dockToolMW );

	// data transfer
	d->le_dtPort->setText( QString::number(opt->dtPort) );
	d->le_dtExternal->setText( opt->dtExternal );
}
