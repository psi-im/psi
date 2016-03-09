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
	const QString& osName() const { return os_name_str_; }
	const QString& osVersion() const { return os_version_str_; }

private:
	SystemInfo();

	static SystemInfo* instance_;
	QString os_str_, os_name_str_, os_version_str_;
};

#endif
