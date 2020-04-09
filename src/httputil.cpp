/*
 * httpuil.cpp - http related utilities
 * Copyright (C) 2019  Sergey Ilinykh
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

#include "httputil.h"

#include <QList>

namespace Http {

std::tuple<ParseResult, qint64, qint64> parseRangeHeader(const QByteArray &rangesBa, qint64 fileSize)
{
    bool   isRanged       = false;
    qint64 requestedStart = 0;
    qint64 requestedSize  = 0;

    if (!rangesBa.startsWith("bytes=")) {
        return std::tuple<ParseResult, qint64, qint64>(NotImplementedRangeType, 0, 0);
    }

    if (rangesBa.indexOf(',') != -1) {
        return std::tuple<ParseResult, qint64, qint64>(NotImplementedMultirange, 0, 0);
    }

    auto ba = QByteArray::fromRawData(rangesBa.data() + sizeof("bytes"), rangesBa.size() - int(sizeof("bytes")));

    auto l = ba.trimmed().split('-');
    if (l.size() != 2) {
        return std::tuple<ParseResult, qint64, qint64>(Unparsed, 0, 0);
    }

    bool   ok;
    qint64 start;
    qint64 end;

    if (!l[0].size()) { // bytes from the end are requested. Jingle-ft doesn't support this
        return std::tuple<ParseResult, qint64, qint64>(NotImplementedTailLoad, 0, 0);
    }

    start = l[0].toLongLong(&ok);
    if (!ok) {
        return std::tuple<ParseResult, qint64, qint64>(Unparsed, 0, 0);
    }
    if (l[1].size()) {              // if we have end
        end = l[1].toLongLong(&ok); // then parse it
        if (!ok || start > end) {   // if something not parsed or range is invalid
            return std::tuple<ParseResult, qint64, qint64>(Unparsed, 0, 0);
        }

        if (fileSize == -1 || start < fileSize) {
            isRanged       = true;
            requestedStart = start;
            requestedSize  = end - start + 1;
        }
    } else { // no end. all the remaining
        if (fileSize == -1 || start < fileSize) {
            isRanged       = true;
            requestedStart = start;
            requestedSize  = 0;
        }
    }

    if (fileSize >= 0 && !isRanged) { // isRanged is not set. So it doesn't fit
        return std::tuple<ParseResult, qint64, qint64>(OutOfRange, 0, 0);
    }

    return std::tuple<ParseResult, qint64, qint64>(Parsed, requestedStart, requestedSize);
}

std::tuple<bool, qint64, qint64> parseContentRangeHeader(const QByteArray &value)
{
    auto arr = value.split(' ');
    if (arr.size() != 2 || arr[0] != "bytes" || (arr = arr[1].split('-')).size() != 2)
        return std::tuple<bool, qint64, qint64>(false, 0, 0);
    qint64 start, size;
    bool   ok;
    start = arr[0].toLongLong(&ok);
    if (ok) {
        arr  = arr[1].split('/');
        size = arr[0].toLongLong(&ok) - start + 1;
    }
    if (!ok || size <= 0)
        return std::tuple<bool, qint64, qint64>(false, 0, 0);
    return std::tuple<bool, qint64, qint64>(true, start, size);
}

}
