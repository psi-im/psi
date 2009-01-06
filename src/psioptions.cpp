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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
	instance_ = 0;
	delete instance_;
}

void ensureStatusPresets(PsiOptions *o)
{
	QVariantList presetNames = o->mapKeyList("options.status.presets");
	if(!presetNames.isEmpty())
		return;

	StatusPreset(PsiOptions::tr("Away from desk"),
		PsiOptions::tr("I am away from my desk.  Please leave a message."),
		XMPP::Status::Away
		).toOptions(o);
	StatusPreset(PsiOptions::tr("Gone Home"),
		PsiOptions::tr("I have gone home.  Please leave a message."),
		XMPP::Status::Away
		).toOptions(o);
	StatusPreset(PsiOptions::tr("Lunch"),
		PsiOptions::tr("I have gone to lunch.  Please leave a message."),
		XMPP::Status::Away
		).toOptions(o);
	StatusPreset(PsiOptions::tr("Work"),
		PsiOptions::tr("I'm currently amidst a great deal of work and can not talk at the moment.  Please leave a message."),
		XMPP::Status::DND
		).toOptions(o);
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
	ensureStatusPresets(this);
	{
		ToolbarPrefs buttons;
		buttons.name = tr("Buttons");
#ifndef Q_WS_MAC
		buttons.on = true;
#endif
		buttons.keys << "button_options" << "button_status";
		buttons.dock = Qt::DockBottom;

		ToolbarPrefs showContacts;
		showContacts.name = tr("Show contacts");
		showContacts.keys << "show_offline" << "show_hidden" << "show_agents" << "show_self" << "show_statusmsg";

		ToolbarPrefs eventNotifier;
		eventNotifier.name = tr("Event notifier");
		eventNotifier.keys << "event_notifier";
		eventNotifier.dock = Qt::DockBottom;

		QList<ToolbarPrefs> toolbars;
		toolbars << buttons
		         << showContacts
		         << eventNotifier;
		foreach(ToolbarPrefs tb, toolbars) {
			tb.locked = true;
			PsiToolBar::structToOptions(this, tb);
		}
	}

	setOption("options.status.auto-away.message", tr("Idle"));

	return ok;
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
#ifdef Q_WS_MAC
	if (!load(":/options/macosx.xml"))
		qWarning("ERROR: Failed to load Mac OS X-specific options");
#endif
#ifdef Q_WS_WIN
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
	if (autoFile_ != "") {
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

PsiOptions* PsiOptions::instance_ = NULL;
PsiOptions* PsiOptions::defaults_ = NULL;

