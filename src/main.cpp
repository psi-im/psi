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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "main.h"

#include "psiapplication.h"
#include <QTimer>

#include <QImage>
#include <QBitmap>
#include <QTextCodec>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QtCrypto>
#include <QTranslator>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTime>
#include <QLibraryInfo>
#include <QHashIterator>

#include <stdlib.h>
#include <time.h>
#include "profiledlg.h"
#include "activeprofiles.h"
#include "psicli.h"
#include "psioptions.h"

#include "eventdlg.h"
#include "psicon.h"
#include "psiiconset.h"
#include "translationmanager.h"
#include "applicationinfo.h"
#include "chatdlg.h"
#ifdef USE_CRASH
#	include"crash.h"
#endif

#ifdef Q_OS_MAC
#include "CocoaUtilities/CocoaInitializer.h"
#endif

#ifdef Q_OS_WIN
#	include <qt_windows.h> // for RegDeleteKey
#endif

#ifdef Q_OS_WIN
#define URI_RESTART
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


PsiMain::PsiMain(const QHash<QString, QString>& commandline, QObject *par)
	: QObject(par)
	, cmdline(commandline)
{
	pcon = 0;

	// migrate old (pre 0.11) registry settings...
	QSettings sUser(QSettings::UserScope, "psi-im.org", "Psi");
	lastProfile = sUser.value("last_profile").toString();
	lastLang = sUser.value("last_lang").toString();
	autoOpen = sUser.value("auto_open", QVariant(false)).toBool();

	QSettings s(ApplicationInfo::homeDir(ApplicationInfo::ConfigLocation) + "/psirc", QSettings::IniFormat);
	lastProfile = s.value("last_profile", lastProfile).toString();
	lastLang = s.value("last_lang", lastLang).toString();
	autoOpen = s.value("auto_open", autoOpen).toBool();
}

PsiMain::~PsiMain()
{
	delete pcon;
}

/**
  * \brief Try to use already existing Psi instance to handle given commandline
  * Don't call after useLocalInstance()
  * \returns true if this instance is no longer needed and should terminate
  */
bool PsiMain::useActiveInstance()
{
	if (!autoOpen && cmdline.isEmpty()) {
		return false;
	}
	else if (!cmdline.contains("help") && !cmdline.contains("version") && !cmdline.contains("choose-profile")
		&& ActiveProfiles::instance()->isAnyActive()
		&& ((cmdline.contains("profile") && ActiveProfiles::instance()->isActive(cmdline["profile"])) || !cmdline.contains("profile"))) {

		bool raise = true;
		if (cmdline.contains("uri")) {
			ActiveProfiles::instance()->openUri(cmdline.value("profile"), cmdline.value("uri"));
			raise = false;
		}
		if (cmdline.contains("status")) {
			ActiveProfiles::instance()->setStatus(cmdline.value("profile"), cmdline.value("status"), cmdline.value("status-message"));
			raise = false;
		}

		if (raise) {
			ActiveProfiles::instance()->raise(cmdline.value("profile"), true);
		}

		return true;
	}
	else if (cmdline.contains("remote")) {
		// there was no active instance to satisfy the request
		// but user doesn't want to start new instance
		return true;
	}
	else {
		return false;
	}
}

/**
  * \brief Initialize this Psi instance
  */
