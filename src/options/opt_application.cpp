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
	d->ck_hideMenubar->setWhatsThis(
		tr("Hides the menubar in the application window."));

	// links
	d->cb_link->setWhatsThis(
		tr("Selects what applications to use for handling URLs and e-mail addresses."
		"  You can choose between the system default and custom applications."));

	QString s = tr("Enter the path to the application's executable and choose \"Custom\" in the list above.");
	d->le_linkBrowser->setWhatsThis(
		tr("Specify what custom browser application to use for handling URLs here.") + "  " + s);
	d->le_linkMailer->setWhatsThis(
		tr("Specify what custom mailer application to use for handling e-mail addresses here.") + "  " + s);

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
	d->ck_dockUseWM->setWhatsThis(
		tr("If checked, Psi will use the Window Maker docklet instead of FreeDesktop one."));

#ifdef Q_WS_MAC
	d->ck_alwaysOnTop->hide();
	d->ck_hideMenubar->hide();
	d->gb_links->hide();
	d->gb_docklet->hide();
#endif
#ifndef Q_WS_X11
	d->ck_dockUseWM->hide();
#endif

	return w;
}

#ifdef Q_WS_X11
static int om_x11browse[] = { 0, 2, 1 };
#endif

void OptionsTabApplication::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	opt->alwaysOnTop = d->ck_alwaysOnTop->isChecked();
	opt->autoRosterSize = d->ck_autoRosterSize->isChecked();
	opt->keepSizes   = d->ck_keepSizes->isChecked();
	opt->useleft = d->ck_useleft->isChecked();
	opt->hideMenubar = d->ck_hideMenubar->isChecked();

	// links
#ifdef Q_WS_X11
	opt->browser = om_x11browse[ d->cb_link->currentItem() ];
#else
	opt->browser = d->cb_link->currentItem();
#endif
	opt->customBrowser = d->le_linkBrowser->text();
	opt->customMailer  = d->le_linkMailer->text();

	// docklet
	opt->useDock = d->ck_docklet->isChecked();
	opt->dockDCstyle = d->ck_dockDCstyle->isChecked();
	opt->dockHideMW = d->ck_dockHideMW->isChecked();
	opt->dockToolMW = d->ck_dockToolMW->isChecked();
	opt->isWMDock  = d->ck_dockUseWM->isChecked();

	// data transfer
	opt->dtPort = d->le_dtPort->text().toInt();
	opt->dtExternal = d->le_dtExternal->text();
}

void OptionsTabApplication::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	d->ck_alwaysOnTop->setChecked( opt->alwaysOnTop );
	d->ck_autoRosterSize->setChecked( opt->autoRosterSize );
	d->ck_keepSizes->setChecked( opt->keepSizes );
	d->ck_hideMenubar->setChecked( opt->hideMenubar );
	d->ck_useleft->setChecked( opt->useleft );

	// links
	connect(d->cb_link, SIGNAL(activated(int)), SLOT(selectBrowser(int)));
#ifdef Q_WS_WIN
	d->cb_link->insertItem(tr("Windows Default Browser/Mail"));
	d->cb_link->insertItem(tr("Custom"));
	d->cb_link->setCurrentItem( opt->browser );
	selectBrowser( opt->browser );
#endif
#ifdef Q_WS_X11
	d->cb_link->insertItem(tr("KDE Default Browser/Mail"));
	d->cb_link->insertItem(tr("GNOME2 Default Browser/Mail"));
	d->cb_link->insertItem(tr("Custom"));
	int rbi = om_x11browse[ opt->browser ];
	d->cb_link->setCurrentItem( rbi );
	selectBrowser( rbi );
#endif
#ifdef Q_WS_MAC
	d->cb_link->insertItem(tr("Mac OS Default Browser/Mail"));
	d->cb_link->setCurrentItem( opt->browser );
	selectBrowser( opt->browser );
#endif
	d->le_linkBrowser->setText( opt->customBrowser );
	d->le_linkMailer->setText( opt->customMailer );

	// docklet
	d->ck_docklet->setChecked( opt->useDock );
	d->ck_dockDCstyle->setChecked( opt->dockDCstyle );
	d->ck_dockHideMW->setChecked( opt->dockHideMW );
	d->ck_dockToolMW->setChecked( opt->dockToolMW );
	d->ck_dockUseWM->setChecked( opt->isWMDock );

	// data transfer
	d->le_dtPort->setText( QString::number(opt->dtPort) );
	d->le_dtExternal->setText( opt->dtExternal );
}

void OptionsTabApplication::selectBrowser(int x)
{
	if ( !w )
		return;

	bool enableCustom = TRUE;

#ifdef Q_WS_WIN
	if(x == 0)
		enableCustom = FALSE;
#endif
#ifdef Q_WS_X11
	if(x == 0 || x == 1)
		enableCustom = FALSE;
#endif
#ifdef Q_WS_MAC
	if(x == 0)
		enableCustom = FALSE;
#endif

	OptApplicationUI *d = (OptApplicationUI *)w;
	d->gb_linkCustom->setEnabled(enableCustom);
}
