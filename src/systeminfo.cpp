#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSysInfo>
#include <QProcess>
#include <QTextStream>

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#endif

#ifdef Q_WS_WIN
#include <windows.h>
#endif

#include "systeminfo.h"

#if defined(Q_WS_X11)
static QString lsbRelease(const QStringList& args)
{
	QStringList path = QString(qgetenv("PATH")).split(':');
	QString found;
	
	foreach(QString dirname, path) {
		QDir dir(dirname);
		QFileInfo cand(dir.filePath("lsb_release"));
		if (cand.isExecutable()) {
			found = cand.absoluteFilePath();
			break;
		}
	}

	if (found.isEmpty()) {
		return QString();
	}

	QProcess process;
	process.start(found, args, QIODevice::ReadOnly);

	if(!process.waitForStarted())
		return QString();   // process failed to start

    QTextStream stream(&process);
	QString ret;

	while(process.waitForReadyRead())
	   ret += stream.readAll();

	process.close();
	return ret.trimmed();
}


static QString unixHeuristicDetect() {
	
	QString ret;
	
	struct utsname u;
	uname(&u);
	ret.sprintf("%s", u.sysname);

	// get description about os
	enum LinuxName {
		LinuxNone = 0,

		LinuxMandrake,
		LinuxDebian,
		LinuxRedHat,
		LinuxGentoo,
		LinuxSlackware,
		LinuxSuSE,
		LinuxConectiva,
		LinuxCaldera,
		LinuxLFS,

		LinuxASP, // Russian Linux distros
		LinuxALT,

		LinuxPLD, // Polish Linux distros
		LinuxAurox,
		LinuxArch
	};

	enum OsFlags {
		OsUseName = 0,
		OsUseFile,
		OsAppendFile
	};

	struct OsInfo {
		LinuxName id;
		OsFlags flags;
		QString file;
		QString name;
	} osInfo[] = {
		{ LinuxMandrake,	OsUseFile,	"/etc/mandrake-release",	"Mandrake Linux"	},
		{ LinuxDebian,		OsAppendFile,	"/etc/debian_version",		"Debian GNU/Linux"	},
		{ LinuxGentoo,		OsUseFile,	"/etc/gentoo-release",		"Gentoo Linux"		},
		{ LinuxSlackware,	OsAppendFile,	"/etc/slackware-version",	"Slackware Linux"	},
		{ LinuxPLD,		OsUseFile,	"/etc/pld-release",		"PLD Linux"		},
		{ LinuxAurox,		OsUseName,	"/etc/aurox-release",		"Aurox Linux"		},
		{ LinuxArch,		OsUseFile,	"/etc/arch-release",		"Arch Linux"		},
		{ LinuxLFS,		OsAppendFile,	"/etc/lfs-release",		"LFS Linux"		},

		// untested
		{ LinuxSuSE,		OsUseFile,	"/etc/SuSE-release",		"SuSE Linux"		},
		{ LinuxConectiva,	OsUseFile,	"/etc/conectiva-release",	"Conectiva Linux"	},
		{ LinuxCaldera,		OsUseFile,	"/etc/.installed",		"Caldera Linux"		},

		// many distros use the /etc/redhat-release for compatibility, so RedHat will be the last :)
		{ LinuxRedHat,		OsUseFile,	"/etc/redhat-release",		"RedHat Linux"		},

		{ LinuxNone,		OsUseName,	"",				""			}
	};

	for (int i = 0; osInfo[i].id != LinuxNone; i++) {
		QFileInfo fi( osInfo[i].file );
		if ( fi.exists() ) {
			char buffer[128];

			QFile f( osInfo[i].file );
			f.open( QIODevice::ReadOnly );
			f.readLine( buffer, 128 );
			QString desc(buffer);

			desc = desc.stripWhiteSpace ();

			switch ( osInfo[i].flags ) {
				case OsUseFile:
					ret = desc;
					break;
				case OsUseName:
					ret = osInfo[i].name;
					break;
				case OsAppendFile:
					ret = osInfo[i].name + " (" + desc + ")";
					break;
			}

			break;
		}
	}
	return ret;
}
#endif



SystemInfo::SystemInfo() : QObject(QCoreApplication::instance())
{
	// Initialize
	timezone_offset_ = 0;
	timezone_str_ = "N/A";
	os_str_ = "Unknown";
	
	// Detect
#if defined(Q_WS_X11) || defined(Q_WS_MAC)
	time_t x;
	time(&x);
	char str[256];
	char fmt[32];
	strcpy(fmt, "%z");
	strftime(str, 256, fmt, localtime(&x));
	if(strcmp(fmt, str)) {
		QString s = str;
		if(s.at(0) == '+')
			s.remove(0,1);
		s.truncate(s.length()-2);
		timezone_offset_ = s.toInt();
	}
	strcpy(fmt, "%Z");
	strftime(str, 256, fmt, localtime(&x));
	if(strcmp(fmt, str))
		timezone_str_ = str;
#endif
#if defined(Q_WS_X11)
	// attempt to get LSB version before trying the distro-specific approach
	os_str_ = lsbRelease(QStringList() << "--description" << "--short");

	if(os_str_.isEmpty()) {
		os_str_ = unixHeuristicDetect();
	}

#elif defined(Q_WS_MAC)
	QSysInfo::MacVersion v = QSysInfo::MacintoshVersion;
	if(v == QSysInfo::MV_10_3)
		os_str_ = "Mac OS X 10.3";
	else if(v == QSysInfo::MV_10_4)
		os_str_ = "Mac OS X 10.4";
	else if(v == QSysInfo::MV_10_5)
		os_str_ = "Mac OS X 10.5";
	else
		os_str_ = "Mac OS X";
#endif

#if defined(Q_WS_WIN)
	TIME_ZONE_INFORMATION i;
	//GetTimeZoneInformation(&i);
	//timezone_offset_ = (-i.Bias) / 60;
	memset(&i, 0, sizeof(i));
	bool inDST = (GetTimeZoneInformation(&i) == TIME_ZONE_ID_DAYLIGHT);
	int bias = i.Bias;
	if(inDST)
		bias += i.DaylightBias;
	timezone_offset_ = (-bias) / 60;
	timezone_str_ = "";
	for(int n = 0; n < 32; ++n) {
		int w = inDST ? i.DaylightName[n] : i.StandardName[n];
		if(w == 0)
			break;
		timezone_str_ += QChar(w);
	}

	QSysInfo::WinVersion v = QSysInfo::WindowsVersion;
	if(v == QSysInfo::WV_95)
		os_str_ = "Windows 95";
	else if(v == QSysInfo::WV_98)
		os_str_ = "Windows 98";
	else if(v == QSysInfo::WV_Me)
		os_str_ = "Windows Me";
	else if(v == QSysInfo::WV_DOS_based)
		os_str_ = "Windows 9x/Me";
	else if(v == QSysInfo::WV_NT)
		os_str_ = "Windows NT 4.x";
	else if(v == QSysInfo::WV_2000)
		os_str_ = "Windows 2000";
	else if(v == QSysInfo::WV_XP)
		os_str_ = "Windows XP";
	else if(v == QSysInfo::WV_2003)
		os_str_ = "Windows Server 2003";
	else if(v == QSysInfo::WV_VISTA)
		os_str_ = "Windows Vista";
	else if(v == QSysInfo::WV_NT_based)
		os_str_ = "Windows NT";
#endif
}

SystemInfo* SystemInfo::instance()
{
	if (!instance_) 
		instance_ = new SystemInfo();
	return instance_;
}

SystemInfo* SystemInfo::instance_ = NULL;
