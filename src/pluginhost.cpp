/*
 * (c) 2006 Kevin Smith
 * (c) 2008 Maciej Niedzielski
 */

#include "pluginhost.h"

#include <QPluginLoader>

//#include "xmpp_message.h"
#include "psioptions.h"
#include "pluginmanager.h"
#include "psiplugin.h"
#include "stanzasender.h"
#include "stanzafilter.h"
#include "iqfilter.h"
#include "iqnamespacefilter.h"
#include "eventfilter.h"
#include "optionaccessor.h"

/**
 * \brief Constructs a host/wrapper for a plugin.
 *
 * PluginHost object manages one plugin. It can load/unload, enable/disable
 * a plugin. It also provides host-side services to the plugin.
 *
 * Constructor attempts to load the plugin, check if it is a valid psi plugin,
 * and cache its name, shortname and version.
 *
 * \param manager PluginManager instance that manages all plugins
 * \param pluginFile path to plugin file
 */
PluginHost::PluginHost(PluginManager* manager, const QString& pluginFile)
	: manager_(manager)
	, file_(pluginFile)
	, plugin_(0)
	, loader_(0)
	, connected_(false)
	, enabled_(false)
	, valid_(false)
{
	load();	// reads plugin name, etc
	unload();
}

/**
 * \brief Destroys plugin host.
 *
 * Plugin is disabled and unloaded if needed.
 */
PluginHost::~PluginHost()
{
	disable();
	unload();
	if (loader_) {
		delete loader_;
		loader_ = 0;
	}
}

/**
 * \brief Returns true if wrapped file is a valid Psi plugin.
 *
 * Check is done once, in PluginHost constructor.
 */
bool PluginHost::isValid() const
{
	return valid_;
}

/**
 * \brief Returns full path to plugin file.
 */
const QString& PluginHost::path() const
{
	return file_;
}

/**
 * \brief Returns plugin full name.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::name() const
{
	return name_;
}

/**
 * \brief Returns plugin short name.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::shortName() const
{
	return shortName_;
}

/**
 * \brief Returns plugin version string.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::version() const
{
	return version_;
}

/**
 * \brief Returns plugin options widget.
 *
 * Always returns null if plugin is not currently loaded.
 */
QWidget* PluginHost::optionsWidget() const
{
	QWidget* widget = 0;
	if (plugin_) {
		widget = qobject_cast<PsiPlugin*>(plugin_)->options();
	}
	return widget;
}


//-- loading and enabling -------------------------------------------

/**
 * \brief Loads the plugin.
 *
 * Plugin is loaded but not enabled.
 * Does nothing if plugin was already loaded.
 *
 * Will fail if the plugin is not suitable (wrong Qt version,
 * wrong debug setting, wrong Psi plugin interface version etc).
 *
 * \return true if plugin is successfully loaded or if it was already loaded before.
 */
bool PluginHost::load()
{
  	qDebug() << "Loading Plugin " << file_;
	if (plugin_) {
		qWarning() << QString("Plugin %1 was already loaded.").arg(file_);
	}
	else {
		if (!loader_) {
			loader_ = new QPluginLoader(file_);
		}
	
		QObject* plugin = loader_->instance();
		if (!loader_->isLoaded()) {
			delete loader_;
		}
		else if (plugin) {
			qDebug(qPrintable(QString("Trying to load plugin")));
			//Check it's the right sort of plugin
			PsiPlugin* psiPlugin = qobject_cast<PsiPlugin*>(plugin);
			if (psiPlugin) {
				plugin_ = plugin;
				valid_ = true;
				//loaded_ = true;
				//enabled_ = false;
				name_ = psiPlugin->name();
				shortName_ = psiPlugin->shortName();
				version_ = psiPlugin->version();
			} else  {
				qWarning(qPrintable(QString("Attempted to load %1, but it is not a valid plugin.").arg(file_)));
				if (loader_->isLoaded()) {
			  		qWarning("File is a plugin but not for Psi");
					loader_->unload();
				}
				delete loader_;
			}
		}
	}

	return plugin_ != 0;
}

