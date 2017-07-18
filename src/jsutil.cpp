/*
 * jsutil.cpp
 * Copyright (C) 2009  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "jsutil.h"
#include <QStringList>
#include <QDateTime>

QString JSUtil::map2json(const QVariantMap &map)
{
	QStringList ret;
	QMapIterator<QString, QVariant> i(map);
	while (i.hasNext()) {
		i.next();
		ret.append(QString("\"%1\":").arg(escapeStringCopy(i.key())) +
				   variant2js(i.value()));
	}
	return QString("{%1}").arg(ret.join(","));
}

QString JSUtil::variant2js(const QVariant &value)
{
	QString strVal;
	switch (value.type()) {
		case QVariant::String:
		case QVariant::Color:
			strVal = value.toString();
			escapeString(strVal);
			strVal = QString("\"%1\"").arg(strVal);
			break;
		case QVariant::StringList:
			{
				QStringList sl = value.toStringList();
				for (int i=0; i<sl.count(); i++) {
					escapeString(sl[i]);
					sl[i] = QString("\"%1\"").arg(sl[i]);
				}
				strVal = QString("[%1]").arg(sl.join(","));
			}
			break;
		case QVariant::List:
			{
				QStringList sl;
				auto vl = value.toList();
				sl.reserve(vl.size());
				for (auto &item: vl) {
					sl.append(variant2js(item));
				}
				strVal = QString("[%1]").arg(sl.join(","));
			}
			break;
		case QVariant::DateTime:
			strVal = QString("new Date(%1)").arg(value.toDateTime().toString("yyyy,M-1,d,h,m,s"));
			break;
		case QVariant::Date:
			strVal = QString("new Date(%1)").arg(value.toDate().toString("yyyy,M-1,d"));
			break;
		case QVariant::Map:
			strVal = map2json(value.toMap());
			break;
		default:
			strVal = value.toString();
	}
	return strVal;
}

void JSUtil::escapeString(QString& str)
{

	str.replace("\r\n", "\n");  //windows
	str.replace("\r", "\n");    //mac
	str.replace("\\", "\\\\");
	str.replace("\"", "\\\"");
	str.replace("\n", "\\\n");
	str.replace(QChar(8232), "\\\n"); //ctrl+enter
}
