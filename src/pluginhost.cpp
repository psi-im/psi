/*
 * (c) 2006 Kevin Smith
 * (c) 2008 Maciej Niedzielski
 */

#include "pluginhost.h"

#include <QPluginLoader>
#include <QWidget>
#include <QSplitter>
#include <QAction>
#include <QByteArray>
#include <QDomElement>
#include <QKeySequence>
#include <QObject>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QTextEdit>
//#include "xmpp_message.h"
#include "psioptions.h"
#include "psiaccount.h"
#include "chatdlg.h"
#include "globalshortcut/globalshortcutmanager.h"
#include "grepshortcutkeydialog.h"
#include "pluginmanager.h"
#include "psiplugin.h"
#include "applicationinfo.h"
#include "stanzasender.h"
#include "stanzafilter.h"
#include "iqfilter.h"
#include "iqnamespacefilter.h"
#include "eventfilter.h"
#include "optionaccessor.h"
#include "shortcutaccessor.h"
#include "iconfactoryaccessor.h"
#include "activetabaccessor.h"
#include "groupchatdlg.h"
#include "tabmanager.h"
#include "popupaccessor.h"
#include "applicationinfoaccessor.h"
#include "accountinfoaccessor.h"
#include "toolbariconaccessor.h"
#include "gctoolbariconaccessor.h"
#include "widgets/iconaction.h"
#include "menuaccessor.h"
#include "contactstateaccessor.h"
#include "plugininfoprovider.h"
#include "psiaccountcontroller.h"
#include "eventcreator.h"
#include "contactinfoaccessor.h"
#include "soundaccessor.h"
#include "textutil.h"
#include "chattabaccessor.h"

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
	, plugin_(0)
	, file_(pluginFile)
	, priority_(PsiPlugin::PriorityNormal)
	, loader_(0)
	, iconset_(0)
	, valid_(false)
	, connected_(false)
	, enabled_(false)
	, hasInfo_(false)
	, infoString_(QString())
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
 * \brief Returns plugin priority.
 *
 * Data is available also when plugin is not loaded.
 */
int PluginHost::priority() const
{
	return priority_;
}

/**
 * \brief Returns plugin icon.
 *
 * Data is available also when plugin is not loaded.
 */
const QIcon& PluginHost::icon() const
{
	return icon_;
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
#ifndef PLUGINS_NO_DEBUG
	qDebug() << "Loading Plugin " << file_;
#endif
	if (plugin_) {
#ifndef PLUGINS_NO_DEBUG
		qDebug() << QString("Plugin %1 was already loaded.").arg(file_);
#endif
	}
	else {
		if (!loader_) {
			loader_ = new QPluginLoader(file_);
			//loader_->setLoadHints(QLibrary::ResolveAllSymbolsHint);
		}

		QObject* plugin = loader_->instance();
		if (!loader_->isLoaded()) {
			delete loader_;
			loader_ = 0;
		}
		else if (plugin) {
#ifndef PLUGINS_NO_DEBUG
			qDebug("Trying to load plugin");
#endif
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
				priority_ = psiPlugin->priority();
				icon_ = QIcon(psiPlugin->icon());
				hasToolBarButton_ = qobject_cast<ToolbarIconAccessor*>(plugin_) ? true : false;
				hasGCToolBarButton_ = qobject_cast<GCToolbarIconAccessor*>(plugin_) ? true : false;
				PluginInfoProvider *pip = qobject_cast<PluginInfoProvider*>(plugin_);
				if (pip) {
					hasInfo_ = true;
					infoString_ = pip->pluginInfo();
				}
			} else  {
				qWarning("Attempted to load %s, but it is not a valid plugin.", qPrintable(file_));
				if (loader_->isLoaded()) {
			  		qWarning("File is a plugin but not for Psi");
					loader_->unload();
				}
				delete loader_;
				loader_ = 0;
				valid_ = false;
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
	if (plugin_ && disable()) {
#ifndef PLUGINS_NO_DEBUG
		qDebug("Try to unload plugin %s", qPrintable(name_));
#endif
		if (!loader_) {
			qWarning("Plugin %s's loader wasn't found when trying to unload", qPrintable(name_));
			return false;
		}
		else if (loader_->unload()) {
			//if we're done with the plugin completely and it's unloaded
			// we can delete the loader;
			delete plugin_;
			delete loader_;
			plugin_ = 0;
			loader_ = 0;
			delete iconset_;
			iconset_ = 0;
			connected_ = false;
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
#ifndef PLUGINS_NO_DEBUG
			qDebug() << "connecting plugin " << name_;
#endif

			StanzaSender* s = qobject_cast<StanzaSender*>(plugin_);
			if (s) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting stanza sender");
#endif
				s->setStanzaSendingHost(this);
			}

			IqFilter* f = qobject_cast<IqFilter*>(plugin_);
			if (f) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting iq filter");
#endif
				f->setIqFilteringHost(this);
			}

			OptionAccessor* o = qobject_cast<OptionAccessor*>(plugin_);
			if (o) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting option accessor");
#endif
				o->setOptionAccessingHost(this);
			}

			ShortcutAccessor* sa = qobject_cast<ShortcutAccessor*>(plugin_);
			if (sa) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting shortcut accessor");
#endif
				sa->setShortcutAccessingHost(this);
			}
			PopupAccessor* pa = qobject_cast<PopupAccessor*>(plugin_);
			if (pa) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting popup accessor");
#endif
				pa->setPopupAccessingHost(this);
			}

			IconFactoryAccessor* ia = qobject_cast<IconFactoryAccessor*>(plugin_);
			if (ia) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting iconfactory accessor");
