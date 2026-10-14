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
#include <algorithm>
#include <limits>

namespace Http {

std::tuple<ParseResult, quint64, quint64> parseRangeHeader(const QByteArray &rangesBa, std::optional<quint64> fileSize)
{
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

    auto decimal = [](const QByteArray &number) {
        return !number.isEmpty()
            && std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    if (l[0].isEmpty()) {
        if (!decimal(l[1]))
            return { Unparsed, 0, 0 };
        const auto suffix = l[1].toULongLong(&ok);
        if (!ok)
            return { Unparsed, 0, 0 };
        if (!suffix || (fileSize && !*fileSize))
            return { OutOfRange, 0, 0 };
        if (!fileSize)
            return { NotImplementedTailLoad, 0, 0 };
        const auto length = std::min(suffix, *fileSize);
        return { Parsed, *fileSize - length, length };
    }

    if (!decimal(l[0]) || (!l[1].isEmpty() && !decimal(l[1])))
        return { Unparsed, 0, 0 };
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
    if (fileSize)
        end = end ? std::min(*end, *fileSize - 1) : *fileSize - 1;
    if (end && *end - start == std::numeric_limits<quint64>::max())
        return { Unparsed, 0, 0 };
    return { Parsed, start, end ? (*end - start + 1) : 0 };
}

std::optional<std::tuple<quint64, quint64, std::optional<quint64>>> parseContentRangeHeader(const QByteArray &value)
{
    auto arr = value.split(' ');
    if (arr.size() != 2 || arr[0] != "bytes" || (arr = arr[1].split('/')).size() != 2)
        return {};

    quint64                start, end;
    std::optional<quint64> totalSize;
    bool                   ok;

    if (arr[1] != "*") {
        totalSize = arr[1].toULongLong(&ok);
        if (!ok) {
            return {};
        }
    }

    arr = arr[0].split('-');
    if (arr.size() == 2) {
        start = arr[0].toULongLong(&ok);
        if (ok) {
            end = arr[1].toULongLong(&ok);
            if (ok && start <= end) {
                if ((!totalSize || end < *totalSize) && end - start != std::numeric_limits<quint64>::max()) {
                    return std::make_tuple(start, end - start + 1, totalSize);
                }
            }
        }
    }
    return {};
}

}
