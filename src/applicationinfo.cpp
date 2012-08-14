#include <QString>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QLocale>
#include <QDesktopServices>
#include <QMessageBox>

#ifdef Q_WS_X11
#include <sys/stat.h> // chmod
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#ifdef Q_WS_MAC
#include <sys/stat.h> // chmod
#include <CoreServices/CoreServices.h>
#endif

#include "psiapplication.h"
#include "applicationinfo.h"
#include "profiles.h"
#include "homedirmigration.h"
#include "activeprofiles.h"
#include "translationmanager.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif

// Constants. These should be moved to a more 'dynamically changeable'
// place (like an external file loaded through the resources system)
// Should also be overridable through an optional file.

#define PROG_NAME "Psi"
#define PROG_VERSION PSI_VERSION
//#define PROG_VERSION "0.15-dev" " (" __DATE__ ")" //CVS Builds are dated
//#define PROG_VERSION "0.15";
#define PROG_CAPS_NODE "http://psi-im.org/caps"
#define PROG_CAPS_VERSION "caps-b75d8d2b25"
#define PROG_IPC_NAME "org.psi-im.Psi"	// must not contain '\\' character on Windows
#define PROG_OPTIONS_NS "http://psi-im.org/options"
#define PROG_STORAGE_NS "http://psi-im.org/storage"
#define PROG_FILECACHE_NS "http://psi-im.org/filecache"
#ifdef Q_WS_MAC
#define PROG_APPCAST_URL "http://psi-im.org/appcast/psi-mac.xml"
#else
#define PROG_APPCAST_URL ""
#endif

#if defined(Q_WS_X11) && !defined(PSI_DATADIR)
#define PSI_DATADIR "/usr/local/share/psi"
#endif


QString ApplicationInfo::name()
{
	return PROG_NAME;
}

QString ApplicationInfo::shortName()
{
	return QString(PROG_NAME).toLower();
}

QString ApplicationInfo::version()
{
	return PROG_VERSION;
}

QString ApplicationInfo::capsNode()
{
	return PROG_CAPS_NODE;
}

QString ApplicationInfo::capsVersion()
{
	return PROG_CAPS_VERSION;
}

QString ApplicationInfo::IPCName()
{
	return PROG_IPC_NAME;
}

QString ApplicationInfo::getAppCastURL()
{
	return PROG_APPCAST_URL;
}

QString ApplicationInfo::optionsNS()
{
	return PROG_OPTIONS_NS;
}

QString ApplicationInfo::storageNS()
{
	return PROG_STORAGE_NS;
}	

QString ApplicationInfo::fileCacheNS()
{
	return PROG_FILECACHE_NS;
}

QStringList ApplicationInfo::getCertificateStoreDirs()
{
	QStringList l;
	l += ApplicationInfo::resourcesDir() + "/certs";
	l += ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + "/certs";
	return l;
}

QStringList ApplicationInfo::dataDirs()
{
	const static QStringList dirs = QStringList() << ":" << "." << homeDir(DataLocation)
												  << resourcesDir();
	return  dirs;
}

QString ApplicationInfo::getCertificateStoreSaveDir()
{
	QDir certsave(homeDir(DataLocation) + "/certs");
	if(!certsave.exists()) {
		QDir home(homeDir(DataLocation));
		home.mkdir("certs");
	}

	return certsave.path();
}