void PsiMain::useLocalInstance()
{
	if(lastLang.isEmpty()) {
		lastLang = QLocale().name().section('_', 0, 0);
		//printf("guessing locale: [%s]\n", lastLang.latin1());
	}

	TranslationManager::instance()->loadTranslation(lastLang);

	if (cmdline.contains("help")) {
		PsiCli().showHelp();
		QTimer::singleShot(0, this, SLOT(bail()));
	}
	else if(cmdline.contains("version")) {
		PsiCli().showVersion();
		QTimer::singleShot(0, this, SLOT(bail()));
	}
	else if(cmdline.contains("choose-profile")) {
		// Select a profile
		QTimer::singleShot(0, this, SLOT(chooseProfile()));
	}
	else if(cmdline.contains("profile") && profileExists(cmdline["profile"])) {
		// Open profile from commandline
		activeProfile = lastProfile = cmdline["profile"];
		QTimer::singleShot(0, this, SLOT(sessionStart()));
	}
	else if(autoOpen && !lastProfile.isEmpty() && profileExists(lastProfile)) {
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
		} else {
			// options.xml will be created by PsiCon::init
			lastProfile = activeProfile = "default";
			autoOpen = true;
			QTimer::singleShot(0, this, SLOT(sessionStart()));
		}
	}
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
		ProfileOpenDlg *w = new ProfileOpenDlg(lastProfile, TranslationManager::instance()->availableTranslations(), TranslationManager::instance()->currentLanguage());
		w->ck_auto->setChecked(autoOpen);
		int r = w->exec();
		// lang change
		if(r == 10) {
			TranslationManager::instance()->loadTranslation(w->newLang);
			lastLang = TranslationManager::instance()->currentLanguage();
			delete w;
			continue;
		}
		else {
			bool again = false;
			autoOpen = w->ck_auto->isChecked();
			if(r == QDialog::Accepted) {
				str = w->cb_profile->currentText();
				again = !ActiveProfiles::instance()->setThisProfile(str);
				if (again) {
					QMessageBox mb(QMessageBox::Question,
						CAP(tr("Profile already in use")),
						QString(tr("The \"%1\" profile is already in use.\nWould you like to activate that session now?")).arg(str),
						QMessageBox::Cancel);
					QPushButton *activate = mb.addButton(tr("Activate"), QMessageBox::AcceptRole);
					mb.exec();
					if (mb.clickedButton() == activate) {
						lastProfile = str;
						saveSettings();
						ActiveProfiles::instance()->raise(str, true);
						quit();
						return;
					}
				}
			}
			delete w;
			if (again) {
				str = "";
				continue;
			}
			break;
		}
	}

	if(str.isEmpty()) {
		quit();
		return;
	}

	// only set lastProfile if the user opened it
	lastProfile = str;
	saveSettings();

	activeProfile = str;
	sessionStart();
}

