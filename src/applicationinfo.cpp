#include "applicationinfo.h"

#include "activeprofiles.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif
#include "profiles.h"
#include "psiapplication.h"
#include "systeminfo.h"
#include "translationmanager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLatin1String>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#include <sys/stat.h> // chmod
#endif
#ifdef Q_OS_UNIX
#include <sys/stat.h> // chmod
#endif
#ifdef Q_OS_WIN
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#endif

#define xstr(a) str(a)
#define str(a) #a

// Constants. These should be moved to a more 'dynamically changeable'
// place (like an external file loaded through the resources system)
// Should also be overridable through an optional file.
//
// PROG_SNAME - read as small name, system name, soname, short name, fs name

#define PROG_NAME CLIENT_NAME
#define PROG_SNAME CLIENT_SNAME
#define PROG_VERSION PSI_VERSION
#define PROG_CAPS_NODE CLIENT_CAPS_NODE
#define PROG_IPC_NAME "org.psi-im.Psi" // must not contain '\\' character on Windows
#define PROG_OPTIONS_NS "http://psi-im.org/options"
#define PROG_STORAGE_NS "http://psi-im.org/storage"
#define PROG_FILECACHE_NS "http://psi-im.org/filecache"
#ifdef Q_OS_MAC
#define PROG_APPCAST_URL "https://psi-im.org/appcast/psi-mac.xml"
#else
#define PROG_APPCAST_URL ""
#endif

QString ApplicationInfo::name() { return PROG_NAME; }

QLatin1String ApplicationInfo::sname() { return QLatin1String(PROG_SNAME); }

QString ApplicationInfo::version() { return PROG_VERSION; }

QString ApplicationInfo::capsNode() { return PROG_CAPS_NODE; }

QString ApplicationInfo::osName() { return SystemInfo::instance()->os(); }

QString ApplicationInfo::IPCName() { return PROG_IPC_NAME; }

QString ApplicationInfo::getAppCastURL() { return PROG_APPCAST_URL; }

QString ApplicationInfo::optionsNS() { return PROG_OPTIONS_NS; }

QString ApplicationInfo::storageNS() { return PROG_STORAGE_NS; }

QString ApplicationInfo::fileCacheNS() { return PROG_FILECACHE_NS; }

QStringList ApplicationInfo::getCertificateStoreDirs()
{
    QStringList l;
    l += ApplicationInfo::resourcesDir() + "/certs";
    l += ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + "/certs";
    return l;
}

QStringList ApplicationInfo::dataDirs()
{
    const static QStringList dirs = QStringList() << ":"
                                                  << "." << homeDir(DataLocation) << resourcesDir();
    return dirs;
}

QStringList ApplicationInfo::pluginDirs()
{
    QStringList l;
    l += ApplicationInfo::resourcesDir() + "/plugins";
    l += homeDir(ApplicationInfo::DataLocation) + "/plugins";
    l += libDir() + "/plugins";
    return l;
}

QString ApplicationInfo::getCertificateStoreSaveDir()
{
    QDir certsave(homeDir(DataLocation) + "/certs");
    if (!certsave.exists()) {
        QDir home(homeDir(DataLocation));
        home.mkdir("certs");
    }

    return certsave.path();
}

QString ApplicationInfo::resourcesDir()
{
#if defined(Q_OS_WIN) || defined(Q_OS_HAIKU)
    return qApp->applicationDirPath();
#elif defined(Q_OS_MAC)
    // FIXME: Clean this up (remko)
    // System routine locates resource files. We "know" that Psi.icns is
    // in the Resources directory.
    QString     resourcePath;
    CFBundleRef mainBundle          = CFBundleGetMainBundle();
#ifdef PSI_PLUS
    const char *appIconName         = "application-plus.icns";
#else
    const char *appIconName = "application.icns";
#endif
    CFStringRef resourceCFStringRef = CFStringCreateWithCString(nullptr, appIconName, kCFStringEncodingASCII);
    CFURLRef    resourceURLRef      = CFBundleCopyResourceURL(mainBundle, resourceCFStringRef, nullptr, nullptr);
    if (resourceURLRef) {
        CFStringRef resourcePathStringRef = CFURLCopyFileSystemPath(resourceURLRef, kCFURLPOSIXPathStyle);
        const char *resourcePathCString   = CFStringGetCStringPtr(resourcePathStringRef, kCFStringEncodingASCII);
        if (resourcePathCString) {
            resourcePath = resourcePathCString;
        } else { // CFStringGetCStringPtr failed; use fallback conversion
            CFIndex bufferLength        = CFStringGetLength(resourcePathStringRef) + 1;
            char *  resourcePathCString = new char[bufferLength];
            Boolean conversionSuccess
                = CFStringGetCString(resourcePathStringRef, resourcePathCString, bufferLength, kCFStringEncodingASCII);
            if (conversionSuccess) {
                resourcePath = resourcePathCString;
            }
            delete[] resourcePathCString; // I own this
        }
        CFRelease(resourcePathStringRef); // I own this
    }
    // Remove the tail component of the path
    if (!resourcePath.isNull()) {
        QFileInfo fileInfo(resourcePath);
        resourcePath = fileInfo.absolutePath();
    }
    return resourcePath;
#else
    return PSI_DATADIR;
#endif
}

