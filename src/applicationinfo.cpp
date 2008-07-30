#include <QString>
#include <QDir>
#include <QFile>

#ifdef Q_WS_X11
#include <sys/stat.h> // chmod
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#ifdef Q_WS_MAC
#include <sys/stat.h> // chmod
#include <CoreServices/CoreServices.h>
#endif

#include "applicationinfo.h"
#include "profiles.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif

// Constants. These should be moved to a more 'dynamically changeable'
// place (like an external file loaded through the resources system)
// Should also be overridable through an optional file.

#define PROG_NAME "Psi"
#define PROG_VERSION "0.13-dev" " (" __DATE__ ")"; //CVS Builds are dated
//#define PROG_VERSION "0.12";
#define PROG_CAPS_NODE "http://psi-im.org/caps";
#define PROG_CAPS_VERSION "0.13-dev-rev2";
#define PROG_IPC_NAME "org.psi-im.Psi"	// must not contain '\\' character on Windows
#define PROG_OPTIONS_NS "http://psi-im.org/options";
#define PROG_STORAGE_NS "http://psi-im.org/storage";

#if defined(Q_WS_X11) && !defined(PSI_DATADIR)
#define PSI_DATADIR "/usr/local/share/psi"
#endif


QString ApplicationInfo::name()
{
	return PROG_NAME;
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

QString ApplicationInfo::optionsNS()
{
	return PROG_OPTIONS_NS;
}

QString ApplicationInfo::storageNS()
{
	return PROG_STORAGE_NS;
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
  CFStringRef resourceCFStringRef
      = CFStringCreateWithCString( NULL, "application.icns",
                                   kCFStringEncodingASCII );
  CFURLRef resourceURLRef = CFBundleCopyResourceURL( mainBundle,
                                                     resourceCFStringRef,
                                                     NULL,
                                                     NULL );
  if ( resourceURLRef ) {
    CFStringRef resourcePathStringRef =
    CFURLCopyFileSystemPath( resourceURLRef, kCFURLPOSIXPathStyle );
    const char* resourcePathCString =
      CFStringGetCStringPtr( resourcePathStringRef, kCFStringEncodingASCII );
    if ( resourcePathCString ) {
      resourcePath.setLatin1( resourcePathCString );
    } else { // CFStringGetCStringPtr failed; use fallback conversion
      CFIndex bufferLength = CFStringGetLength( resourcePathStringRef ) + 1;
      char* resourcePathCString = new char[ bufferLength ];
      Boolean conversionSuccess =
        CFStringGetCString( resourcePathStringRef,
                            resourcePathCString, bufferLength,
                            kCFStringEncodingASCII );
      if ( conversionSuccess ) {
        resourcePath = resourcePathCString;
      }
      delete [] resourcePathCString;  // I own this
    }
    CFRelease( resourcePathStringRef ); // I own this
  }
  // Remove the tail component of the path
  if ( ! resourcePath.isNull() ) {
    QFileInfo fileInfo( resourcePath );
    resourcePath = fileInfo.dirPath( true );
  }
  return resourcePath;
#endif
}


/** \brief return psi's private read write data directory
  * unix+mac: $HOME/.psi
  * environment variable "PSIDATADIR" overrides
  */
QString ApplicationInfo::homeDir()
{
	// Try the environment override first
	char *p = getenv("PSIDATADIR");
	if(p)
		return p;

#if defined(Q_WS_X11)
	QDir proghome(QDir::homeDirPath() + "/.psi");
	if(!proghome.exists()) {
		QDir home = QDir::home();
		home.mkdir(".psi");
		chmod(QFile::encodeName(proghome.path()), 0700);
	}
	return proghome.path();
#elif defined(Q_WS_WIN)
	QString base;

	// Windows 9x
	if(QDir::homeDirPath() == QDir::rootDirPath())
		base = ".";
	// Windows NT/2K/XP variant
	else
		base = QDir::homeDirPath();

	// no trailing slash
	if(base.at(base.length()-1) == '/')
		base.truncate(base.length()-1);

	QDir proghome(base + "/PsiData");
	if(!proghome.exists()) {
		QDir home(base);
		home.mkdir("PsiData");
	}

	return proghome.path();
#elif defined(Q_WS_MAC)
	QDir proghome(QDir::homeDirPath() + "/.psi");
	if(!proghome.exists()) {
		QDir home = QDir::home();
		home.mkdir(".psi");
		chmod(QFile::encodeName(proghome.path()), 0700);
	}

	return proghome.path();
#endif
}

QString ApplicationInfo::historyDir()
{
	QDir history(pathToProfile(activeProfile) + "/history");
	if (!history.exists()) {
		QDir profile(pathToProfile(activeProfile));
		profile.mkdir("history");
	}

	return history.path();
}

QString ApplicationInfo::vCardDir()
{
	QDir vcard(pathToProfile(activeProfile) + "/vcard");
	if (!vcard.exists()) {
		QDir profile(pathToProfile(activeProfile));
		profile.mkdir("vcard");
	}

	return vcard.path();
}

QString ApplicationInfo::profilesDir()
{
	QString profiles_dir(homeDir() + "/profiles");
	QDir d(profiles_dir);
	if(!d.exists()) {
		QDir d(homeDir());
		d.mkdir("profiles");
	}
	return profiles_dir;
}