void PsiMain::sessionStart()
{
	if (!ActiveProfiles::instance()->setThisProfile(activeProfile)) { // already running
		if (!ActiveProfiles::instance()->raise(activeProfile, true)) {
			QMessageBox::critical(0, tr("Error"), tr("Cannot open this profile - it is already running, but not responding"));
		}
		quit();
		return;
	}

	// make sure we have clean PsiOptions and PsiIconset
	PsiOptions::reset();
	// get a PsiCon
	pcon = new PsiCon();
	if (!pcon->init()) {
		delete pcon;
		pcon = 0;
		quit();
		return;
	}
	connect(pcon, SIGNAL(quit(int)), SLOT(sessionQuit(int)));

	if (cmdline.contains("uri")) {
		ActiveProfiles::instance()->openUriRequested(cmdline.value("uri"));
		cmdline.remove("uri");
	}
	if (cmdline.contains("status") || cmdline.contains("status-message")) {
		ActiveProfiles::instance()->setStatusRequested(cmdline.value("status"), cmdline.value("status-message"));
		cmdline.remove("status");
		cmdline.remove("status-message");
	}
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

void PsiMain::saveSettings()
{
	QSettings s(ApplicationInfo::homeDir(ApplicationInfo::ConfigLocation) + "/psirc", QSettings::IniFormat);
	s.setValue("last_profile", lastProfile);
	s.setValue("last_lang", lastLang);
	s.setValue("auto_open", autoOpen);
}

#ifdef URI_RESTART
// all printable chars except for space, dash, and double-quote are
//   backslash-escaped.
static QByteArray encodeUri(const QByteArray &in)
{
	QByteArray out;
	for(int n = 0; n < in.size(); ++n) {
		unsigned char c = (unsigned char)in[n];
		if(c == '\\') {
			out += "\\\\";
		}
		else if(c >= 0x21 && c < 0x7f && c != '-' && c != '\"') {
			out += in[n];
		}
		else {
			char hex[5];
			qsnprintf(hex, 5, "\\x%02x", c);
			out += QByteArray::fromRawData(hex, 4);
		}
	}
	return out;
}

static QString decodeUri(const QString &in)
{
	// note that the input is an 8-bit text encoding which is then escaped,
	//   and the result is a latin1-compatible string.  to decode this, we
	//   need to first unescape back to 8-bit, and then decode the 8-bit
	//   into unicode

	QByteArray dec;
	for(int n = 0; n < in.length(); ++n) {
		if(in[n] == '\\') {
			if(n + 1 < in.length()) {
				++n;
				if(in[n] == '\\') {
					dec += '\\';
				}
				else if(in[n] == 'x') {
					if(n + 2 < in.length()) {
						++n;
						QString xs = in.mid(n, 2);
						++n;
						bool ok = false;
						int x = xs.toInt(&ok, 16);
						if(ok) {
							unsigned char c = (unsigned char)x;
							dec += c;
						}
					}
				}
			}
		}
		else
			dec += in[n].toLatin1();
	}

	return QString::fromLocal8Bit(dec);
}

// NOTE: this function is called without qapp existing
static int restart_process(int argc, char **argv, const QByteArray &uri)
{
	// FIXME: in the future we should use CreateProcess instead, but for
	//   now it is easier to use qprocess, and it should be safe if
	//   qcoreapp is used instead of qapp which i believe does not read
	//   args
	/*BOOL ret;
	QT_WA(
		ret = CreateProcessW();
	,
		ret = CreateProcessA();
	)*/

	// re-run ourself, with encoded uri
	QCoreApplication qapp(argc, argv);
	QString selfExe = QCoreApplication::applicationFilePath();

	// don't use QCoreApplication::arguments(), because that calls
	//   GetCommandLine which is not clipped.  instead, use the provided
	//   argc/argv.
	QStringList args;
	for(int n = 1; n < argc; ++n) {
		args += QString::fromLocal8Bit(argv[n]);
	}

	if(!uri.isEmpty()) {
		args += QString("--encuri=") + QString::fromLatin1(encodeUri(uri));
	}

	if(QProcess::startDetached(selfExe, args)) {
		return 0;
	}
	else {
		return 1;
	}
}
#endif

#ifdef HAVE_QT5
void psiMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	QString time = QTime::currentTime().toString();
	QByteArray localMsg = msg.toLocal8Bit();
	switch (type) {
	case QtDebugMsg:
		fprintf(stderr, "[%s] %s (%s:%u, %s)\n", qPrintable(time), localMsg.constData(), context.file, context.line, context.function);
		break;
	case QtWarningMsg:
		fprintf(stderr, "[%s] W:%s (%s:%u, %s)\n", qPrintable(time), localMsg.constData(), context.file, context.line, context.function);
		break;
	case QtCriticalMsg:
		fprintf(stderr, "[%s] C:%s (%s:%u, %s)\n", qPrintable(time), localMsg.constData(), context.file, context.line, context.function);
		break;
	case QtFatalMsg:
		fprintf(stderr, "[%s] F:%s (%s:%u, %s)\n", qPrintable(time), localMsg.constData(), context.file, context.line, context.function);
		abort();
	case QtInfoMsg:
		fprintf(stderr, "[%s] I:%s (%s:%u, %s)\n", qPrintable(time), localMsg.constData(), context.file, context.line, context.function);
	}
}
#else
void psiMessageOutput(QtMsgType type, const char *msg)
{
	QString time = QTime::currentTime().toString();
	switch (type) {
	case QtDebugMsg:
		fprintf(stderr, "[%s] %s\n", qPrintable(time), msg);
		break;
	case QtWarningMsg:
		fprintf(stderr, "[%s] W:%s\n", qPrintable(time), msg);
		break;
	case QtCriticalMsg:
		fprintf(stderr, "[%s] C:%s\n", qPrintable(time), msg);
		break;
	case QtFatalMsg:
		fprintf(stderr, "[%s] F:%s\n", qPrintable(time), msg);
		abort();
	}
}
#endif

QStringList getQtPluginPathEnvVar()
{
	QStringList out;

	QByteArray val = qgetenv("QT_PLUGIN_PATH");
	if(!val.isEmpty())
	{
#if defined(Q_OS_WIN) || defined(Q_OS_SYMBIAN)
		QLatin1Char pathSep(';');
#else
		QLatin1Char pathSep(':');
#endif
		QStringList paths = QString::fromLatin1(val).split(pathSep, QString::SkipEmptyParts);
		foreach(const QString &path, paths)
		{
			QString canonicalPath = QDir(path).canonicalPath();
			if(!canonicalPath.isEmpty() && !out.contains(canonicalPath))
				out += canonicalPath;
		}
	}

	return out;
}