QString ApplicationInfo::resourcesDir()
{
#if defined(Q_WS_X11)
	return PSI_DATADIR;
#elif defined(Q_WS_WIN)
	return qApp->applicationDirPath();
#elif defined(Q_WS_MAC)
	// FIXME: Clean this up (remko)
	// System routine locates resource files. We "know" that Psi.icns is
	// in the Resources directory.
	QString resourcePath;
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFStringRef resourceCFStringRef = CFStringCreateWithCString(NULL,
									"application.icns", kCFStringEncodingASCII);
	CFURLRef resourceURLRef = CFBundleCopyResourceURL(mainBundle,
											resourceCFStringRef, NULL, NULL);
	if (resourceURLRef) {
		CFStringRef resourcePathStringRef = CFURLCopyFileSystemPath(
					resourceURLRef, kCFURLPOSIXPathStyle);
		const char* resourcePathCString = CFStringGetCStringPtr(
					resourcePathStringRef, kCFStringEncodingASCII);
		if (resourcePathCString) {
			resourcePath = resourcePathCString;
		}
		else { // CFStringGetCStringPtr failed; use fallback conversion
			CFIndex bufferLength = CFStringGetLength(resourcePathStringRef) + 1;
			char* resourcePathCString = new char[ bufferLength ];
			Boolean conversionSuccess = CFStringGetCString(
						resourcePathStringRef, resourcePathCString,
						bufferLength, kCFStringEncodingASCII);
			if (conversionSuccess) {
				resourcePath = resourcePathCString;
			}
			delete [] resourcePathCString;  // I own this
		}
		CFRelease( resourcePathStringRef ); // I own this
	}
	// Remove the tail component of the path
	if (! resourcePath.isNull()) {
		QFileInfo fileInfo(resourcePath);
		resourcePath = fileInfo.absolutePath();
	}
	return resourcePath;
#endif
}

QString ApplicationInfo::libDir()
{
#if defined(Q_OS_UNIX)
	return PSI_LIBDIR;
#else
	return QString();
#endif
}

/** \brief return psi's private read write data directory
  * unix+mac: $HOME/.psi
  * environment variable "PSIDATADIR" overrides
  */
