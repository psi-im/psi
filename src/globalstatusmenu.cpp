#include "globalstatusmenu.h"

#include "psiactionlist.h"

void GlobalStatusMenu::addChoose()
{
	ActionList actions = psi->actionList()->suitableActions(PsiActionList::Actions_MainWin);
	IconAction* action = actions.action( "choose_status" );

	if ( !action ) {
		qWarning("GlobalStatusMenu::addChoose(): action choose_status not found!");
	}

	addAction(static_cast<QAction*>(action));
}

void GlobalStatusMenu::addReconnect()
{
	ActionList actions = psi->actionList()->suitableActions(PsiActionList::Actions_MainWin);
	IconAction* action = actions.action( "reconnect_all" );
	if ( !action ) {
		qWarning("GlobalStatusMenu::addReconnect(): action reconnect_all not found!");
	}
	addAction(static_cast<QAction*>(action));
}

void GlobalStatusMenu::fill()
{
	StatusMenu::fill();
	//TODO: Find another way to prevent manual toggling
	foreach (QAction* action, statusActs)
		connect(action, SIGNAL(triggered(bool)), SLOT(preventStateChange(bool)));
	foreach (QAction* action, presetActs)
		connect(action, SIGNAL(triggered(bool)), SLOT(preventStateChange(bool)));
	statusChanged(makeStatus(psi->currentStatusType(), psi->currentStatusMessage()));
}

void GlobalStatusMenu::preventStateChange(bool checked)
{
	QAction* action = static_cast<QAction*>(sender());
	action->setChecked(!checked);
}
