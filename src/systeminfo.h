/*
 * Copyright (C) 2007-2008  Psi Development Team
 * Licensed under the GNU General Public License.
 * See the COPYING file for more information.
 */

#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>
#include <QString>

class SystemInfo : public QObject
{
public:
	static SystemInfo* instance();
	const QString& os() const { return os_str_; }
	int timezoneOffset() const { return timezone_offset_; }
	const QString& timezoneString() const { return timezone_str_; }

private:
	SystemInfo();

	static SystemInfo* instance_;
	int timezone_offset_;
	QString timezone_str_;
	QString os_str_;
};

#endif