QString ApplicationInfo::libDir()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_HAIKU)
    return PSI_LIBDIR;
#else
    return QCoreApplication::applicationDirPath();
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
#if defined(Q_OS_WIN)
            QString base = ApplicationInfo::isPortable() ? QCoreApplication::applicationDirPath() : "";
            if (base.isEmpty()) {
                wchar_t path[MAX_PATH];
                if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path) == S_OK) {
                    configDir_ = QString::fromWCharArray(path) + "\\" + name();
                } else {
                    configDir_ = QDir::homePath() + "/" + name();
                }
                dataDir_ = configDir_;
                // prefer non-roaming data location for cache which is default for qds:DataLocation
                cacheDir_ = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
            } else {
                configDir_ = dataDir_ = cacheDir_ = base + "/" + name();
            }
            // temporary store for later processing
            QDir configDir(configDir_);
            QDir cacheDir(cacheDir_);
            QDir dataDir(dataDir_);
#elif defined(Q_OS_MAC)
            QDir configDir(QDir::homePath() + "/Library/Application Support/" + name());
            QDir cacheDir(QDir::homePath() + "/Library/Caches/" + name());
            QDir dataDir(configDir);
#elif defined(HAVE_FREEDESKTOP)

            QString XdgConfigHome(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
            QString XdgDataHome(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
            QString XdgCacheHome(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation));

            if (XdgConfigHome.isEmpty()) {
                XdgConfigHome = QDir::homePath() + "/.config";
            }
            if (XdgDataHome.isEmpty()) {
                XdgDataHome = QDir::homePath() + "/.local/share";
            }
            if (XdgCacheHome.isEmpty()) {
                XdgCacheHome = QDir::homePath() + "/.cache";
            }
            QDir configDir(XdgConfigHome + "/" + sname());
            QDir dataDir(XdgDataHome + "/" + sname());
            QDir cacheDir(XdgCacheHome + "/" + sname());

#elif defined(Q_OS_HAIKU)
            QDir configDir(QDir::homePath() + "/config/settings/Qt/.config/" + sname());
            QDir cacheDir(QDir::homePath() + "/config/cache/" + sname());
            QDir dataDir(configDir);
#endif
            configDir_ = configDir.path();
            cacheDir_  = cacheDir.path();
            dataDir_   = dataDir.path();

            if (!dataDir.exists()) {
                dataDir.mkpath(".");
            }
            if (!cacheDir.exists()) {
                cacheDir.mkpath(".");
            }
        } else {
            cacheDir_ = configDir_;
            dataDir_  = configDir_;
        }
    }

    QString ret;
    switch (type) {
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

QString ApplicationInfo::historyDir() { return makeSubprofilePath("history", ApplicationInfo::DataLocation); }

QString ApplicationInfo::vCardDir() { return makeSubprofilePath("vcard", ApplicationInfo::CacheLocation); }

QString ApplicationInfo::bobDir() { return makeSubhomePath("bob", ApplicationInfo::CacheLocation); }

QString ApplicationInfo::documentsDir()
{
    QString docDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/" + name());
    QDir    d(docDir);
    if (!d.exists()) {
        if (!d.mkpath("."))
            return QString();
    }
    return docDir;
}

QString ApplicationInfo::profilesDir(ApplicationInfo::HomedirType type) { return makeSubhomePath("profiles", type); }

QString ApplicationInfo::currentProfileDir(ApplicationInfo::HomedirType type)
{
    return pathToProfile(activeProfile, type);
}

QString ApplicationInfo::desktopFile()
{
    QString       dFile;
    const QString _desktopFile(xstr(APP_PREFIX) "/share/applications/" xstr(APP_BIN_NAME) ".desktop");
    QFile         f(_desktopFile);
    if (f.open(QIODevice::ReadOnly)) {
        dFile = QString::fromUtf8(f.readAll());
    }
    return dFile;
}

bool ApplicationInfo::isPortable()
{
    static bool portable
        = QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLower().indexOf("portable") != -1;
    return portable;
}
