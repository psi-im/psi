#include "opt_application.h"
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "proxy.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
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

	if (!haveAutoUpdater_) {
		d->ck_autoUpdate->hide();
	}

	//Proxy

	ProxyChooser *pc = ProxyManager::instance()->createProxyChooser(w);
	d->gb_proxy->layout()->addWidget(ProxyManager::instance()->proxyForObject()->getComboBox(pc, w));
	d->gb_proxy->layout()->addWidget(pc);


	connect(d->le_dtPort, SIGNAL(textChanged(QString)), this, SLOT(updatePortLabel()));

	return w;
}

void OptionsTabApplication::setHaveAutoUpdater(bool b) 
{
	haveAutoUpdater_ = b;
}

void OptionsTabApplication::applyOptions()
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	PsiOptions::instance()->setOption("options.ui.contactlist.always-on-top", d->ck_alwaysOnTop->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.automatically-resize-roster", d->ck_autoRosterSize->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.use-left-click", d->ck_useleft->isChecked());
	PsiOptions::instance()->setOption("options.ui.contactlist.show-menubar", d->ck_showMenubar->isChecked());

	// Auto-update
	PsiOptions::instance()->setOption("options.auto-update.check-on-startup", d->ck_autoUpdate->isChecked());
	
	// docklet
	PsiOptions::instance()->setOption("options.ui.systemtray.enable", d->ck_docklet->isChecked());
	PsiOptions::instance()->setOption("options.ui.systemtray.use-double-click", d->ck_dockDCstyle->isChecked());
	PsiOptions::instance()->setOption("options.contactlist.hide-on-start", d->ck_dockHideMW->isChecked());
	PsiOptions::instance()->setOption("options.contactlist.use-toolwindow", d->ck_dockToolMW->isChecked());

	// data transfer
	PsiOptions::instance()->setOption("options.p2p.bytestreams.listen-port", d->le_dtPort->text().toInt());
	PsiOptions::instance()->setOption("options.p2p.bytestreams.external-address", d->le_dtExternal->text().trimmed());

	//Proxy
	ProxyManager::instance()->proxyForObject()->save();
}

void OptionsTabApplication::restoreOptions()
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	d->ck_alwaysOnTop->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.always-on-top").toBool() );
	d->ck_autoRosterSize->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.automatically-resize-roster").toBool() );
	d->ck_showMenubar->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.show-menubar").toBool() );
	d->ck_useleft->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.use-left-click").toBool() );
	d->ck_useleft->setVisible(false); //currently useless
	d->ck_autoUpdate->setChecked(PsiOptions::instance()->getOption("options.auto-update.check-on-startup").toBool());

	// docklet
	d->ck_docklet->setChecked( PsiOptions::instance()->getOption("options.ui.systemtray.enable").toBool() );
	d->ck_dockDCstyle->setChecked( PsiOptions::instance()->getOption("options.ui.systemtray.use-double-click").toBool() );
	d->ck_dockHideMW->setChecked( PsiOptions::instance()->getOption("options.contactlist.hide-on-start").toBool() );
	d->ck_dockToolMW->setChecked( PsiOptions::instance()->getOption("options.contactlist.use-toolwindow").toBool() );

	// data transfer
	d->le_dtPort->setText( QString::number(PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt()) );
	d->le_dtExternal->setText( PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString() );
}

void OptionsTabApplication::updatePortLabel()
{
	if ( !w )
		return;

	OptApplicationUI *d = (OptApplicationUI *)w;

	if ( d->le_dtPort->text().isEmpty() ) {
		d->label->clear();
		return;
	}

	int port = d->le_dtPort->text().toInt();
	if ( port < 0 || port > 65532 ) {
		d->label->clear();
		return;
	}

	if ( port == 0 ) {
		d->label->setText(tr("(TCP: Disabled, UDP: Auto)"));
	}
	else {
		d->label->setText(tr("(TCP: %1, UDP: %1-%2)").arg( port ).arg( port + 3 ));
	}
}