/**
 * \brief Unloads the plugin.
 *
 * Plugin is disabled (if needed) and unloaded.
 * Does nothing if plugin was not loaded yet.
 *
 * If plugin is successfully disabled but unloading fails, it is not enabled again.
 *
 * \return true if plugin is successfully unloaded or if it was not loaded at all.
 */
bool PluginHost::unload()
{
	return false; // TODO(mck): loading plugin again after unloading fails for some reason
	              //            so I disabled unloading for now.

	if (plugin_ && disable()) {
		if (!loader_) {
	 		qWarning(qPrintable(QString("Plugin %1's loader wasn't found when trying to unload").arg(name_)));
			return false;
		}
		else if (loader_->unload()) {	
	  		//if we're done with the plugin completely and it's unloaded
	  		// we can delete the loader;	
			delete plugin_;
			delete loader_;
			plugin_ = 0;
		}	  	
	}
	return plugin_ == 0;
}

/**
 * \brief Returns true if plugin is currently loaded.
 */
bool PluginHost::isLoaded() const
{
	return plugin_ != 0;
}

/**
 * \brief Enabled the plugin
 *
 * Plugin is loaded (if needed) and enabled.
 * Does nothing if plugin was already enabled.
 *
 * Before plugin is enabled for the first time,
 * all appropriate set...Host() methods are called.
 *
 * Will fail if the plugin cannot be loaded or if plugin's enable() method fails.
 *
 * \return true if plugin is successfully enabled or if it was already enabled before.
 */
bool PluginHost::enable()
{
	if (!enabled_ && load()) {
		if (!connected_) {
			qDebug() << "connecting plugin " << name_;
	
			StanzaSender* s = qobject_cast<StanzaSender*>(plugin_);
			if (s) {
				qDebug("connecting stanza sender");
				s->setStanzaSendingHost(this);
			}

			IqFilter* f = qobject_cast<IqFilter*>(plugin_);
			if (f) {
				qDebug("connecting iq filter");
				f->setIqFilteringHost(this);
			}

			OptionAccessor* o = qobject_cast<OptionAccessor*>(plugin_);
			if (o) {
				qDebug("connecting option accessor");
				o->setOptionAccessingHost(this);
			}
			
			connected_ = true;
		}

		enabled_ = qobject_cast<PsiPlugin*>(plugin_)->enable();
	}

	return enabled_;	
}

/**
 * \brief Disabled the plugin.
 *
 * Plugin is disabled but not unloaded.
 * Does nothing if plugin was not enabled yet.
 *
 * \return true if plugin is successfully disabled or if it was not enabled at all.
 */
bool PluginHost::disable()
{
	if (enabled_) {
		enabled_ = qobject_cast<PsiPlugin*>(plugin_)->disable();
	}
	return !enabled_;
}

/**
 * \brief Returns true if plugin is currently enabled.
 */
bool PluginHost::isEnabled() const
{
	return enabled_;
}


//-- for StanzaFilter and IqNamespaceFilter -------------------------

/**
 * \brief Give plugin the opportunity to process incoming xml
 *
 * If plugin implements incoming XML filters, they are called in turn.
 * Any handler may then modify the XML and may cause the stanza to be
 * silently discarded.
 * TODO: modification doesn't work
 *
 * \param account Identifier of the PsiAccount responsible
 * \param xml Incoming XML (may be modified)
 * \return Continue processing the XML stanza; true if the stanza should be silently discarded.
 */
