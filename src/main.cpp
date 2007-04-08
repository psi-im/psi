/*
 * main.cpp - initialization and profile/settings handling
 * Copyright (C) 2001-2003  Justin Karneges
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

#include "main.h"

#include "psiapplication.h"
#include <qtimer.h>

#include <qimage.h>
#include <qbitmap.h>
#include <qtextcodec.h>
#include <qsettings.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qmessagebox.h>
#include <QtCrypto>
#include <QTranslator>
#include <stdlib.h>
#include <time.h>
#include "profiles.h"
#include "profiledlg.h"
#include "xmpp.h"

#include "eventdlg.h"
#include "psiiconset.h"
#include "applicationinfo.h"
#include "chatdlg.h"
#ifdef USE_CRASH
#	include"crash.h"
#endif

#ifdef Q_OS_WIN
#	include <qt_windows.h> // for RegDeleteKey
#endif

/** \mainpage Psi API Documentation
 *
 *	\section intro_sec Indroduction
 *		Let's write an introduction to go here
 *	
 *	\section Installation
 *		For installation details, please see the INSTALL file
 *
 *	\section Contact Details
 *		And here we might put our contact details
 */
  


using namespace XMPP;

QTranslator *trans;
QTranslator *qttrans;
QString curLang = "en";
QString curLangName = QT_TR_NOOP("language_name");

void setLang(const QString &lang)
{
	//printf("changing lang: [%s]\n", lang.latin1());
	//The Qt book suggests these are not necessary and they don't
  	//exist in Qt4
  	/*trans->clear();
	qttrans->clear();*/
	if(lang == "en") {
		curLang = lang;
		curLangName = "English";
		return;
	}

	QStringList dirs;
	QString subdir = "";
	dirs += "." + subdir;
	dirs += ApplicationInfo::homeDir() + subdir;
	dirs += ApplicationInfo::resourcesDir() + subdir;
	for(QStringList::Iterator it = dirs.begin(); it != dirs.end(); ++it) {
		if(!QFile::exists(*it))
			continue;
		if(trans->load("psi_" + lang, *it)) {
			// try to load qt library translation
			qttrans->load("qt_" + lang, *it);
			curLang = lang;
			return;
		}
	}
}

PsiMain::PsiMain(QObject *par)
:QObject(par)
{
	pcon = 0;

	// detect available language packs
	langs.set("en", "English");

	QStringList dirs;
	QString subdir = "";
	dirs += "." + subdir;
	dirs += ApplicationInfo::homeDir() + subdir;
	dirs += ApplicationInfo::resourcesDir() + subdir;

	for(QStringList::Iterator it = dirs.begin(); it != dirs.end(); ++it) {
		if(!QFile::exists(*it))
			continue;
		QDir d(*it);
		QStringList entries = d.entryList();
		for(QStringList::Iterator it2 = entries.begin(); it2 != entries.end(); ++it2) {
			if(*it2 == "." || *it2 == "..")
				continue;

			QString str = *it2;
			// verify that it is a language file
			if(str.left(4) != "psi_")
				continue;
			int n = str.find('.', 4);
			if(n == -1)
				continue;
			if(str.mid(n) != ".qm")
				continue;
			QString lang = str.mid(4, n-4);

			//printf("found [%s], lang=[%s]\n", str.latin1(), lang.latin1());

			// get the language_name
			QString name = QString("[") + str + "]";
			QTranslator t(0);
			if(!t.load(str, *it))
				continue;

			//Is translate equivalent to the old findMessage? I hope so
			//Qt4 conversion
			QString s = t.translate("@default", "language_name");
			if(!s.isEmpty())
				name = s;

			langs.set(lang, name);
		}
	}

	// load simple registry settings
	QSettings s(QSettings::UserScope, "psi-im.org", "Psi");
	lastProfile = s.value("last_profile").toString();
	lastLang = s.value("last_lang").toString();
	autoOpen = s.value("auto_open", QVariant(false)).toBool();

	if(lastLang.isEmpty()) {
		lastLang = QTextCodec::locale();
		//printf("guessing locale: [%s]\n", lastLang.latin1());
	}

	setLang(lastLang);

	if(autoOpen && !lastProfile.isEmpty() && profileExists(lastProfile)) {
		// Auto-open the last profile
		activeProfile = lastProfile;
		QTimer::singleShot(0, this, SLOT(sessionStart()));
	}
	else if (!lastProfile.isEmpty() && !getProfilesList().isEmpty()) {
		// Select a profile
		QTimer::singleShot(0, this, SLOT(chooseProfile()));
	}
	else if (getProfilesList().count() == 1) {
		// Open the (only) profile
		activeProfile = getProfilesList()[0];
		QTimer::singleShot(0, this, SLOT(sessionStart()));
	}
	else if (!getProfilesList().isEmpty()) {
		// Select a profile
		QTimer::singleShot(0, this, SLOT(chooseProfile()));
	}
	else {
		// Create & open the default profile
		if (!profileExists("default") && !profileNew("default")) {
			QMessageBox::critical(0, tr("Error"), 
				tr("There was an error creating the default profile."));
			QTimer::singleShot(0, this, SLOT(bail()));
		}
		else {
			lastProfile = activeProfile = "default";
			autoOpen = true;
			QTimer::singleShot(0, this, SLOT(sessionStart()));
		}
	}
}

