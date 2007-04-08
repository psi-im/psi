#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>

#if defined(Q_WS_X11) || defined(Q_WS_MAC)
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#endif

#include "systeminfo.h"

SystemInfo::SystemInfo()
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
	struct utsname u;
	uname(&u);
	os_str_.sprintf("%s", u.sysname);

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
					os_str_ = desc;
					break;
				case OsUseName:
					os_str_ = osInfo[i].name;
					break;
				case OsAppendFile:
					os_str_ = osInfo[i].name + " (" + desc + ")";
					break;
			}

			break;
		}
	}
#elif defined(Q_WS_MAC)
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
