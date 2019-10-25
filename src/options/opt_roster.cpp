#include "opt_roster.h"

#include "opt_roster_main.h"
#include "opt_roster_muc.h"

OptionsTabRoster::OptionsTabRoster(QObject *parent) :
    MetaOptionsTab(parent, "roster", "", tr("Roster"), tr("Roster options"), "psi/roster_icon")
{
    addTab(new OptionsTabRosterMain(this));
    addTab(new OptionsTabRosterMuc(this));
}