PsiMain::~PsiMain()
{
	delete pcon;

// Removed QSettings stuff for windows, don't think it's necessary anymore (remko)
/*
#ifdef Q_OS_WIN
	// remove Psi's settings from HKLM
	QSettings *rs = new QSettings;
	rs->setPath("Affinix", "psi", QSettings::SystemScope);
	rs->removeEntry("/lastProfile");
	rs->removeEntry("/lastLang");
	rs->removeEntry("/autoOpen");

	QString affinixKey = "Software\\Affinix";
#ifdef Q_OS_TEMP
	RegDeleteKeyW(HKEY_LOCAL_MACHINE, affinixKey.ucs2());
#else
	RegDeleteKeyA(HKEY_LOCAL_MACHINE, affinixKey.latin1());
#endif
	delete rs;
#endif
*/
	QSettings s(QSettings::UserScope, "psi-im.org", "Psi");
	s.setValue("last_profile", lastProfile);
	s.setValue("last_lang", lastLang);
	s.setValue("auto_open", autoOpen);
}

void PsiMain::chooseProfile()
{
	if(pcon) {
		delete pcon;
		pcon = 0;
	}

	QString str = "";

	// dirty, dirty, dirty hack
	PsiIconset::instance()->loadSystem();

	while(1) {
		ProfileOpenDlg *w = new ProfileOpenDlg(lastProfile, langs, curLang);
		w->ck_auto->setChecked(autoOpen);
		int r = w->exec();
		// lang change
		if(r == 10) {
			QString newLang = w->newLang;
			delete w;
			setLang(newLang);
			lastLang = curLang;
			continue;
		}
		else {
			if(r == QDialog::Accepted)
				str = w->cb_profile->currentText();
			autoOpen = w->ck_auto->isChecked();
			delete w;
			break;
		}
	}

	if(str.isEmpty()) {
		quit();
		return;
	}

	// only set lastProfile if the user opened it
	lastProfile = str;

	activeProfile = str;
	sessionStart();
}

void PsiMain::sessionStart()
{
	// get a PsiCon
	pcon = new PsiCon();
	if(!pcon->init()) {
		delete pcon;
		pcon = 0;
		quit();
		return;
	}
	connect(pcon, SIGNAL(quit(int)), SLOT(sessionQuit(int)));
}

void PsiMain::sessionQuit(int x)
{
	if(x == PsiCon::QuitProgram) {
		QTimer::singleShot(0, this, SLOT(bail()));
	}
	else if(x == PsiCon::QuitProfile) {
		QTimer::singleShot(0, this, SLOT(chooseProfile()));
	}
}

void PsiMain::bail()
{
	if(pcon) {
		delete pcon;
		pcon = 0;
	}
	quit();
}

int main(int argc, char *argv[])
{
	// it must be initialized first in order for ApplicationInfo::resourcesDir() to work
	QCA::Initializer init;
	PsiApplication app(argc, argv);
	QApplication::addLibraryPath(ApplicationInfo::homeDir());
	QApplication::addLibraryPath(ApplicationInfo::resourcesDir());
	QApplication::setQuitOnLastWindowClosed(false);

	// Initialize QCA
	QCA::keyStoreManager()->start();
	QCA::keyStoreManager()->waitForBusyFinished();

#ifdef USE_CRASH
	int useCrash = true;
	int i;
	for(i = 1; i < argc; ++i) {
		QString str = argv[i];
		if ( str == "--nocrash" )
			useCrash = false;
	}

	if ( useCrash )
		Crash::registerSigsegvHandler(argv[0]);
#endif

	// seed the random number generator
	srand(time(NULL));

	//dtcp_port = 8000;
	for(int n = 1; n < argc; ++n) {
		QString str = argv[n];
		QString var, val;
		int x = str.find('=');
		if(x == -1) {
			var = str;
			val = "";
		}
		else {
			var = str.mid(0,x);
			val = str.mid(x+1);
		}

		//if(var == "--no-gpg")
		//	use_gpg = false;
		//else if(var == "--no-gpg-agent")
		//	no_gpg_agent = true;
		//else if(var == "--linktest")
		//	link_test = true;
	}

	//if(link_test)
	//	printf("Link test enabled\n");

	// silly winsock workaround
	//QSocketDevice *d = new QSocketDevice;
	//delete d;

	// japanese
	trans = new QTranslator(0);
	app.installTranslator(trans);

	qttrans = new QTranslator(0);
	app.installTranslator(qttrans);

	// need SHA1 for Iconset sound
	//if(!QCA::isSupported(QCA::CAP_SHA1))
	//	QCA::insertProvider(XMPP::createProviderHash());

	PsiMain *psi = new PsiMain;
	QObject::connect(psi, SIGNAL(quit()), &app, SLOT(quit()));
	int returnValue = app.exec();
	delete psi;

	app.removeTranslator(trans);
	delete trans;
	trans = 0;
	app.removeTranslator(qttrans);
	delete qttrans;
	qttrans = 0;
	QCA::unloadAllPlugins();

	return returnValue;
}

#ifdef QCA_STATIC
#include <QtPlugin>
#ifdef HAVE_OPENSSL
Q_IMPORT_PLUGIN(qca_openssl)
#endif
#ifdef HAVE_CYRUSSASL
Q_IMPORT_PLUGIN(qca_sasl)
#endif
//Q_IMPORT_PLUGIN(qca_gnupg)
#endif