bool PluginHost::incomingXml(int account, const QDomElement &e)
{
	bool handled = false;

	// try stanza filter first
	StanzaFilter* sf = qobject_cast<StanzaFilter*>(plugin_);
	if (sf && sf->incomingStanza(account, e)) {
		handled = true;
	} 
	// try iq filters
	else if (e.tagName() == "iq") {
		// get iq namespace
		QString ns;
		for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if (!i.isNull() && i.hasAttribute("xmlns")) {
				ns = i.attribute("xmlns");
				break;
			}
		}

		// choose handler function depending on iq type
		bool (IqNamespaceFilter::*handler)(int account, const QDomElement& xml) = 0;
		const QString type = e.attribute("type");
		if (type == "get") {
			handler = &IqNamespaceFilter::iqGet;
		} else if (type == "set") {
			handler = &IqNamespaceFilter::iqSet;
		} else if (type == "result") {
			handler = &IqNamespaceFilter::iqResult;
		} else if (type == "error") {
			handler = &IqNamespaceFilter::iqError;
		}

		if (handler) {
			// normal filters
			foreach (IqNamespaceFilter* f, iqNsFilters_.values(ns)) {
				if ((f->*handler)(account, e)) {
					handled = true;
					break;
				}
			}
		
			// regex filters
			QMapIterator<QRegExp, IqNamespaceFilter*> i(iqNsxFilters_);
			while (!handled && i.hasNext()) {
				if (i.key().indexIn(ns) >= 0 && (i.value()->*handler)(account, e)) {
					handled = true;
				}
			}
		}
	}

	return handled;
}


//-- for EventFilter ------------------------------------------------

/**
 * \brief Give plugin the opportunity to process incoming event.
 *
 * If plugin implements EventFilter interface,
 * this will call its processEvent() handler.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 * TODO: modification doesn't work
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param e Event XML
 * \return Continue processing the event; true if the stanza should be silently discarded.
 */
bool PluginHost::processEvent(int account, const QDomElement& e)
{
	bool handled = false;
	EventFilter *ef = qobject_cast<EventFilter*>(plugin_);
	if (ef && ef->processEvent(account, e)) {
		handled = true;
	}
	return handled;
}

/**
 * \brief Give plugin the opportunity to process incoming message event.
 *
 * If plugin implements EventFilter interface,
 * this will call its processMessage() handler.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 * TODO: modification doesn't work
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param jidFrom Jid of message sender
 * \param body Message body
 * \param subject Message subject
 * \return Continue processing the event; true if the stanza should be silently discarded.
 */
bool PluginHost::processMessage(int account, const QString& jidFrom, const QString& body, const QString& subject)
{
	bool handled = false;
	EventFilter *ef = qobject_cast<EventFilter*>(plugin_);
	if (ef && ef->processMessage(account, jidFrom, body, subject)) {
		handled = true;
	}
	return handled;
}


//-- StanzaSender ---------------------------------------------------

/**
 * \brief Sends a stanza from the specified account.
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param stanza The stanza to be sent
 */ 
void PluginHost::sendStanza(int account, const QDomElement& stanza)
{
	QTextStream stream;
	stream.setString(new QString());
	stanza.save(stream, 0);
	manager_->sendXml(account, *stream.string());
}

/**
 * \brief Sends a stanza from the specified account.
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param stanza The stanza to be sent.
 */ 
void PluginHost::sendStanza(int account, const QString& stanza)
{
	manager_->sendXml(account, stanza);
}

/**
 * \brief Sends a message from the specified account.
 * 
 * \param account Identifier of the PsiAccount responsible
 * \param to Jid of message addressee
 * \param body Message body
 * \param subject Message type
 * \param type Message type (XMPP message type)
 * \param stanza The stanza to be sent.
 */ 
void PluginHost::sendMessage(int account, const QString& to, const QString& body, const QString& subject, const QString& type)
{
	//XMPP::Message m;
	//m.setTo(to);
	//m.setBody(body);
	//m.setSubject(subject);
	//if (type =="chat" || type == "error" || type == "groupchat" || type == "headline" || type == "normal") {
	//	m.setType(type);
	//}
	//manager_->sendXml(account, m.toStanza(...).toString());

	//TODO(mck): yeah, that's sick..
	manager_->sendXml(account, QString("<message to='%1' type='%4'><subject>%3</subject><body>%2</body></message>").arg(to).arg(body).arg(subject).arg(type));

}

