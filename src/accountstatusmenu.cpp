#include "accountstatusmenu.h"

#include "psiiconset.h"
#include "psioptions.h"
#include "iconaction.h"
#include "common.h"
#include "statusdlg.h"

void AccountStatusMenu::addChoose()
{
	//Action will be automatically added to menu, because we set "this" as parent for it
	IconAction *action = new IconAction(tr("Choose status..."), "psi/action_direct_presence", tr("Choose..."), 0, this);
	connect(action, SIGNAL(triggered()), SLOT(chooseStatusActivated()));
}

void AccountStatusMenu::chooseStatusActivated()
{
	XMPP::Status::Type lastStatus = XMPP::Status::txt2type(PsiOptions::instance()->getOption("options.status.last-status").toString());
	StatusSetDlg *w = new StatusSetDlg(account, makeLastStatus(lastStatus), lastPriorityNotEmpty());
	connect(w, SIGNAL(set(const XMPP::Status &, bool, bool)), account, SLOT(setStatus(const XMPP::Status &, bool, bool)));
	w->show();
}

void AccountStatusMenu::addReconnect() {
	IconAction *action = new IconAction(tr("Reconnect"), "psi/reload", tr("Reconnect"), 0, this);
	connect(action, SIGNAL(triggered()), SIGNAL(reconnectActivated()));
}

void AccountStatusMenu::fill()
{
	StatusMenu::fill();
	statusChanged(account->status());
	addIgnoreGlobalActions();
}

void AccountStatusMenu::addIgnoreGlobalActions()
{
	QAction *blockAction = new IconAction(tr("Ignore global actions"), this, "psi/ignore_global_actions");
	blockAction->setCheckable(true);
	blockAction->setChecked(account->accountOptions().ignore_global_actions);
	blockAction->setToolTip(tr("Ignore all global actions for this account. For example, autostatus, mood, activity etc."));
	connect(blockAction, SIGNAL(triggered(bool)), account, SLOT(actionSetBlock(bool)));
	addSeparator();
	addAction(blockAction);

}

