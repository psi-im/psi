#include "optionsdlg.h"
#include "psicon.h"
#include "../avcall/avcall.h"
#include "iconset.h"

// tabs
#include "opt_toolbars.h"
#include "opt_application.h"
#include "opt_roster.h"
#include "opt_appearance.h"
#include "opt_chat.h"
#include "opt_events.h"
#include "opt_popups.h"
#include "opt_status.h"
#include "opt_iconset.h"
#include "opt_groupchat.h"
#include "opt_sound.h"
#include "opt_avcall.h"
#include "opt_advanced.h"
#include "opt_shortcuts.h"
#include "opt_tree.h"

#ifdef PSI_PLUGINS
#include "opt_plugins.h"
#endif

OptionsDlg::OptionsDlg(PsiCon *psi, QWidget *parent) :
	OptionsDlgBase(psi, parent)
{
	setWindowTitle(CAP(windowTitle()));
	setWindowIcon(IconsetFactory::icon("psi/options").icon());

	QList<OptionsTab*> tabs;

	// tabs - base
	/*tabs.append( new OptionsTabGeneral(this) );
	//tabs.append( new OptionsTabBase(this, "general",  "", "psi/logo_16",	tr("General"),		tr("General preferences list")) );
	tabs.append( new OptionsTabEvents(this) );
	//tabs.append( new OptionsTabBase(this, "events",   "", "psi/system",	tr("Events"),		tr("Change the events behaviour")) );
	tabs.append( new OptionsTabPresence(this) );
	//tabs.append( new OptionsTabBase(this, "presence", "", "status/online",	tr("Presence"),		tr("Presence configuration")) );
	tabs.append( new OptionsTabLookFeel(this) );
	tabs.append( new OptionsTabIconset(this) );
	//tabs.append( new OptionsTabBase(this, "lookfeel", "", "psi/smile",	tr("Look and Feel"),	tr("Change the Psi's Look and Feel")) );
	tabs.append( new OptionsTabSound(this) );
	//tabs.append( new OptionsTabBase(this, "sound",    "", "psi/playSounds",	tr("Sound"),		tr("Configure how Psi sounds")) );
	*/

	OptionsTabApplication* applicationTab = new OptionsTabApplication(this);
	applicationTab->setHaveAutoUpdater(psi->haveAutoUpdater());
	tabs.append( applicationTab );
	tabs.append( new OptionsTabRoster(this) );
	tabs.append( new OptionsTabChat(this) );
	tabs.append( new OptionsTabEvents(this) );
	tabs.append( new OptionsTabPopups(this) );
	tabs.append( new OptionsTabStatus(this) );
	tabs.append( new OptionsTabAppearance(this) );
	//tabs.append( new OptionsTabIconsetSystem(this) );
	//tabs.append( new OptionsTabIconsetRoster(this) );
	//tabs.append( new OptionsTabIconsetEmoticons(this) );
	tabs.append( new OptionsTabGroupchat(this) );
	tabs.append( new OptionsTabSound(this) );
	if(AvCallManager::isSupported())
		tabs.append( new OptionsTabAvCall(this) );
	tabs.append( new OptionsTabToolbars(this) );
#ifdef PSI_PLUGINS
	tabs.append( new OptionsTabPlugins(this) );
#endif
	tabs.append( new OptionsTabShortcuts(this) );
	tabs.append( new OptionsTabAdvanced(this) );
	tabs.append( new OptionsTabTree(this) );

	// tabs - general
	/*tabs.append( new OptionsTabGeneralRoster(this) );
	tabs.append( new OptionsTabGeneralDocking(this) );
	tabs.append( new OptionsTabGeneralNotifications(this) );
	tabs.append( new OptionsTabGeneralGroupchat(this) );
	tabs.append( new OptionsTabGeneralMisc(this) );*/

	// tabs - events
	/*tabs.append( new OptionsTabEventsReceive(this) );
	tabs.append( new OptionsTabEventsMisc(this) );*/

	// tabs - presence
	/*tabs.append( new OptionsTabPresenceAuto(this) );
	tabs.append( new OptionsTabPresencePresets(this) );
	tabs.append( new OptionsTabPresenceMisc(this) );*/

	// tabs - look and feel
	/*tabs.append( new OptionsTabLookFeelColors(this) );
	tabs.append( new OptionsTabLookFeelFonts(this) );
	tabs.append( new OptionsTabIconsetSystem(this) );
	tabs.append( new OptionsTabIconsetEmoticons(this) );
	tabs.append( new OptionsTabIconsetRoster(this) );
	tabs.append( new OptionsTabLookFeelToolbars(this) );
	tabs.append( new OptionsTabLookFeelMisc(this) );*/

	// tabs - sound
	/*tabs.append( new OptionsTabSoundPrefs(this) );
	tabs.append( new OptionsTabSoundEvents(this) );*/

	setTabs(tabs);

	psi->dialogRegister(this);
	resize(640, 480);

	openTab( "application" );
}
