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

std::tuple<ParseResult, quint64, quint64> parseRangeHeader(const QByteArray &rangesBa, std::optional<quint64> fileSize)
{
    bool    isRanged       = false;
    quint64 requestedStart = 0;
    quint64 requestedSize  = 0;

    if (!rangesBa.startsWith("bytes=")) {
        return { NotImplementedRangeType, 0, 0 };
    }

    if (rangesBa.indexOf(',') != -1) {
        return { NotImplementedMultirange, 0, 0 };
    }

    auto ba = QByteArray::fromRawData(rangesBa.data() + sizeof("bytes"), rangesBa.size() - int(sizeof("bytes")));

    auto l = ba.trimmed().split('-');
    if (l.size() != 2) {
        return { Unparsed, 0, 0 };
    }

    bool                   ok;
    quint64                start;
    std::optional<quint64> end;

    if (!l[0].size()) { // bytes from the end are requested. Jingle-ft doesn't support this
        return { NotImplementedTailLoad, 0, 0 };
    }

    start = l[0].toULongLong(&ok);
    if (!ok) {
        return { Unparsed, 0, 0 };
    }
    if (l[1].size()) {               // if we have end
        end = l[1].toULongLong(&ok); // then parse it
        if (!ok || start > *end) {   // if something not parsed or range is invalid
            return { Unparsed, 0, 0 };
        }
    }

    if (fileSize && start >= *fileSize) {
        return { OutOfRange, 0, 0 };
    }
    return { Parsed, start, end ? (*end - start + 1) : 0 };
}

std::optional<std::tuple<quint64, quint64, std::optional<quint64>>> parseContentRangeHeader(const QByteArray &value)
{
    auto arr = value.split(' ');
    if (arr.size() != 2 || arr[0] != "bytes" || (arr = arr[1].split('/')).size() != 2)
        return {};

    qint64                 start, end;
    std::optional<quint64> totalSize;
    bool                   ok;

    if (arr[1] != "*") {
        totalSize = arr[1].toULongLong(&ok);
        if (!ok) {
            return {};
        }
    }

    arr = arr[0].split('-');
    if (arr.size() != 2) {
        start = arr[0].toULongLong(&ok);
        if (ok) {
            end = arr[0].toULongLong(&ok);
            if (ok && start <= end) {
                if (!totalSize || (start < *totalSize && end < *totalSize)) {
                    return std::make_tuple(start, end - start + 1, totalSize);
                }
            }
        }
    }
    return {};
}

}
