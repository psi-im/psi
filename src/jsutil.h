/*
 * jsutil.h
 * Copyright (C) 2001-2019  Psi Team
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef JSUTIL
#define JSUTIL

class QString;
class QVariant;

#include <QVariantMap>

class JSUtil
{
public:
    static QString variant2js(const QVariant &);
    /** Escapes " and \n  (for JS evaluation) */
    static void escapeString(QString &str);

    /** Escapes " and \n  (for JS evaluation) [overload] */
    static inline QString escapeStringCopy(QString str)
    {
        escapeString(str);
        return str;
    }
};

#endif