#endif
				ia->setIconFactoryAccessingHost(this);
			}
			ActiveTabAccessor* ta = qobject_cast<ActiveTabAccessor*>(plugin_);
			if (ta) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting activetab accessor");
#endif
				ta->setActiveTabAccessingHost(this);
			}
			ApplicationInfoAccessor* aia = qobject_cast<ApplicationInfoAccessor*>(plugin_);
			if (aia) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting applicationinfo accessor");
#endif
				aia->setApplicationInfoAccessingHost(this);
			}
			AccountInfoAccessor* ai = qobject_cast<AccountInfoAccessor*>(plugin_);
			if (ai) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting accountinfo accessor");
#endif
				ai->setAccountInfoAccessingHost(this);
			}
			ToolbarIconAccessor *tia = qobject_cast<ToolbarIconAccessor*>(plugin_);
			if (tia) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("load toolbaricon param");
#endif
				buttons_ = tia->getButtonParam();
			}
			GCToolbarIconAccessor *gtia = qobject_cast<GCToolbarIconAccessor*>(plugin_);
			if (gtia) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("load gctoolbaricon param");
#endif
				gcbuttons_ = gtia->getGCButtonParam();
			}
			MenuAccessor *ma = qobject_cast<MenuAccessor*>(plugin_);
			if (ma) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("load menu actions param");
#endif
				accMenu_ = ma->getAccountMenuParam();
				contactMenu_ = ma->getContactMenuParam();
			}
			ContactStateAccessor *csa = qobject_cast<ContactStateAccessor*>(plugin_);
			if (csa) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting contactstate accessor");
#endif
				csa->setContactStateAccessingHost(this);
			}
			PsiAccountController *pac = qobject_cast<PsiAccountController*>(plugin_);
			if (pac) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connectint psiaccount controller");
#endif
				pac->setPsiAccountControllingHost(this);
			}
			EventCreator *ecr = qobject_cast<EventCreator*>(plugin_);
			if (ecr) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connectint event creator");
#endif
				ecr->setEventCreatingHost(this);
			}
			ContactInfoAccessor *cia = qobject_cast<ContactInfoAccessor*>(plugin_);
			if (cia) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting contactinfo accessor");
