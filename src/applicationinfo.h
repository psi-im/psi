#ifndef APPLICATIONINFO_H
#define APPLICATIONINFO_H

class QString;
class QStringList;

class ApplicationInfo
{
public:
	// Version info
	static QString name();
	static QString version();
	static QString capsNode();
	static QString capsVersion();
	static QString IPCName();

	// URLs
	static QString getAppCastURL();

	// Directories
	static QString homeDir();
	static QString resourcesDir();
	static QString libDir();
	static QString profilesDir();
	static QString makeSubhomePath(const QString &);
	static QString makeSubprofilePath(const QString &);
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
};

#endif
