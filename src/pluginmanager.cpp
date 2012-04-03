/*
 * (c) 2006 Kevin Smith
 * (c) 2008 Maciej Niedzielski
 */

#include "pluginmanager.h"

#include <QtCore>
#include <QtCrypto>
#include <QPluginLoader>
#include <QDebug>

#include "xmpp_client.h"
#include "xmpp_task.h"
#include "xmpp_message.h"

#include "applicationinfo.h"
#include "psioptions.h"

#include "pluginhost.h"
#include "psiplugin.h"
#include "psiaccount.h"
#include "stanzafilter.h"
#include "stanzasender.h"
#include "iqfilter.h"
#include "iqnamespacefilter.h"
#include "eventfilter.h"
#include "optionaccessor.h"


//TODO(mck)
// - make sure PluginManager works correctly when changing profiles
// - use native separators when displaying file path


/**
 * Helper class used to process incoming XML in plugins.
 */
class PluginManager::StreamWatcher: public XMPP::Task
{
public:
	StreamWatcher(Task* t, PluginManager* m, int a) : Task(t), manager(m), account(a) {}
	bool take(const QDomElement& e) {
		return manager->incomingXml(account, e);
	}
	PluginManager* manager;
	int account;
};


/**
 * Function to obtain all the directories in which plugins can be stored
 * \return List of plugin directories
 */ 
static QStringList pluginDirs()
{
	QStringList l;
	l += ApplicationInfo::resourcesDir() + "/plugins";
	l += ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + "/plugins";
#if defined(Q_OS_UNIX)
	l += ApplicationInfo::libDir() + "/plugins";
#endif
	return l;
}

/**
 * Method for accessing the singleton instance of the class.
 * Instanciates if no instance yet exists.
 * \return Pointer to the plugin manager.
 */
PluginManager* PluginManager::instance()
{
	if (!instance_) {
		instance_ = new PluginManager();
	}
	return instance_;
}

/**
 * Default constructor. Locates all plugins, sets watchers on those directories to 
 * locate new ones and loads those enabled in the config.
 */ 
PluginManager::PluginManager() : QObject(NULL)
{
	updatePluginsList();
	loadEnabledPlugins();
	foreach (QString path, pluginDirs()) {
		QCA::DirWatch *dw = new QCA::DirWatch(path, this);
		connect(dw, SIGNAL(changed()), SLOT(dirsChanged()));
		dirWatchers_.append(dw);
	}
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), this, SLOT(optionChanged(const QString&)));
}

/**
 * Updates list of known plugins by reading all plugin directories
 */
void PluginManager::updatePluginsList()
{
	foreach (QString d, pluginDirs()) {
		QDir dir(d);
		foreach (QString file, dir.entryList()) {
			file = dir.absoluteFilePath(file);
			qWarning("Found plugin: %s", qPrintable(file));
			if (!pluginByFile_.contains(file)) {
				PluginHost* host = new PluginHost(this, file);
				if (host->isValid()) {
					hosts_[host->name()] = host;
					pluginByFile_[file] = host;
				}
			} else {
				qWarning("Which we already knew about");
			}
		}
	}
}

/**
 * This slot is executed when the contents of a plugin directory changes
 * It causes the available plugin list to be refreshed.
 *
 * TODO: it should also load the plugins if they're on the autoload list
 */ 
void PluginManager::dirsChanged()
{
	updatePluginsList();
}

/**
 * This causes all plugins that are both set for auto-load, and available
 * to be loaded. 
 */
void PluginManager::loadEnabledPlugins()
{
	qDebug("Loading enabled plugins");
	foreach (PluginHost* plugin, hosts_.values()) {
		QString option = QString("%1.%2").arg(loadOptionPrefix).arg(plugin->shortName());
		if (PsiOptions::instance()->getOption(option).toBool()) {
			qWarning("Plugin %s is enabled in config: loading", qPrintable(plugin->shortName()));
			plugin->load();
			plugin->enable();
		}
	}
}

/**
 * Called when an option changes to load or unload a plugin if it's a plugin 
 * option
 * \param option Option changed
 */
void PluginManager::optionChanged(const QString& option)
{
	//QString("%1.%2").arg(loadOptionPrefix).arg(shortNames_[plugin]);

	//TODO(mck): implement this... for now, enabling/disabling requires psi restart
}

/**
 * Loads all available plugins
 */ 
void PluginManager::loadAllPlugins()
{
	qDebug("Loading all plugins");
  	//Any static (compiled in) plugins we happen to have
	/*foreach( QObject* plugin, QPluginLoader::staticInstances() ) {
		loadPlugin( plugin );
	}*/

	//Now look for external plugins
	foreach (PluginHost* plugin, hosts_.values()) {
		plugin->load();
		plugin->enable();
	}
}

/**
 * Unloads all Psi plugins. 
 * \return Success of unloading all plugins; if any plugins couldn't be 
 *         unloaded, false.
 */ 
bool PluginManager::unloadAllPlugins()
{
	qDebug("Unloading all plugins");
	bool ok = true;
	foreach (PluginHost* plugin, hosts_.values()) {
		if (!plugin->disable()) {
			ok = false;
		} else if (plugin->unload()) {
			ok = false;
		}
	}
	return ok;
}

