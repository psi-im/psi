#ifndef APPLICATIONINFOACCESSINGHOST_H
#define APPLICATIONINFOACCESSINGHOST_H

class QString;

struct Proxy {
	QString type;
	QString host;
	int port;
	QString user;
	QString pass;
};

class ApplicationInfoAccessingHost
{
public:
	enum HomedirType {
		ConfigLocation,
		DataLocation,
		CacheLocation
	};
	virtual ~ApplicationInfoAccessingHost() {}

	// Version info
	virtual QString appName() = 0;
	virtual QString appVersion() = 0;
	virtual QString appCapsNode() = 0;
	virtual QString appCapsVersion() = 0;
	virtual QString appOsName() = 0;

	// Directories
	virtual QString appHomeDir(HomedirType type) = 0;
	virtual QString appResourcesDir() = 0;
	virtual QString appLibDir() = 0;
	virtual QString appProfilesDir(HomedirType type) = 0;
	virtual QString appHistoryDir() = 0;
	virtual QString appCurrentProfileDir(HomedirType type) = 0;
	virtual QString appVCardDir() = 0;

	virtual Proxy getProxyFor(const QString& obj) = 0;
};

Q_DECLARE_INTERFACE(ApplicationInfoAccessingHost, "org.psi-im.ApplicationInfoAccessingHost/0.1");

#endif // APPLICATIONINFOACCESSINGHOST_H
