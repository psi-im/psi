#include "opt_lookfeel.h"
#include "common.h"
#include "iconwidget.h"
#include "mainwin.h"
#include "psicon.h"
#include "psitoolbar.h"

//----------------------------------------------------------------------------
// OptionsTabLookFeelToolbars
//----------------------------------------------------------------------------

OptionsTabLookFeelToolbars::OptionsTabLookFeelToolbars(QObject *parent)
: OptionsTab(parent, "", "", "", "")
{
}

QWidget *OptionsTabLookFeelToolbars::widget()
{
	return 0;
}

void OptionsTabLookFeelToolbars::applyOptions(Options *o)
{
	o->toolbars = option.toolbars;

	// get current toolbars' positions
	MainWin *mainWin = (MainWin *)psi->mainWin();
	for (int i = 0; i < o->toolbars.count() && i < mainWin->toolbars.count(); i++) {
		//if ( toolbarPositionInProgress && posTbDlg->n() == (int)i )
		//	continue;

		Options::ToolbarPrefs &tbPref = o->toolbars["mainWin"][i];
		mainWin->getLocation ( mainWin->toolbars.at(i), tbPref.dock, tbPref.index, tbPref.nl, tbPref.extraOffset );
	}
}

void OptionsTabLookFeelToolbars::setData(PsiCon *p, QWidget *)
{
	psi = p;
}
