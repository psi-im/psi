#ifndef APPLICATIONINFO_H
#define APPLICATIONINFO_H

class QString;
class QStringList;
class QLatin1String;

class ApplicationInfo
{
public:
	enum HomedirType {
		ConfigLocation,
		DataLocation,
		CacheLocation
	};

	// Version info
	static QString name();
	static QLatin1String sname();
	static QString version();
	static QString capsNode();
	static QString osName();
	static QString IPCName();

	// URLs
	static QString getAppCastURL();

	// Directories
	static QString homeDir(HomedirType type);
	static QString resourcesDir();
	static QString libDir();
	static QString profilesDir(ApplicationInfo::HomedirType);
	static QString currentProfileDir(HomedirType type);
	static QString makeSubhomePath(const QString &, HomedirType type);
	static QString makeSubprofilePath(const QString &, HomedirType type);
	static QString historyDir();
	static QString vCardDir();
	static QString bobDir();
	static QStringList getCertificateStoreDirs();
	static QString getCertificateStoreSaveDir();
	static QStringList dataDirs();

	// Namespaces
	static QString optionsNS();
	static QString storageNS();
	static QString fileCacheNS();

	// Common
	static QString desktopFile();
};

#endif