QString ApplicationInfo::homeDir(ApplicationInfo::HomedirType type)
{
	static QString configDir_;
	static QString dataDir_;
	static QString cacheDir_;

	if (configDir_.isEmpty()) {
		// Try the environment override first
		configDir_ = QString::fromLocal8Bit(getenv("PSIDATADIR"));

		if (configDir_.isEmpty()) {
#if defined Q_WS_WIN
			QString base = QFileInfo(QCoreApplication::applicationFilePath()).fileName()
					.toLower().indexOf("portable") == -1?
						"" : QCoreApplication::applicationDirPath();
			if (base.isEmpty()) {
				wchar_t path[MAX_PATH];
				if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
					configDir_ = QString::fromWCharArray(path) + "\\" + name();
				} else {
					configDir_ = QDir::homePath() + "/" + name();
				}
				dataDir_ = configDir_;
				// prefer non-roaming data location for cache which is default for qds:DataLocation
				cacheDir_ = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
			} else {
				configDir_ = dataDir_ = cacheDir_ = base + "/" + name();
			}
			// temporary store for later processing
			QDir configDir(configDir_);
			QDir cacheDir(cacheDir_);
			QDir dataDir(dataDir_);
#elif defined Q_WS_MAC
			QDir configDir(QDir::homePath() + "/Library/Application Support/" + name());
			QDir cacheDir(QDir::homePath() + "/Library/Caches/" + name());
			QDir dataDir(configDir);
#elif defined Q_WS_X11
			QString XdgConfigHome = QString::fromLocal8Bit(getenv("XDG_CONFIG_HOME"));
			QString XdgDataHome = QString::fromLocal8Bit(getenv("XDG_DATA_HOME"));
			QString XdgCacheHome = QString::fromLocal8Bit(getenv("XDG_CACHE_HOME"));
			if (XdgConfigHome.isEmpty()) {
				XdgConfigHome = QDir::homePath() + "/.config";
			}
			if (XdgDataHome.isEmpty()) {
				XdgDataHome = QDir::homePath() + "/.local/share";
			}
			if (XdgCacheHome.isEmpty()) {
				XdgCacheHome = QDir::homePath() + "/.cache";
			}
			QDir configDir(XdgConfigHome + "/" + shortName());
			QDir dataDir(XdgDataHome + "/" + shortName());
			QDir cacheDir(XdgCacheHome + "/" + shortName());

			// migrate mix-cased to lowercase, if needed

			QDir configDirOld(XdgConfigHome + "/" + name());
			QDir dataDirOld(XdgDataHome + "/" + name());
			QDir cacheDirOld(XdgCacheHome + "/" + name());

			bool ok = true;
			if (ok && !configDir.exists() && configDirOld.exists()) {
				configDirOld = QDir(XdgConfigHome);
				ok = configDirOld.rename(name(), shortName());
			}
			if (ok && !dataDir.exists() && dataDirOld.exists()) {
				dataDirOld = QDir(XdgDataHome);
				ok = dataDirOld.rename(name(), shortName());
			}
			if (ok && !cacheDir.exists() && cacheDirOld.exists()) {
				cacheDirOld = QDir(XdgCacheHome);
				ok = cacheDirOld.rename(name(), shortName());
			}

			if(!ok)
			{
				QMessageBox::information(0, QObject::tr("Conversion Error"), QObject::tr("Configuration data for a previous version of Psi was found, but it was not possible to convert it to work with the current version. Ensure you have appropriate permission and that another copy of Psi is not running, and try again."));
				exit(0);
			}
#endif
			configDir_ = configDir.path();
			cacheDir_ = cacheDir.path();
			dataDir_ = dataDir.path();

			// To prevent from multiple startup of import  wizard
			if (ActiveProfiles::instance()->isActive("import_wizard")) {
				exit(0);
			}

			if (!configDir.exists() && !dataDir.exists() && !cacheDir.exists()) {
				HomeDirMigration dlg;

				if (dlg.checkOldHomeDir()) {
					ActiveProfiles::instance()->setThisProfile("import_wizard");
					QSettings s(dlg.oldHomeDir() + "/psirc", QSettings::IniFormat);
					QString lastLang = s.value("last_lang", QString()).toString();
					if(lastLang.isEmpty()) {
						lastLang = QLocale().name().section('_', 0, 0);
					}
					TranslationManager::instance()->loadTranslation(lastLang);
					dlg.exec();
					ActiveProfiles::instance()->unsetThisProfile();
				}
			}
			if (!dataDir.exists()) {
				dataDir.mkpath(".");
			}
			if (!cacheDir.exists()) {
				cacheDir.mkpath(".");
			}
		}
		else {
			cacheDir_ = configDir_;
			dataDir_ = configDir_;
		}
	}

	QString ret;
	switch(type) {
	case ApplicationInfo::ConfigLocation:
		ret = configDir_;
		break;

	case ApplicationInfo::DataLocation:
		ret = dataDir_;
		break;

	case ApplicationInfo::CacheLocation:
		ret = cacheDir_;
		break;
	}
	return ret;
}

QString ApplicationInfo::makeSubhomePath(const QString &path, ApplicationInfo::HomedirType type)
{
	if (path.indexOf("..") == -1) { // ensure its in home dir
		QDir dir(homeDir(type) + "/" + path);
		if (!dir.exists()) {
			dir.mkpath(".");
		}
		return dir.path();
	}
	return QString();
}

QString ApplicationInfo::makeSubprofilePath(const QString &path, ApplicationInfo::HomedirType type)
{
	if (path.indexOf("..") == -1) { // ensure its in profile dir
		QDir dir(pathToProfile(activeProfile, type) + "/" + path);
		if (!dir.exists()) {
			dir.mkpath(".");
		}
		return dir.path();
	}
	return QString();
}

QString ApplicationInfo::historyDir()
{
	return makeSubprofilePath("history", ApplicationInfo::DataLocation);
}

QString ApplicationInfo::vCardDir()
{
	return makeSubprofilePath("vcard", ApplicationInfo::CacheLocation);
}

QString ApplicationInfo::bobDir()
{
	return makeSubhomePath("bob", ApplicationInfo::CacheLocation);
}

QString ApplicationInfo::profilesDir(ApplicationInfo::HomedirType type)
{
	return makeSubhomePath("profiles", type);
}

QString ApplicationInfo::currentProfileDir(ApplicationInfo::HomedirType type)
{
	return pathToProfile(activeProfile, type);
}
