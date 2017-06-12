#include "opt_plugins.h"
#include "ui_opt_plugins.h"

#include <QWhatsThis>
#include <QToolButton>
#include <QHeaderView>
#include <QSignalMapper>

#include "common.h"
#include "iconwidget.h"
#include "pluginmanager.h"
#include "psioptions.h"
#include "psiiconset.h"
#include "optionsdlgbase.h"
#include "psicon.h"

class OptPluginsUI : public QWidget, public Ui::OptPlugins
{
public:
	OptPluginsUI() : QWidget() { setupUi(this); }
};


class OptionsTabPlugin : public OptionsTab
{
	Q_OBJECT

	QString pluginName;
public:
	OptionsTabPlugin(const QString &pluginName, QObject *parent) :
	    OptionsTab(parent, "general", "", tr("General"), tr("General plugin options"), "psi/options"),
	    pluginName(pluginName) {}
	QWidget *widget() { return PluginManager::instance()->optionsWidget( pluginName ); }
	void applyOptions() { PluginManager::instance()->applyOptions( pluginName ); }
	void restoreOptions() { PluginManager::instance()->restoreOptions( pluginName ); }
};


class PluginsOptionsDlg : public OptionsDlgBase
{
	Q_OBJECT
public:
	PluginsOptionsDlg(const QString &pluginName, PsiCon *psi, QWidget *parent = 0) :
	    OptionsDlgBase(psi, parent)
	{
		setWindowTitle(QString("%1: %2").arg(pluginName).arg(windowTitle()));
		QIcon icon = PluginManager::instance()->icon(pluginName);
		if (icon.isNull()) {
			icon = IconsetFactory::iconPtr("psi/options")->icon();
		}
		setWindowIcon(icon);
		setTabs(QList<OptionsTab*>() << new OptionsTabPlugin(pluginName, this));

		psi->dialogRegister(this);
		resize(440, 300);

		openTab( "general" );
	}

};

//----------------------------------------------------------------------------
// OptionsTabPlugins
//----------------------------------------------------------------------------

OptionsTabPlugins::OptionsTabPlugins(QObject *parent)
	: OptionsTab(parent, "plugins", "", tr("Plugins"), tr("Options for Psi plugins"), "psi/plugins")
	, w(0)
{
}

OptionsTabPlugins::~OptionsTabPlugins()
{
	if( infoDialog )
		delete(infoDialog);
}

void OptionsTabPlugins::setData(PsiCon *psi, QWidget *)
{
	this->psi = psi;
}

QWidget *OptionsTabPlugins::widget()
{
	if ( w )
		return 0;

	w = new OptPluginsUI();
	OptPluginsUI *d = (OptPluginsUI *)w;

	listPlugins();
	connect(d->tw_Plugins, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(itemChanged(QTreeWidgetItem*,int)));

	return w;
}

void OptionsTabPlugins::applyOptions()
{
}

void OptionsTabPlugins::restoreOptions()
{
}

bool OptionsTabPlugins::stretchable() const
{
	return true;
}