/**
 * Find the file which provides the named plugin. If the named plugin is not
 * known, an empty string is provided.
 * \param plugin Name of the plugin.
 * \return Path to the plugin file.
 */ 
QString PluginManager::pathToPlugin(const QString& plugin)
{
	QString path;
	if (hosts_.contains(plugin)) {
		path = hosts_[plugin]->path();
	}
	return path;
}

/**
 * Returns short name of the named plugin. If the named plugin is not
 * known, an empty string is provided.
 * \param plugin Name of the plugin.
 * \return Path to the plugin file.
 */ 
QString PluginManager::shortName(const QString& plugin)
{
	QString name;
	if (hosts_.contains(plugin)) {
		name = hosts_[plugin]->shortName();
	}
	return name;
}

/**
 * Returns a list of available plugin names found in all plugin directories.
 */
QStringList PluginManager::availablePlugins()
{
	return hosts_.keys();
}

/**
 * Provides a pointer to a QWidget providing the options dialog for the 
 * named plugin, if it exists, else NULL.
 * \param plugin Name of the plugin.
 * \return Pointer to the options widget for the named plugin.
 */
QWidget* PluginManager::optionsWidget(const QString& plugin)
{
	QWidget* widget = 0;
	if (hosts_.contains(plugin)) {
		widget = hosts_[plugin]->optionsWidget();
	} else {
		qWarning("Attempting to get options for %s which doesn't exist", qPrintable(plugin));
	}
	return widget;
}

/**
 * \brief Give each plugin the opportunity to process the incoming message event
 *
 * Each plugin is passed the event in turn. Any plugin may then modify the event
 * and may cause the event to be silently discarded.
 *
 * \param account Pointer to the PsiAccount responsible
 * \param event Incoming event
 * \return Continue processing the event; true if the event should be silently discarded.
 */
bool PluginManager::processMessage(const PsiAccount* account, const QString& jidFrom, const QString& body, const QString& subject)
{
	bool handled = false;
	foreach (PluginHost* host, hosts_.values()) {
		if (host->processMessage(accountIds_[account], jidFrom, body, subject)) {
			handled = true;
			break;
		}
	}
	return handled;
}

/**
 * \brief Give each plugin the opportunity to process the incoming event
 * 
 * Each plugin is passed the event in turn. Any plugin may then modify the event
 * and may cause the event to be silently discarded.
 * 
 * \param account Pointer to the PsiAccount responsible
 * \param event Incoming event
 * \return Continue processing the event; true if the event should be silently discarded.
 */
bool PluginManager::processEvent(const PsiAccount* account, QDomElement& event)
{
	bool handled = false;
	foreach (PluginHost* host, hosts_.values()) {
		if (host->processEvent(accountIds_[account], event)) {
			handled = true;
			break;
		}
	}
	return handled;
}

/**
 * \brief Give each plugin the opportunity to process the incoming xml
 *
 * Each plugin is passed the xml in turn using various filter interfaces
 * (for example, see StanzaFilter or IqFilter).
 * Any plugin may then modify the xml and may cause the stanza to be
 * silently discarded.
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param xml Incoming XML
 * \return Continue processing the event; true if the event should be silently discarded.
 */
bool PluginManager::incomingXml(int account, const QDomElement &xml)
{
	bool handled = false;
	foreach (PluginHost* host, hosts_.values()) {
		if (host->incomingXml(account, xml)) {
			handled = true;
			break;
		}
	}
	return handled;
}

/**
 * Called by PluginHost when its hosted plugin wants to send xml stanza.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param xml XML stanza to be sent
 */
void PluginManager::sendXml(int account, const QString& xml)
{
	//TODO(mck)
	// - think if it is better to ask plugin(host) for string or xml node
	// - validate things
	// - add id if missing
	// - maybe use appropriate Task to send
	// - make psi aware of things that are being send
	//   (for example, pipeline messages through history system)

	if (account < clients_.size()) {
		clients_[account]->send(xml);
	}
}

/**
 * Returns unique stanza identifier in account's stream
 *
 * \param account Identifier of the PsiAccount responsible
 * \return Unique ID to be used for when sending a stanza
 */
QString PluginManager::uniqueId(int account)
{
	QString id;
	if (account < clients_.size()) {
		id = clients_[account]->genUniqueId();
	}
	return id;
}

/**
 * Tells the plugin manager about an XMPP::Client and the owning PsiAccount
 */
void PluginManager::addAccount(const PsiAccount* account, XMPP::Client* client)
{
	clients_.append(client);
	const int id = clients_.size() - 1;
	accountIds_[account] = id;
	new StreamWatcher(client->rootTask(), this, id);
}

/**
 * Performs basic validity checking on a stanza
 * TODO : populate verifyStanza method and use it
 */
bool PluginManager::verifyStanza(const QString& stanza)
{
	Q_UNUSED(stanza);
	return true;
}

PluginManager* PluginManager::instance_ = NULL;
const QString PluginManager::loadOptionPrefix = "plugins.auto-load";
const QString PluginManager::pluginOptionPrefix = "plugins.options";
