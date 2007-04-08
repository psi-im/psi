#include <QtCore>
#include <QPluginLoader>

#include "pluginmanager.h"
#include "psiplugin.h"
#include "userlist.h"
#include "applicationinfo.h"
#include "psioptions.h"
#include <QtCrypto>


/**
 * Function to obtain all the directories in which plugins can be stored
 * \return List of plugin directories
 */ 
static QStringList pluginDirs()
{
	QStringList l;
	l += ApplicationInfo::resourcesDir() + "/plugins";
	l += ApplicationInfo::homeDir() + "/plugins";
	return l;
}

/**
 * Default constructor. Locates all plugins, sets watchers on those directories to 
 * locate new ones and loads those enabled in the config.
 */ 
PluginManager::PluginManager() : QObject(NULL)
{
	loadEnabledPlugins();
	foreach (QString path, pluginDirs()) {
		QCA::DirWatch *dw = new QCA::DirWatch(path, this);
		connect(dw, SIGNAL(changed()), SLOT(dirsChanged()));
		dirWatchers_.append(dw);
	}
	connect( PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), this, SLOT(optionChanged(const QString&)));
}

/**
 * This slot is executed when the contents of a plugin directory changes
 * It causes the available plugin list to be refreshed.
 * TODO: it should also load the plugins if they're on the autoload list
 */ 
void PluginManager::dirsChanged()
{
	availablePlugins();
}

/**
 * This causes all plugins that are both set for auto-load, and available
 * to be loaded. 
 */
