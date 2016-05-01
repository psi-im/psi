/*
 * psioptions.cpp - Psi options class
 * Copyright (C) 2006  Kevin Smith, Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psioptions.h"

#include <QCoreApplication>
#include <QTimer>

#include "applicationinfo.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_task.h"
#include "xmpp_jid.h"
#include "xmpp_client.h"
#include "statuspreset.h"
#include "psitoolbar.h"
#include "common.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
using namespace XMPP;

// ----------------------------------------------------------------------------

class OptionsStorageTask : public Task
{
public:
	OptionsStorageTask(Task* parent) : Task(parent) { }

	// TODO
	void set(/* ... */) {
		iq_ = createIQ(doc(), "set", "", id());

		QDomElement prvt = doc()->createElement("query");
		prvt.setAttribute("xmlns", "jabber:iq:private");
		iq_.appendChild(prvt);

		// ...
	}

	void get() {
		iq_ = createIQ(doc(), "get", "", id());

		QDomElement prvt = doc()->createElement("query");
		prvt.setAttribute("xmlns", "jabber:iq:private");
		iq_.appendChild(prvt);

		QDomElement options = doc()->createElement("options");
		options.setAttribute("xmlns", ApplicationInfo::storageNS());
		prvt.appendChild(options);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (!e.isNull() && e.tagName() == "options" && e.attribute("xmlns") == ApplicationInfo::storageNS()) {
					options_ = e;
				}
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

	const QDomElement& options() const {
		return options_;
	}

private:
	QDomElement iq_;
	QDomElement options_;
};

// ----------------------------------------------------------------------------

/**
 * Returns the singleton instance of this class
 * \return Instance of PsiOptions
 */
PsiOptions* PsiOptions::instance()
{
	if ( !instance_ )
		instance_ = new PsiOptions();
	return instance_;
}

/**
 * Returns the instance of this class containing default values of all options
 * \return Instance of PsiOptions
 */
const PsiOptions* PsiOptions::defaults()
{
	if ( !defaults_ )
		defaults_ = new PsiOptions();
	return defaults_;
}

/**
 * Reset the singleton instance of this class
 * this delete the old instance so be sure no references are there anymore
 */
void PsiOptions::reset() {
	delete instance_;
	instance_ = 0;
}


/**
 * initizialises the default options for a new profile
 */
bool PsiOptions::newProfile()
{
	bool ok = true;
	if (!load(":/options/newprofile.xml")) {
		ok = false;
	}
	StatusPreset(tr("Away from desk"),
	             tr("I am away from my desk.  Leave a message."),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Showering"),
	             tr("I'm in the shower.  You'll have to wait for me to get out."),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Eating"),
	             tr("Out eating.  Mmmm.. food."),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Sleep"),
	             tr("Sleep is good.  Zzzzz"),
	             XMPP::Status::DND
	            ).toOptions(this);
	StatusPreset(tr("Work"),
	             tr("Can't chat.  Gotta work."),
	             XMPP::Status::DND
	            ).toOptions(this);
	StatusPreset(tr("Air"),
	             tr("Stepping out to get some fresh air."),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Movie"),
	             tr("Out to a movie.  Is that OK with you?"),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Secret"),
	             tr("I'm not available right now and that's all you need to know."),
	             XMPP::Status::XA
	            ).toOptions(this);
	StatusPreset(tr("Out for the night"),
	             tr("Out for the night."),
	             XMPP::Status::Away
	            ).toOptions(this);
	StatusPreset(tr("Greece"),
	             tr("I have gone to a far away place.  I will be back someday!"),
	             XMPP::Status::XA
	            ).toOptions(this);

	{
		QStringList pluginsKeys;
#ifdef PSI_PLUGINS
		PluginManager *pm = PluginManager::instance();
		QStringList plugins = pm->availablePlugins();
		foreach (const QString &plugin, plugins) {
			pluginsKeys << pm->shortName(plugin) + "-plugin";
		}
#endif
		ToolbarPrefs chatToolbar;
		chatToolbar.on = true;
		chatToolbar.name = "Chat";
		chatToolbar.keys << "chat_clear"  << "chat_find" << "chat_html_text" << "chat_add_contact";
		chatToolbar.keys += pluginsKeys;
		chatToolbar.keys << "spacer" << "chat_icon" << "chat_file"
						 << "chat_pgp" << "chat_info" << "chat_history" << "chat_voice"
						 << "chat_active_contacts";

		ToolbarPrefs groupchatToolbar;
		groupchatToolbar.on = true;
		groupchatToolbar.name = "Groupchat";
		groupchatToolbar.keys << "gchat_clear"  << "gchat_find" << "gchat_html_text" << "gchat_configure";
		groupchatToolbar.keys += pluginsKeys;
		groupchatToolbar.keys << "spacer" << "gchat_icon" ;

		ToolbarPrefs buttons;
		buttons.name = tr("Buttons");
#ifndef Q_OS_MAC
		buttons.on = true;
#endif
		buttons.keys << "button_options" << "button_status";
		buttons.dock = Qt3Dock_Bottom;

		ToolbarPrefs showContacts;
		showContacts.name = tr("Show contacts");
		showContacts.keys << "show_offline" << "show_hidden" << "show_agents" << "show_self" << "show_statusmsg";

		ToolbarPrefs eventNotifier;
		eventNotifier.name = tr("Event notifier");
		eventNotifier.keys << "event_notifier";
		eventNotifier.dock = Qt3Dock_Bottom;

		QList<ToolbarPrefs> toolbars;
		toolbars << chatToolbar
		         << groupchatToolbar
				 << buttons
		         << showContacts
		         << eventNotifier;
		foreach(ToolbarPrefs tb, toolbars) {
			tb.locked = true;
			PsiToolBar::structToOptions(this, tb);
		}
	}

	setOption("options.status.auto-away.message", tr("Auto Status (idle)"));

	return ok;
}


