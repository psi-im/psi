#include "opt_status.h"
#include "opt_statusgeneral.h"
#include "opt_statusauto.h"

OptionsTabStatus::OptionsTabStatus(QObject *parent)
: MetaOptionsTab(parent, "status", "", tr("Status"), tr("Status preferences"), "psi/status")
{
	OptionsTabStatusGeneral* general = new OptionsTabStatusGeneral(this);
	addTab(general);
	connect(general, SIGNAL(enableDlgCommonWidgets(bool)), parent, SLOT(enableCommonWidgets(bool)));
	connect(general, SIGNAL(enableDlgCommonWidgets(bool)), SLOT(enableOtherTabs(bool)));
	addTab(new OptionsTabStatusAuto(this));
}
