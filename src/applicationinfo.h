#ifndef APPLICATIONINFO_H
#define APPLICATIONINFO_H

class QString;

class ApplicationInfo
{
public:
	// Version info
	static QString name();
	static QString version();
	static QString capsNode();
	static QString capsVersion();
	static QString IPCName();

	// Directories
	static QString homeDir();
	static QString resourcesDir();
	static QString profilesDir();
	static QString historyDir();
	static QString vCardDir();
	
	// Namespaces
	static QString optionsNS();
	static QString storageNS();
};

#endif