/**
 * Checks for existing saved Options.
 * Does not guarantee that load succeeds if the config file was corrupted.
 */
bool PsiOptions::exists(QString fileName) {
	return OptionsTree::exists(fileName);
}

/**
 * Loads the options present in the xml config file named.
 * \param file Name of the xml config file to load
 * \return Success
 */
bool PsiOptions::load(QString file)
{
	return loadOptions(file, "options", ApplicationInfo::optionsNS());
}

/**
 * Loads the options stored in the private storage of
 * the given client connection.
 * \param client the client whose private storage should be checked
 */
void PsiOptions::load(XMPP::Client* client)
{
	OptionsStorageTask* t = new OptionsStorageTask(client->rootTask());
	connect(t,SIGNAL(finished()),SLOT(getOptionsStorage_finished()));
	t->get();
	t->go(true);
}

/**
 * Saves the options tree to an xml config file.
 * \param file Name of file to save to
 * \return Success
 */
bool PsiOptions::save(QString file)
{
	return saveOptions(file, "options", ApplicationInfo::optionsNS(), ApplicationInfo::version());
}

PsiOptions::PsiOptions()
	: OptionsTree()
	, autoSaveTimer_(0)
{
	autoSaveTimer_ = new QTimer(this);
	autoSaveTimer_->setSingleShot(true);
	autoSaveTimer_->setInterval(1000);
	connect(autoSaveTimer_, SIGNAL(timeout()), SLOT(saveToAutoFile()));

	setParent(QCoreApplication::instance());
	autoSave(false);

	if (!load(":/options/default.xml"))
		qWarning("ERROR: Failed to load default options");
#ifdef Q_OS_MAC
	if (!load(":/options/macosx.xml"))
		qWarning("ERROR: Failed to load Mac OS X-specific options");
#endif
#ifdef Q_OS_WIN
	if (!load(":/options/windows.xml"))
		qWarning("ERROR: Failed to load Windows-specific options");
#endif
}

PsiOptions::~PsiOptions()
{
	// since we queue connection to saveToAutoFile, so if some option was saved prior
	// to program termination, the PsiOptions is never given the chance to save
	// the changed option
	if (!autoFile_.isEmpty()) {
		saveToAutoFile();
	}
}

/**
 * Sets whether to automatically save the options each time they change
 *
 * \param autoSave Enable/disable the feature
 * \param autoFile File to automatically save to (not needed when disabling the feature)
 */
void PsiOptions::autoSave(bool autoSave, QString autoFile)
{
	if (autoSave) {
		connect(this, SIGNAL(optionChanged(const QString&)), autoSaveTimer_, SLOT(start()));
		autoFile_ = autoFile;
	}
	else {
		disconnect(this, SIGNAL(optionChanged(const QString&)), autoSaveTimer_, SLOT(start()));
		autoFile = "";
	}
}

/**
 * Saves to the previously set file, if automatic saving is enabled
 */
void PsiOptions::saveToAutoFile()
{
	if (!autoFile_.isEmpty()) {
		save(autoFile_);
	}
}

/**
 * This slot is called when the stored options retrieval is finished.
 */
void PsiOptions::getOptionsStorage_finished()
{
	OptionsStorageTask* t = (OptionsStorageTask*) sender();
	if (t->success()) {
		QDomElement e = t->options();
		e.setAttribute("xmlns",ApplicationInfo::optionsNS());
		loadOptions(e, "options", ApplicationInfo::optionsNS());
	}
}

/**
 * Reset node \a name to default value
 * \return Nothing
 */
void PsiOptions::resetOption(const QString &name)
{
	defaults(); // to init defaults_
	QStringList nodes = getChildOptionNames(name);

	if (!isInternalNode(name)) {
		const QVariant &dev = defaults_->getOption(name);
		if (!dev.isValid()) { // don't touch non exist nodes
			return;
		}
		const QVariant &prev = getOption(name);
		if (prev == dev) { // do nothing if values is identical
			return;
		}
		setOption(name, dev);
	}
	else { // internal node
		foreach(QString node, nodes) {
			const QVariant &dev = defaults_->getOption(node);
			if (!dev.isValid()) {
				continue;
			}
			const QVariant &prev = getOption(node);
			if (prev == dev) {
				continue;
			}
			setOption(node, dev);
		}
	}
}


PsiOptions* PsiOptions::instance_ = NULL;
PsiOptions* PsiOptions::defaults_ = NULL;