void OptionsTabPlugins::listPlugins()
{
	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;

	d->tw_Plugins->clear();

	PluginManager *pm=PluginManager::instance();

	QStringList plugins = pm->availablePlugins();
	plugins.sort();
	const QSize buttonSize = QSize(21,21);
	QSignalMapper *info = new QSignalMapper(d);
	QSignalMapper *settings = new QSignalMapper(d);
	connect(info, SIGNAL(mapped(int)), this, SLOT(showPluginInfo(int)));
	connect(settings, SIGNAL(mapped(int)), this, SLOT(settingsClicked(int)));
	foreach ( const QString& plugin, plugins ){
		QIcon icon = pm->icon(plugin);
		bool enabled = pm->isEnabled(plugin);
		const QString path = pm->pathToPlugin(plugin);
		QString toolTip = tr("Plugin Path:\n%1").arg(path);
		Qt::CheckState state = enabled ? Qt::Checked : Qt::Unchecked;
		QTreeWidgetItem *item = new QTreeWidgetItem(d->tw_Plugins, QTreeWidgetItem::Type);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setText(C_NAME, plugin);
		item->setText(C_VERSION, pm->version(plugin));
		item->setTextAlignment(C_VERSION, Qt::AlignHCenter);
		item->setToolTip(C_NAME, toolTip);
		item->setCheckState(C_NAME, state);
		if ( !enabled ) {
			icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
		}
		item->setIcon(C_NAME,icon);
		QString shortName = PluginManager::instance()->shortName(plugin);
		const int index = d->tw_Plugins->indexOfTopLevelItem(item);

		QToolButton *aboutbutton = new QToolButton(d->tw_Plugins);
		aboutbutton->setIcon(QIcon(IconsetFactory::iconPixmap("psi/info")));
		aboutbutton->resize(buttonSize);
		aboutbutton->setObjectName("ab_" + shortName);
		aboutbutton->setToolTip(tr("Show information about plugin"));
		d->tw_Plugins->setItemWidget(item, C_ABOUT, aboutbutton);
		connect(aboutbutton, SIGNAL(clicked(bool)), info, SLOT(map()));
		info->setMapping(aboutbutton, index);

		QToolButton *settsbutton = new QToolButton(d->tw_Plugins);
		settsbutton->setIcon(QIcon(IconsetFactory::iconPixmap("psi/options")));
		settsbutton->resize(buttonSize);
		settsbutton->setObjectName("sb_" + shortName);
		settsbutton->setToolTip(tr("Open plugin settings dialog"));
		settsbutton->setEnabled(enabled);
		d->tw_Plugins->setItemWidget(item, C_SETTS, settsbutton);
		connect(settsbutton, SIGNAL(clicked(bool)), settings, SLOT(map()));
		settings->setMapping(settsbutton, index);
	}
	if ( d->tw_Plugins->topLevelItemCount() > 0 ) {
#ifdef HAVE_QT5
		d->tw_Plugins->header()->setSectionResizeMode(C_NAME, QHeaderView::Stretch);
#else
		d->tw_Plugins->header()->setResizeMode(C_NAME, QHeaderView::Stretch);
#endif
		d->tw_Plugins->resizeColumnToContents(C_VERSION);
		d->tw_Plugins->resizeColumnToContents(C_ABOUT);
		d->tw_Plugins->resizeColumnToContents(C_SETTS);
	}
}

void OptionsTabPlugins::itemChanged(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);
	if ( !w )
		return;
	OptPluginsUI *d = (OptPluginsUI *)w;
	bool enabled = item->checkState(C_NAME) == Qt::Checked;
	d->tw_Plugins->setCurrentItem(item);

	PluginManager *pm = PluginManager::instance();
	QString name = item->text(C_NAME);
	QString option=QString("%1.%2")
		.arg(PluginManager::loadOptionPrefix)
		.arg(pm->shortName(name));
	PsiOptions::instance()->setOption(option, enabled);

	d->tw_Plugins->blockSignals(true); //Block signalls to change item elements
	d->tw_Plugins->itemWidget(item, C_SETTS)->setEnabled(enabled);
	QIcon icon = pm->icon(name);
	if ( !enabled ) {
		icon = QIcon(icon.pixmap(icon.availableSizes().at(0), QIcon::Disabled));
	}
	item->setIcon(C_NAME, icon);
	d->tw_Plugins->blockSignals(false); //Release signals blocking
}

void OptionsTabPlugins::showPluginInfo(int item)
{
	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;
	d->tw_Plugins->setCurrentItem(d->tw_Plugins->topLevelItem(item));

	if ( d->tw_Plugins->selectedItems().size() > 0 ) {
		if( infoDialog )
			delete(infoDialog);

		infoDialog = new QDialog(d);
		ui_.setupUi(infoDialog);
		QString name = d->tw_Plugins->currentItem()->text(C_NAME);
		infoDialog->setWindowTitle(QString("%1 %2").arg(infoDialog->windowTitle()).arg(name));
		infoDialog->setWindowIcon(QIcon(IconsetFactory::iconPixmap("psi/logo_128")));
		ui_.te_info->setText(PluginManager::instance()->pluginInfo(name));
		infoDialog->setAttribute(Qt::WA_DeleteOnClose);
		infoDialog->show();
	}
}

void OptionsTabPlugins::settingsClicked(int item)
{
	if ( !w )
		return;

	OptPluginsUI *d = (OptPluginsUI *)w;
	d->tw_Plugins->setCurrentItem(d->tw_Plugins->topLevelItem(item));

	if ( d->tw_Plugins->selectedItems().size() > 0 ) {
		const QString pluginName = d->tw_Plugins->currentItem()->text(C_NAME);
		const QString shortName = PluginManager::instance()->shortName(pluginName);
		PluginsOptionsDlg *sw = d->findChild<PluginsOptionsDlg*>(shortName);
		if (!sw) {
			sw = new PluginsOptionsDlg(pluginName, psi, d);
			sw->setObjectName(shortName);
		}
		bringToFront(sw);
	}
}

#include "opt_plugins.moc"