#endif
				cia->setContactInfoAccessingHost(this);
			}
			SoundAccessor *soa = qobject_cast<SoundAccessor*>(plugin_);
			if(soa) {
#ifndef PLUGINS_NO_DEBUG
				qDebug("connecting sound accessor");
#endif
				soa->setSoundAccessingHost(this);
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
		enabled_ = !qobject_cast<PsiPlugin*>(plugin_)->disable();
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


bool PluginHost::outgoingXml(int account, QDomElement &e)
{
	bool handled = false;
	StanzaFilter *ef = qobject_cast<StanzaFilter*>(plugin_);
	if (ef && ef->outgoingStanza(account, e)) {
		handled = true;
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
 *
 * \param account Identifier of the PsiAccount responsible
 * \param e Event XML
 * \return Continue processing the event; true if the stanza should be silently discarded.
 */
bool PluginHost::processEvent(int account, QDomElement& e)
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

bool PluginHost::processOutgoingMessage(int account, const QString& jidTo, QString& body, const QString& type, QString& subject)
{
	bool handled = false;
	EventFilter *ef = qobject_cast<EventFilter*>(plugin_);
	if (ef && ef->processOutgoingMessage(account, jidTo, body, type, subject)) {
		handled = true;
	}
	return handled;
}

void PluginHost::logout(int account)
{
	EventFilter *ef = qobject_cast<EventFilter*>(plugin_);
	if (ef) {
		ef->logout(account);
	}
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
	manager_->sendXml(account, QString("<message to='%1' type='%4'><subject>%3</subject><body>%2</body></message>")
			  .arg(escape(to)).arg(escape(body)).arg(escape(subject)).arg(escape(type)));

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

QString PluginHost::escape(const QString &str)
{
	return TextUtil::escape(str);
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
#ifndef PLUGINS_NO_DEBUG
		qDebug("pluginmanager: blocked attempt to register the same filter again");
#endif
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
#ifndef PLUGINS_NO_DEBUG
	qDebug("add nsx");
#endif
	if (iqNsxFilters_.values(ns).contains(filter)) {
#ifndef PLUGINS_NO_DEBUG
		qDebug("pluginmanager: blocked attempt to register the same filter again");
#endif
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
	QString optionKey=QString("%1.%2.%3").arg(PluginManager::pluginOptionPrefix).arg(shortName()).arg(option);
	PsiOptions::instance()->setOption(optionKey, value);
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
QVariant PluginHost::getPluginOption(const QString &option, const QVariant &defValue)
{
	QString pluginName = name();
	QString optionKey=QString("%1.%2.%3").arg(PluginManager::pluginOptionPrefix).arg(shortName()).arg(option);
	return PsiOptions::instance()->getOption(optionKey, defValue);
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

void PluginHost::optionChanged(const QString& option)
{
	OptionAccessor *oa = qobject_cast<OptionAccessor*>(plugin_);
	if(oa)
		oa->optionChanged(option);
}

void PluginHost::applyOptions()
{
	PsiPlugin* pp = qobject_cast<PsiPlugin*>(plugin_);
	if(pp)
		pp->applyOptions();
}

void PluginHost::restoreOptions()
{
	PsiPlugin* pp = qobject_cast<PsiPlugin*>(plugin_);
	if(pp)
		pp->restoreOptions();
}


/**
 * Shortcut accessing host
 */
void PluginHost::setShortcuts()
{
	ShortcutAccessor *sa = qobject_cast<ShortcutAccessor*>(plugin_);
	if (sa) {
		sa->setShortcuts();
	}
}

void PluginHost::connectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot)
{
	GlobalShortcutManager::instance()->connect(shortcut, receiver, slot);
}

void PluginHost::disconnectShortcut(const QKeySequence& shortcut, QObject *receiver, const char* slot)
{
	GlobalShortcutManager::instance()->disconnect(shortcut, receiver, slot);
}

void PluginHost::requestNewShortcut(QObject *receiver, const char* slot)
{
	GrepShortcutKeyDialog* grep = new GrepShortcutKeyDialog();
	connect(grep, SIGNAL(newShortcutKey(QKeySequence)), receiver, slot);
	grep->show();
}

/**
 * IconFactory accessing host
 */
QIcon PluginHost::getIcon(const QString& name)
{
	return IconsetFactory::icon(name).icon();
}

void PluginHost::addIcon(const QString& name, const QByteArray& ba)
{
	QPixmap pm;
	pm.loadFromData(ba);
	PsiIcon icon;
	icon.setImpix(pm);
	icon.setName(name);
	if (!iconset_) {
		iconset_ = new Iconset();
	}
	iconset_->setIcon(name,icon);
	iconset_->addToFactory();
}

QTextEdit* PluginHost::getEditBox()
{
	QTextEdit* ed = 0;
	TabbableWidget* tw = findActiveTab();
	if(tw) {
		QWidget* chatEditProxy = tw->findChild<QWidget*>("mle");
		if(chatEditProxy) {
			ed = (QTextEdit *)chatEditProxy->children().at(1);
		}
	}

	return ed;
}

QString PluginHost::getJid()
{
	QString jid;
	TabbableWidget* tw = findActiveTab();
	if(tw) {
		jid = tw->jid().full();
	}

	return jid;
}

QString PluginHost::getYourJid()
{
	QString jid;
	TabbableWidget* tw = findActiveTab();
	if(tw) {
		jid = tw->account()->jid().full();
	}

	return jid;
}

/**
 * ApplicationInfo accessing host
 */
Proxy PluginHost::getProxyFor(const QString &obj)
{
	Proxy prx;
	ProxyItem it = ProxyManager::instance()->getItemForObject(obj);
	prx.type = it.type;
	prx.host = it.settings.host;
	prx.port = it.settings.port;
	prx.user = it.settings.user;
	prx.pass = it.settings.pass;
	return prx;
}

QString PluginHost::appName()
{
	return ApplicationInfo::name();
}

QString PluginHost::appVersion()
{
	return ApplicationInfo::version();
}

QString PluginHost::appCapsNode()
{
	return ApplicationInfo::capsNode();
}

QString PluginHost::appCapsVersion()
{ // this stuff is incompatible with new caps 1.5
	return QString(); // return ApplicationInfo::capsVersion();
}

QString PluginHost::appOsName()
{
	return ApplicationInfo::osName();
}

QString PluginHost::appHomeDir(ApplicationInfoAccessingHost::HomedirType type)
{
	return ApplicationInfo::homeDir((ApplicationInfo::HomedirType)type);
}

QString PluginHost::appResourcesDir()
{
	return ApplicationInfo::resourcesDir();
}

QString PluginHost::appLibDir()
{
	return ApplicationInfo::libDir();
}

QString PluginHost::appProfilesDir(ApplicationInfoAccessingHost::HomedirType type)
{
	return ApplicationInfo::profilesDir((ApplicationInfo::HomedirType)type);
}

QString PluginHost::appHistoryDir()
{
	return ApplicationInfo::historyDir();
}

QString PluginHost::appCurrentProfileDir(ApplicationInfoAccessingHost::HomedirType type)
{
	return ApplicationInfo::currentProfileDir((ApplicationInfo::HomedirType)type);
}

QString PluginHost::appVCardDir()
{
	return ApplicationInfo::vCardDir();
}

//AccountInfoAcsessingHost
QString PluginHost::getStatus(int account)
{
	return manager_->getStatus(account);
}

QString PluginHost::getStatusMessage(int account)
{
	return manager_->getStatusMessage(account);
}

QString PluginHost::proxyHost(int account)
{
	return manager_->proxyHost(account);
}

int PluginHost::proxyPort(int account)
{
	return manager_->proxyPort(account);
}

QString PluginHost::proxyUser(int account)
{
	return manager_->proxyUser(account);
}

QString PluginHost::proxyPassword(int account)
{
	return manager_->proxyPassword(account);
}

QStringList PluginHost::getRoster(int account)
{
	return manager_->getRoster(account);
}

QString PluginHost::getJid(int account)
{
	return manager_->getJid(account);
}

QString PluginHost::getId(int account)
{
	return manager_->getId(account);
}

QString PluginHost::getName(int account)
{
	return manager_->getName(account);
}

int PluginHost::findOnlineAccountForContact(const QString &jid) const
{
	return manager_->findOnlineAccountForContact(jid);
}

bool PluginHost::setActivity(int account, const QString& Jid, QDomElement xml)
{
	return manager_->setActivity(account, Jid, xml);
}

bool PluginHost::setMood(int account, const QString& Jid, QDomElement xml)
{
	return manager_->setMood(account, Jid, xml);
}
bool PluginHost::setTune(int account, const QString& Jid, QString tune)
{
	return manager_->setTune(account, Jid, tune);
}

void PluginHost::addToolBarButton(QObject* parent, QWidget* toolbar, int account, const QString& contact)
{
	ToolbarIconAccessor *ta = qobject_cast<ToolbarIconAccessor*>(plugin_);
	if (ta) {
		if(!buttons_.isEmpty()) {
			for (int i = 0; i < buttons_.size(); ++i) {
				QVariantHash param = buttons_.at(i);
				QString th = param.value("tooltip").value<QString>();
				IconAction *button = new IconAction(th, param.value("icon").value<QString>(), th, 0, parent);
				connect(button, SIGNAL(triggered()),param.value("reciver").value<QObject *>(), param.value("slot").value<QString>().toLatin1());
				toolbar->addAction(button);
			}
		}
		QAction *act = ta->getAction(parent, account, contact);
		if(act) {
			act->setObjectName(shortName_);
			toolbar->addAction(act);
		}
	}
}

bool PluginHost::hasToolBarButton()
{
	return hasToolBarButton_;
}

void PluginHost::addGCToolBarButton(QObject* parent, QWidget* toolbar, int account, const QString& contact)
{
	GCToolbarIconAccessor *ta = qobject_cast<GCToolbarIconAccessor*>(plugin_);
	if (ta) {
		if(!gcbuttons_.isEmpty()) {
			for (int i = 0; i < gcbuttons_.size(); ++i) {
				QVariantHash param = gcbuttons_.at(i);
				QString th = param.value("tooltip").value<QString>();
				IconAction *button = new IconAction(th, param.value("icon").value<QString>(), th, 0, parent);
				connect(button, SIGNAL(triggered()),param.value("reciver").value<QObject *>(), param.value("slot").value<QString>().toLatin1());
				toolbar->addAction(button);
			}
		}
		QAction *act = ta->getGCAction(parent, account, contact);
		if(act)
			toolbar->addAction(act);
	}
}

bool PluginHost::hasGCToolBarButton()
{
	return hasGCToolBarButton_;
}

void PluginHost::initPopup(const QString& text, const QString& title, const QString& icon, int type)
{
	manager_->initPopup(text, title, icon, type);
}

void PluginHost::initPopupForJid(int account, const QString &jid, const QString &text, const QString &title, const QString &icon, int type)
{
	manager_->initPopupForJid(account, jid, text, title, icon, type);
}

int PluginHost::registerOption(const QString& name, int initValue, const QString& path)
{
	return manager_->registerOption(name, initValue, path);
}

void PluginHost::unregisterOption(const QString &name)
{
	manager_->unregisterOption(name);
}

int PluginHost::popupDuration(const QString& name)
{
	return manager_->popupDuration(name);
}

void PluginHost::setPopupDuration(const QString& name, int value)
{
	manager_->setPopupDuration(name, value);
}

void PluginHost::addAccountMenu(QMenu *menu, int account)
{
	MenuAccessor *ma = qobject_cast<MenuAccessor*>(plugin_);
	if(ma) {
		if( !accMenu_.isEmpty()) {
			for (int i = 0; i < accMenu_.size(); ++i) {
				QVariantHash param = accMenu_.at(i);
				IconAction *act = new IconAction(param.value("name").value<QString>(), menu, param.value("icon").value<QString>());
				act->setProperty("account", QVariant(account));
				connect(act, SIGNAL(triggered()), param.value("reciver").value<QObject *>(), param.value("slot").value<QString>().toLatin1());
				menu->addAction(act);
			}
		}
		QAction *act = ma->getAccountAction(menu, account);
		if(act)
			menu->addAction(act);
	}
}

void PluginHost::addContactMenu(QMenu *menu, int account, const QString& jid)
{
	MenuAccessor *ma = qobject_cast<MenuAccessor*>(plugin_);
	if(ma) {
		if(!contactMenu_.isEmpty()) {
			for (int i = 0; i < contactMenu_.size(); ++i) {
				QVariantHash param = contactMenu_.at(i);
				IconAction *act = new IconAction(param.value("name").value<QString>(), menu, param.value("icon").value<QString>());
				act->setProperty("account", QVariant(account));
				act->setProperty("jid", QVariant(jid));
				connect(act, SIGNAL(triggered()), param.value("reciver").value<QObject *>(), param.value("slot").value<QString>().toLatin1());
				menu->addAction(act);
			}
		}
		QAction *act = ma->getContactAction(menu, account, jid);
		if(act)
			menu->addAction(act);
	}
}

void PluginHost::setupChatTab(QWidget* tab, int account, const QString& contact)
{
	ChatTabAccessor *cta = qobject_cast<ChatTabAccessor*>(plugin_);
	if(cta) {
		cta->setupChatTab(tab, account, contact);
	}
}

void PluginHost::setupGCTab(QWidget* tab, int account, const QString& contact)
{
	ChatTabAccessor *cta = qobject_cast<ChatTabAccessor*>(plugin_);
	if(cta) {
		cta->setupGCTab(tab, account, contact);
	}
}

bool PluginHost::appendingChatMessage(int account, const QString &contact,
				      QString &body, QDomElement &html, bool local)
{
	ChatTabAccessor *cta = qobject_cast<ChatTabAccessor*>(plugin_);
	if(cta) {
		return cta->appendingChatMessage(account, contact, body, html, local);
	}
	return false;
}

bool PluginHost::isSelf(int account, const QString& jid)
{
	return manager_->isSelf(account, jid);
}

bool PluginHost::isAgent(int account, const QString& jid)
{
	return manager_->isAgent(account, jid);
}

bool PluginHost::inList(int account, const QString& jid)
{
	return manager_->inList(account, jid);
}

bool PluginHost::isPrivate(int account, const QString& jid)
{
	return manager_->isPrivate(account, jid);
}

bool PluginHost::isConference(int account, const QString& jid)
{
	return manager_->isConference(account, jid);
}

QString PluginHost::name(int account, const QString& jid)
{
	return manager_->name(account, jid);
}

QString PluginHost::status(int account, const QString& jid)
{
	return manager_->status(account, jid);
}

QString PluginHost::statusMessage(int account, const QString& jid)
{
	return manager_->statusMessage(account, jid);
}

QStringList PluginHost::resources(int account, const QString& jid)
{
	return manager_->resources(account, jid);
}

bool PluginHost::hasInfoProvider()
{
	return hasInfo_;
}

QString PluginHost::pluginInfo()
{
	return infoString_;
}

void PluginHost::setStatus(int account, const QString& status, const QString& statusMessage)
{
	manager_->setStatus(account, status, statusMessage);
}

bool PluginHost::appendSysMsg(int account, const QString& jid, const QString& message)
{
	return manager_->appendSysMsg(account, jid, message);
}

void PluginHost::createNewEvent(int account, const QString& jid, const QString& descr, QObject *receiver, const char* slot)
{
	manager_->createNewEvent(account, jid, descr, receiver, slot);
}

void PluginHost::playSound(const QString &fileName)
{
	soundPlay(fileName);
}

//-- helpers --------------------------------------------------------

static bool operator<(const QRegExp &a, const QRegExp &b)
{
	return a.pattern() < b.pattern();
}