/**
 * \brief Returns a unique stanza id in given account XMPP stream.
 * 
 * \param account Identifier of the PsiAccount responsible
 * \return Unique stanza id, or empty string if account id is invalid.
 */
QString PluginHost::uniqueId(int account)
{
	return manager_->uniqueId(account);
}


//-- IqFilter -------------------------------------------------------

/**
 * \brief Registers an Iq handler for given namespace.
 *
 * One plugin may register multiple handlers, even for the same namespace.
 * The same handler may be registered for more than one namespace.
 * Attempts to register the same handler for the same namespace will be blocked.
 *
 * When Iq stanza arrives, it is passed in turn to matching handlers.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 *
 * Note that iq-result may contain no namespaced element in some protocols,
 * and connection made by this method will not work in such case.
 *
 * \param ns Iq namespace
 * \param filter Filter to be registered
 */
void PluginHost::addIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter)
{
	if (iqNsFilters_.values(ns).contains(filter)) {
		qWarning("pluginmanager: blocked attempt to register the same filter again");
	} else {
		iqNsFilters_.insert(ns, filter);
	}
}

/**
 * \brief Registers an Iq handler for given namespace.
 *
 * One plugin may register multiple handlers, even for the same namespace.
 * The same handler may be registered for more than one namespace.
 * Attempts to register the same handler for the same namespace will be blocked.
 *
 * When Iq stanza arrives, it is passed in turn to matching handlers.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 *
 * Note that iq-result may contain no namespaced element in some protocols,
 * and connection made by this method will not work in such case.
 *
 * \param ns Iq namespace defined by a regular expression
 * \param filter Filter to be registered
 */
void PluginHost::addIqNamespaceFilter(const QRegExp &ns, IqNamespaceFilter *filter)
{
	qDebug("add nsx");
	if (iqNsxFilters_.values(ns).contains(filter)) {
		qWarning("pluginmanager: blocked attempt to register the same filter again");
	} else {
		iqNsxFilters_.insert(ns, filter);
	}
}

/**
 * \brief Unregisters namespace handler.
 *
 * Breaks connection made by addIqNamespaceFilter().
 * Note that \a filter object is never deleted by this function.
 */
void PluginHost::removeIqNamespaceFilter(const QString &ns, IqNamespaceFilter *filter)
{
	iqNsFilters_.remove(ns, filter);
}

/**
 * \brief Unregisters namespace handler.
 *
 * Breaks connection made by addIqNamespaceFilter().
 * Note that \a filter object is never deleted by this function.
 */
void PluginHost::removeIqNamespaceFilter(const QRegExp &ns, IqNamespaceFilter *filter)
{
	iqNsxFilters_.remove(ns, filter);
}


//-- OptionAccessor -------------------------------------------------

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
void PluginHost::setPluginOption( const QString& option, const QVariant& value)
{
	// TODO(mck)

	//PsiPlugin* plugin=NULL;
	//
	//if (!plugin)
	//	return;
	//QString pluginName = plugin->name();
	//QString optionKey=QString("%1.%2.%3").arg(pluginOptionPrefix).arg(shortNames_[pluginName]).arg(option);
	//PsiOptions::instance()->setOption(optionKey,value);
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
QVariant PluginHost::getPluginOption(const QString& option)
{
	return QVariant();	// TODO(mck)
}

/**
 * \brief Sets a global option (not local to the plugin)
 * The options will be passed unaltered by the plugin manager, so
 * the options are presented as they are stored in the main option
 * system. Use setPluginOption instead of this in almost every case.
 * \param  option Option to set
 * \param value New option value
 */
void PluginHost::setGlobalOption(const QString& option, const QVariant& value)
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
QVariant PluginHost::getGlobalOption(const QString& option)
{
	return PsiOptions::instance()->getOption(option);
}


//-- helpers --------------------------------------------------------

static bool operator<(const QRegExp &a, const QRegExp &b)
{
	return a.pattern() < b.pattern();
}
