#include "opt_plugins.h"
#include "common.h"
#include "iconwidget.h"
#include "pluginmanager.h"
#include "psioptions.h"

#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

#include "ui_opt_plugins.h"

class OptPluginsUI : public QWidget, public Ui::OptPlugins
{
public:
	OptPluginsUI() : QWidget() { setupUi(this); }
};

//----------------------------------------------------------------------------
// OptionsTabPlugins
//----------------------------------------------------------------------------

OptionsTabPlugins::OptionsTabPlugins(QObject *parent)
: OptionsTab(parent, "plugins", "", tr("Plugins"), tr("Options for Psi plugins"), "psi/advanced")
{
	w = 0;
}

OptionsTabPlugins::~OptionsTabPlugins()
{
}

QWidget *OptionsTabPlugins::widget()
{
	if ( w )
		return 0;

	w = new OptPluginsUI();
	OptPluginsUI *d = (OptPluginsUI *)w;

	listPlugins();

	
	/*QWhatsThis::add(d->ck_messageevents,
		tr("Enables the sending and requesting of message events such as "
		"'Contact is Typing', ..."));*/

	connect(d->cb_plugins,SIGNAL(currentIndexChanged(int)),SLOT(pluginSelected(int)));
	connect(d->cb_loadPlugin,SIGNAL(stateChanged(int)),SLOT(loadToggled(int)));
	
	return w;
}

void OptionsTabPlugins::applyOptions(Options *opt)
{
	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;
	Q_UNUSED(d);
	Q_UNUSED(opt);
}

void OptionsTabPlugins::restoreOptions(const Options *opt)
{
	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;
	Q_UNUSED(opt);
	Q_UNUSED(d);
}

void OptionsTabPlugins::listPlugins()
{
  	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;

	d->cb_plugins->clear();
	
	PluginManager *pm=PluginManager::instance();
	
	QStringList plugins=pm->availablePlugins();
		foreach (QString plugin, plugins){
		d->cb_plugins->addItem(plugin);
	}
	pluginSelected(0);
}

void OptionsTabPlugins::loadToggled(int state)
{
	Q_UNUSED(state);
	if ( !w )
		return;
	
	OptPluginsUI *d = (OptPluginsUI *)w;
	
	QString option=QString("%1.%2")
		.arg(PluginManager::loadOptionPrefix)
		.arg(PluginManager::instance()->shortName(d->cb_plugins->currentText()));
	bool value=d->cb_loadPlugin->isChecked(); 
	PsiOptions::instance()->setOption(option, value);
}

void OptionsTabPlugins::pluginSelected(int index)
{
	Q_UNUSED(index);
  	if ( !w )
		return;
	
	OptPluginsUI *d = (OptPluginsUI *)w;
	d->le_location->setText(tr("No plugin selected."));
	d->cb_loadPlugin->setEnabled(false);
	delete d->pluginOptions;
	d->pluginOptions = new QLabel(tr("This plugin has no user configurable options"));

	if ( d->cb_plugins->count() > 0 ) {
		QString pluginName=d->cb_plugins->currentText();
		d->le_location->setText(PluginManager::instance()->pathToPlugin( pluginName ));
		d->cb_loadPlugin->setEnabled(true);
		QWidget* pluginOptions = PluginManager::instance()->optionsWidget( pluginName );
		d->cb_plugins->setEnabled(true);
	
		QString option=QString("%1.%2")
			.arg(PluginManager::loadOptionPrefix)
			.arg(PluginManager::instance()->shortName(pluginName));
		int value=PsiOptions::instance()->getOption(option).toBool();
		if (value)
			value=Qt::Checked;
		else
			value=Qt::Unchecked;
		d->cb_loadPlugin->setChecked(value);
	
		d->vboxLayout1->remove(d->pluginOptions);
		delete d->pluginOptions;
		d->pluginOptions=NULL;
		if (pluginOptions)
		{
			d->pluginOptions = pluginOptions;
			d->pluginOptions->setParent(d);
			qWarning("Showing Plugin options");
		}
		else
		{
			d->pluginOptions = new QLabel(tr("This plugin has no user configurable options"),d);
			qWarning("Plugin has no options");
		}
		
		d->vboxLayout1->addWidget(d->pluginOptions);
		//d->pluginOptions->show();
		//d->updateGeometry();
	}
}