void PluginManager::loadEnabledPlugins()
{
	qDebug("Loading enabled plugins");
	QStringList plugins=availablePlugins();
	foreach(QString plugin, plugins) {
		QString option=QString("%1.%2").arg(loadOptionPrefix).arg(shortNames_[plugin]);
		if ( PsiOptions::instance()->getOption(option).toBool()  ) {
			qWarning(qPrintable(QString("Plugin %1 is enabled in config: loading").arg(plugin)));
			loadPlugin(files_[plugin]);
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
	QStringList plugins=availablePlugins();
	foreach(QString d, plugins)
		loadPlugin( d );

}

/**
 * Load a plugin file. Will fail if the plugin is already active.
 * If a plugin has previously had unloadPlugin() called on it but
 * Psi has been unable to unload it (such as on OSX) and it was 
 * instead disabled, this method will reactivate the plugin, instead
 * of attempting to reload it.
 * \param file File to load
 * \return Success in loading the plugin
 */ 
bool PluginManager::loadPlugin( const QString& file )
{
  	qDebug(qPrintable(QString("Loading Plugin %1").arg(file)));
	//we can safely take the first key, as we won't have the same
	// file belonging to multiple plugins
	QList<QString> names = files_.keys(file);
	if (! names.isEmpty() ) {
		QString name = names.first();
		if ( plugins_.contains(name) ) {
			qWarning( qPrintable( QString("Plugin %1 is already active, but this should never be.").arg(file) ) );
			return false;
		}
	}
	QPluginLoader* loader=NULL;
	if ( loaders_.contains(file) ) {
		loader=loaders_[file];
	}
	else {
		loader=new QPluginLoader( file );
		loaders_.insert( file , loader);
	}
	QObject* plugin = loader->instance();
	if ( !loader->isLoaded() ) {
		delete loader;
		loaders_.remove( loaders_.keys(loader).first() );
		return false;
	}
	
	if ( !loadPlugin(plugin) ) {
		qWarning( qPrintable( QString("pluginmanager.cpp: Attempted to load %1, but it is not a valid plugin.").arg(file) ));
		if ( loader->isLoaded() ) {
		  	qWarning("File is a plugin but not for Psi");
			loader->unload();
		}
		delete loader;
		loaders_.remove( loaders_.keys(loader).first() );
		return false;
	}
	
	return true;
}

/**
 * Imports a Psi plugin loaded into a plugin object. Will fail if the plugin 
 * is not suitable (wrong Qt version, wrong debug setting, wrong Psi plugin 
 * interface version etc).
 * \param pluginObject Plugin Object
 * \return Success
 */
bool PluginManager::loadPlugin( QObject* pluginObject )
{
	if (!pluginObject)
		return false;
	qDebug( qPrintable( QString("Trying to load plugin") ) );
	//Check it's the right sort of plugin
	PsiPlugin* plugin = qobject_cast<PsiPlugin*> (pluginObject);
	if ( !plugin ) {
		return false;
	}
	qDebug( qPrintable( QString("loading plugin %1").arg(plugin->name() )));
	plugins_.insert( plugin->name(), plugin );
	
	qDebug(qPrintable(QString("connecting to plugin %1").arg(plugin->name())));
	connect( plugin, SIGNAL(sendStanza(const PsiAccount*, const QDomElement&)), this, SLOT(sendStanza(const PsiAccount*, const QDomElement&)));
	connect( plugin, SIGNAL(sendStanza(const PsiAccount*, const QString&)), this, SLOT(sendStanza(const PsiAccount*, const QString&)));
	connect( plugin, SIGNAL(setPluginOption( const QString&, const QVariant& )), this, SLOT( setPluginOption( const QString&, const QVariant& )));
	connect( plugin, SIGNAL(getPluginOption( const QString&, QVariant&)), this, SLOT( getPluginOption( const QString&, QVariant&)));
	connect( plugin, SIGNAL(setGlobalOption( const QString&, const QVariant& )), this, SLOT( setGlobalOption( const QString&, const QVariant& )));
	connect( plugin, SIGNAL(getGlobalOption( const QString&, QVariant&)), this, SLOT( getGlobalOption( const QString&, QVariant&)));
	return true;
}

/**
 * Unloads all Psi plugins. 
 * \return Success of unloading all plugins; if any plugins couldn't be 
 *         unloaded, false.
 */ 
bool PluginManager::unloadAllPlugins()
{
	qDebug("Unloading all plugins");
	bool ok=true;
	foreach (QString plugin, plugins_.keys()) {
		if (!unloadPlugin(plugin))
			ok=false;
	}
	return ok;
}

/**
 * Unloads the named plugin. If the plugin cannot be unloaded, the method
 * returns false but the plugin is still removed from the active list
 * and not used for further processing (This is most relevant on OSX
 * where it seems we can't ever unload a plugin).
 * \param plugin Name of the plugin to unload
 * \return Plugin unloaded result.
 */ 
bool PluginManager::unloadPlugin(const QString& plugin)
{
	if ( !plugins_.contains(plugin) ) {
	  	qWarning( qPrintable( QString("Plugin %1 wasn't found when trying to unload").arg(plugin) ) );
		return false;
	}
	qDebug(qPrintable(QString("attempting to disconnect %1").arg(plugins_[plugin]->name())));
	plugins_[plugin]->disconnect();
	QString file=files_[plugin];
	if ( !loaders_.contains(file) ) {
	 	qWarning( qPrintable( QString("Plugin %1's loader wasn't found when trying to unload").arg(plugin) ) );
		return false;
	}
	QPluginLoader* loader=loaders_[file];
	bool done=false;
	if ( loader->unload() ) {	
	  	//if we're done with the plugin completely and it's unloaded
	  	// we can delete the loader;
		done=true;
		delete loader;
	  	loaders_.remove(file);
	}
	//if the plugin was not unloaded, we remove it from the active
	// list, but keep the loader so we can obtain future instances
	plugins_.remove(plugin);
	return done;
}

/**
 * Find the file which provides the named plugin. If the named plugin is not
 * known, an empty string is provided.
 * \param plugin Name of the plugin.
 * \return Path to the plugin file.
 */ 
QString PluginManager::pathToPlugin(const QString& plugin)
{
	if (! files_.contains(plugin) )
		return QString("");
	return files_[plugin];
}

/**
 * Find the file which provides the named plugin. If the named plugin is not
 * known, an empty string is provided.
 * \param plugin Name of the plugin.
 * \return Path to the plugin file.
 */ 
QString PluginManager::shortName(const QString& plugin)
{
	if (! shortNames_.contains(plugin) )
		return QString("");
	return shortNames_[plugin];
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
 * Searches the relevant folders for plugins, queries them for their names
 * and returns a list of available plugin names.
 */
QStringList PluginManager::availablePlugins()
{
	foreach(QString d, pluginDirs()) {
		QDir dir(d);
		foreach(QString file, dir.entryList()) {
		  	file=dir.absoluteFilePath(file);
			qWarning(qPrintable(QString("Found plugin: %1").arg(file)));
			if ( !loaders_.contains(file) ) { 
				loadPlugin(file);
				if ( loaders_.contains(file) ) {
			  		qWarning("aye");
			  		PsiPlugin* plugin=qobject_cast<PsiPlugin*> ( loaders_[file]->instance() );
			  		files_.insert(plugin->name(), dir.absoluteFilePath(file) );
					shortNames_.insert(plugin->name(), plugin->shortName());
					unloadPlugin( plugin->name() );
				}
			}
			else
			 	qWarning("Which we already knew about");
		}
	}

	return files_.keys();
}

/**
 * Provides a pointer to a QWidget providing the options dialog for the 
 * named plugin, if it exists, else NULL.
 * \param plugin Name of the plugin.
 * \return Pointer to the options widget for the named plugin.
 */
QWidget* PluginManager::getOptionsWidget( const QString& plugin )
{
  	if (plugins_.contains(plugin))
		return plugins_.value(plugin)->options();
	else
		qWarning(qPrintable(QString("Attempting to get options for %1 which doesn't exist").arg(plugin)));
	return NULL;
}

/**
 * \brief Sets an option (local to the plugin)
 * The options will be automatically prefixed by the plugin manager, so
 * there is no need to uniquely name the options. In the same way as the
 * main options system, a hierachy is available by dot-delimiting the 
 * levels ( e.g. "emoticons.show"). Use this and not setGlobalOption
 * in almost every case.
 * \param  option Option to set
 * \param value New option value
 */
void PluginManager::setPluginOption( const QString& option, const QVariant& value)
{
	PsiPlugin* plugin=NULL;
	
	if (!plugin)
		return;
	QString pluginName = plugin->name();
	QString optionKey=QString("%1.%2.%3").arg(pluginOptionPrefix).arg(shortNames_[pluginName]).arg(option);
	PsiOptions::instance()->setOption(optionKey,value);
}

/**
 * \brief Gets an option (local to the plugin)
 * The options will be automatically prefixed by the plugin manager, so
 * there is no need to uniquely name the options. In the same way as the
 * main options system, a hierachy is available by dot-delimiting the 
 * levels ( e.g. "emoticons.show"). Use this and not getGlobalOption
 * in almost every case.
 * \param  option Option to set
 * \param value Return value
 */
void PluginManager::getPluginOption( const QString& option, QVariant& value)
{
	
}

/**
 * \brief Sets a global option (not local to the plugin)
 * The options will be passed unaltered by the plugin manager, so
 * the options are presented as they are stored in the main option
 * system. Use setPluginOption instead of this in almost every case.
 * \param  option Option to set
 * \param value New option value
 */
void PluginManager::setGlobalOption( const QString& option, const QVariant& value)
{
	PsiOptions::instance()->setOption(option, value);
}
	
/**
 * \brief Gets a global option (not local to the plugin)
 * The options will be passed unaltered by the plugin manager, so
 * the options are presented as they are stored in the main option
 * system. Use getPluginOption instead of this in almost every case.
 * \param  option Option to set
 * \param value Return value
 */
void PluginManager::getGlobalOption( const QString& option, QVariant& value)
{
	value=PsiOptions::instance()->getOption(option);
	if (value.isValid())
		qDebug("valid option");
	else
		qDebug("not valid option");
}
	
void PluginManager::message(PsiAccount* account, const XMPP::Jid& from, const UserListItem* ul, const QString& message)
{
	QString fromString=QString("%1").arg(from.full());
	qDebug(qPrintable(QString("message from %1").arg(fromString)));
	foreach(PsiPlugin* plugin, plugins_.values() ) {
		plugin->message( account, message , fromString , from.full() );
	}
}

/**
 * \brief Give each plugin the opportunity to process the incoming event
 * 
 * Each plugin is passed the event in turn. Any plugin may then modify the event
 * and may cause the event to be silently discarded.
 * 
 * \param account Pointer to the PsiAccount responsible
 * \param event Incoming event
 * \return Continue processing the event; false if the event should be silently discarded.
 */
bool PluginManager::processEvent( const PsiAccount* account, QDomElement &event )
{
	bool ok=true;
	QDomNode message = event.elementsByTagName("message").item(0);
	//QString account = event.elementsByTagName("account").item(0).toText().data();
	foreach(PsiPlugin* plugin, plugins_.values() ) {
		ok = ok && plugin->processEvent( account, message ) ;
	}
	return ok;
}

/**
 *	\brief Sends a stanza from the specified account.
 * 
 * \param account The account name, as used by the plugin interface.
 * \param stanza The stanza to be sent.
 */ 
void PluginManager::sendStanza( const PsiAccount* account, const QDomElement& stanza)
{
	sendStanza(account,PsiPlugin::toString(stanza));
}

/**
 *	\brief Sends a stanza from the specified account.
 * 
 * \param account The account name, as used by the plugin interface.
 * \param stanza The stanza to be sent.
 */ 
void PluginManager::sendStanza( const PsiAccount* account, const QString& stanza)
{
	qDebug(qPrintable(QString("Want to send stanza  to account %2").arg((int)account)));
	if (!clients_.contains(account) || !verifyStanza(stanza))
		return;
	clients_[account]->send(stanza);
}

/**
 * Tells the plugin manager about an XMPP::Client and the owning PsiAccount
 */
void PluginManager::addAccount( const PsiAccount* account, XMPP::Client* client)
{
	clients_[account]=client;
}

/**
 * Performs basic validity checking on a stanza
 * TODO : populate verifyStanza method
 */
bool PluginManager::verifyStanza(const QString& stanza)
{
	Q_UNUSED(stanza);
	return true;
}

PluginManager* PluginManager::instance_ = NULL;
const QString PluginManager::loadOptionPrefix = "plugins.auto-load";
const QString PluginManager::pluginOptionPrefix = "plugins.options";