int main(int argc, char *argv[])
{
	// If Psi runs as uri handler the commandline might contain
	// almost arbitary network supplied data after the "--uri" argument.
	// To prevent any potentially dangerous options in Psi or
	// Qt to be triggered by this, filter out the uri and any following
	// data as early as possible.  We even filter before qca init, just in
	// case qca ever decides to check commandline arguments in a future
	// version.
	// see http://www.mozilla.org/security/announce/2007/mfsa2007-23.html
	// for how this problem affected firefox on windows.

	PsiCli cli;

	QHash<QByteArray, QByteArray> cmdline = cli.parse(argc, argv, QList<QByteArray>() << "uri", &argc);

	if (cmdline.contains("uri")) {
#ifdef URI_RESTART
		// for windows, we have to restart the process
		return restart_process(argc, argv, cmdline["uri"]);
#else
		// otherwise, it should enough to modify argc/argv
		argv[argc] = 0;
#endif
	}

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
	// remove qt's own plugin path on these platforms, to enable safe
	//   distribution
	QString defaultPluginPath = QLibraryInfo::location(QLibraryInfo::PluginsPath);
	if(!getQtPluginPathEnvVar().contains(defaultPluginPath))
		QCoreApplication::removeLibraryPath(defaultPluginPath);
#endif

	// NOTE: Qt 4.5 compatibility note: please don't move this call.
	//   instead, upgrade to QCA 2.0.2, which fixes the bug in the right
	//   place.
	QCA::Initializer init;
	// END NOTE

#ifdef Q_OS_MAC
	CocoaInitializer cocoaInitializer;
#endif

	// it must be initialized first in order for ApplicationInfo::resourcesDir() to work
#ifdef HAVE_QT5
	qInstallMessageHandler(psiMessageOutput);
#else
	qInstallMsgHandler(psiMessageOutput);
#endif
	PsiApplication app(argc, argv);
	QApplication::setApplicationName(ApplicationInfo::name());
	QApplication::addLibraryPath(ApplicationInfo::resourcesDir());
	QApplication::addLibraryPath(ApplicationInfo::homeDir(ApplicationInfo::DataLocation));
	QApplication::setQuitOnLastWindowClosed(false);

#ifdef Q_OS_MAC
	QDir dir(QApplication::applicationDirPath());
	dir.cdUp();
	dir.cd("Plugins");
	QApplication::addLibraryPath(dir.absolutePath());
#endif

	// Initialize QCA
	QCA::setProperty("pgp-always-trust", true);
	QCA::KeyStoreManager keystoremgr;
	QCA::KeyStoreManager::start();
	keystoremgr.waitForBusyFinished(); // FIXME get rid of this

#ifdef USE_CRASH
	int useCrash = !cmdline.contains("nocrash");

	if ( useCrash )
		Crash::registerSigsegvHandler(argv[0]);
#endif

	// seed the random number generator
	srand(time(NULL));

	//dtcp_port = 8000;

	// now when QApplication created and text codecs initiaized we can convert
	// command line arguments to strings
	QHash<QString,QString> cmdlines;
	QHashIterator<QByteArray,QByteArray> clIt(cmdline);
	while (clIt.hasNext()) {
		clIt.next();
#ifdef URI_RESTART
		if (clIt.key() == "encuri") {
			cmdlines.insert("uri", decodeUri(QString::fromLatin1(clIt.value())));
			continue;
		}
#endif
		cmdlines.insert(QString::fromLocal8Bit(clIt.key().constData()),
						QString::fromLocal8Bit(clIt.value().constData()));
	}

	PsiMain *psi = new PsiMain(cmdlines);
	// check if we want to remote-control other psi instance
	if (psi->useActiveInstance()) {
		delete psi;
		return 0;
	}

	//if(link_test)
	//	printf("Link test enabled\n");

	// silly winsock workaround
	//QSocketDevice *d = new QSocketDevice;
	//delete d;

	// Initialize translations
	TranslationManager::instance();

	// need SHA1 for Iconset sound
	//if(!QCA::isSupported(QCA::CAP_SHA1))
	//	QCA::insertProvider(XMPP::createProviderHash());

	QObject::connect(psi, SIGNAL(quit()), &app, SLOT(quit()));
	psi->useLocalInstance();
	int returnValue = app.exec();
	delete psi;

	return returnValue;
}

#ifdef QCA_STATIC
#include <QtPlugin>
#ifdef HAVE_OPENSSL
Q_IMPORT_PLUGIN(qca_ossl)
#endif
#ifdef HAVE_CYRUSSASL
Q_IMPORT_PLUGIN(qca_cyrus_sasl)
#endif
Q_IMPORT_PLUGIN(qca_gnupg)
#endif

//#if defined(Q_OS_WIN) && defined(QT_STATICPLUGIN)
//Q_IMPORT_PLUGIN(qjpeg)
//Q_IMPORT_PLUGIN(qgif)
//#endif
