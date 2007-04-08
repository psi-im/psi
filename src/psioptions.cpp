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
#include "common.h"
#include "im.h"
#include "xmpp_xmlcommon.h"

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
		options.setAttribute("xmlns", PROG_STORAGE_NS);
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
				if (!e.isNull() && e.tagName() == "options" && e.attribute("xmlns") == PROG_STORAGE_NS) {
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
 * Loads the defaults shipped in the Psi binary
 * \return Success
 */
bool PsiOptions::loadDefaults()
{
	return load(":/options-default.xml");
	
}

/**
 * Loads the options present in the xml config file named.
 * \param file Name of the xml config file to load
 * \return Success
 */
bool PsiOptions::load(QString file)
{
	return loadOptions(file, "options", PROG_OPTIONS_NS);
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
	return saveOptions(file, "options", PROG_OPTIONS_NS, PROG_VERSION);
}

/**
 * Default (private) constructor
 */
PsiOptions::PsiOptions()
	: OptionsTree()
{
	autoSave(false);
}

/**
 * Boom (Destructor)
 */
PsiOptions::~PsiOptions()
{
	
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
		connect(this, SIGNAL(optionChanged(const QString&)), SLOT(saveToAutoFile()));
		autoFile_=autoFile;
	}
	else {
		disconnect(this, SLOT(saveToAutoFile()));
		autoFile="";
	}
		
	
}

/**
 * Saves to the previously set file, if automatic saving is enabled
 */
void PsiOptions::saveToAutoFile()
{
	if (autoFile_ != "")
		save(autoFile_);
}

/**
 * This slot is called when the stored options retrieval is finished.
 */
void PsiOptions::getOptionsStorage_finished()
{
	OptionsStorageTask* t = (OptionsStorageTask*) sender();
	if (t->success()) {
		QDomElement e = t->options();
		e.setAttribute("xmlns",PROG_OPTIONS_NS);
		loadOptions(e, "options", PROG_OPTIONS_NS);
	}
}

PsiOptions* PsiOptions::instance_ = NULL;

