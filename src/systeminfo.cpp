/*
 * Copyright (C) 2007-2008  Psi Development Team
 * Licensed under the GNU General Public License.
 * See the COPYING file for more information.
 */

#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSysInfo>
#include <QProcess>
#include <QTextStream>
#include <QByteArray>

#if defined(HAVE_X11) || defined(Q_OS_MAC)
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "systeminfo.h"

#if defined(HAVE_X11)
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
		LinuxExherbo,
		LinuxSlackware,
		LinuxSuSE,
		LinuxConectiva,
		LinuxCaldera,
		LinuxLFS,

		LinuxASP, // Russian Linux distros
		LinuxALT,
		LinuxRFRemix,

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
		{ LinuxExherbo,		OsUseName,	"/etc/exherbo-release",		"Exherbo Linux"		},
		{ LinuxArch,		OsUseName,	"/etc/arch-release",		"Arch Linux"		},
		{ LinuxSlackware,	OsAppendFile,	"/etc/slackware-version",	"Slackware Linux"	},
		{ LinuxPLD,		OsUseFile,	"/etc/pld-release",		"PLD Linux"		},
		{ LinuxAurox,		OsUseName,	"/etc/aurox-release",		"Aurox Linux"		},
		{ LinuxArch,		OsUseFile,	"/etc/arch-release",		"Arch Linux"		},
		{ LinuxLFS,		OsAppendFile,	"/etc/lfs-release",		"LFS Linux"		},
		{ LinuxRFRemix,		OsUseFile,	"/etc/rfremix-release",		"RFRemix Linux"		},

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
			QString desc = QString::fromUtf8(buffer);

			desc = desc.trimmed();

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
	os_str_ = "Unknown";

	// Detect
#if defined(HAVE_X11)
	// attempt to get LSB version before trying the distro-specific approach
	os_str_ = lsbRelease(QStringList() << "--description" << "--short");

	if(os_str_.isEmpty()) {
		os_str_ = unixHeuristicDetect();
	}

#elif defined(Q_OS_MAC)
	QSysInfo::MacVersion v = QSysInfo::MacintoshVersion;
	switch (v) {
		case QSysInfo::MV_10_3:
			os_str_ = "Mac OS X 10.3 (Panther)";
			break;
		case QSysInfo::MV_10_4:
			os_str_ = "Mac OS X 10.4 (Tiger)";
			break;
		case QSysInfo::MV_10_5:
			os_str_ = "Mac OS X 10.5 (Leopard)";
			break;
		case QSysInfo::MV_10_6:
			os_str_ = "Mac OS X 10.6 (Snow Leopard)";
			break;
		case QSysInfo::MV_10_7:
			os_str_ = "OS X 10.7 (Lion)";
			break;
		case 0x000A: // QSysInfo::MV_10_8 should not be used for compatibility reasons
			os_str_ = "OS X 10.8 (Mountain Lion)";
			break;
		case 0x000B: // QSysInfo::MV_10_9 should not be used for compatibility reasons
			os_str_ = "OS X 10.9 (Mavericks)";
			break;
		case 0x000C: // QSysInfo::MV_10_10 should not be used for compatibility reasons
			os_str_ = "OS X 10.10 (Yosemite)";
			break;
		default:
			os_str_ = "Mac OS X";
	}
#endif

#if defined(Q_OS_WIN)

	QSysInfo::WinVersion v = QSysInfo::WindowsVersion;
	switch (v) {
		case QSysInfo::WV_95:
			os_str_ = "Windows 95";
			break;
		case QSysInfo::WV_98:
			os_str_ = "Windows 98";
			break;
		case QSysInfo::WV_Me:
			os_str_ = "Windows Me";
			break;
		case QSysInfo::WV_DOS_based:
			os_str_ = "Windows 9x/Me";
			break;
		case QSysInfo::WV_NT:
			os_str_ = "Windows NT 4.x";
			break;
		case QSysInfo::WV_2000:
			os_str_ = "Windows 2000";
			break;
		case QSysInfo::WV_XP:
			os_str_ = "Windows XP";
			break;
		case QSysInfo::WV_2003:
			os_str_ = "Windows Server 2003";
			break;
		case QSysInfo::WV_VISTA:
			os_str_ = "Windows Vista";
			break;
		case QSysInfo::WV_WINDOWS7:
			os_str_ = "Windows 7";
			break;
		case 0x00a0: // QSysInfo::WV_WINDOWS8 should not be used for compatibility reasons
			os_str_ = "Windows 8";
			break;
		case 0x00b0: // QSysInfo::WV_WINDOWS8_1 should not be used for compatibility reasons
			os_str_ = "Windows 8.1";
			break;
		case 0x00c0: // QSysInfo::WV_WINDOWS10 should not be used for compatibility reasons
			os_str_ = "Windows 10";
			break;
		case QSysInfo::WV_NT_based:
			os_str_ = "Windows NT";
			break;
		// make compiler happy with unsupported Windows versions
		case QSysInfo::WV_32s:
		case QSysInfo::WV_CE:
		case QSysInfo::WV_CENET:
		case QSysInfo::WV_CE_5:
		case QSysInfo::WV_CE_6:
		case QSysInfo::WV_CE_based:
			break;
	}
#endif
}

SystemInfo* SystemInfo::instance()
{
	if (!instance_)
		instance_ = new SystemInfo();
	return instance_;
}

SystemInfo* SystemInfo::instance_ = NULL;
