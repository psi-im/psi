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

#include <QByteArray>
#include <tuple>

namespace Http {

enum ParseResult {
    Parsed,
    Unparsed,
    NotImplementedRangeType,
    NotImplementedTailLoad,
    NotImplementedMultirange,
    OutOfRange
};

/**
 * @brief parseRangeHeader parses http bytes uni-"Range" header
 * @param value    - heder value
 * @param fileSize - if destination file size is know it will be checked. set -1 when not known
 * @return (result, start, size)
 *
 * While it's allowed by standard this parser will return error is start is not set in the header
 */
std::tuple<ParseResult, qint64, qint64> parseRangeHeader(const QByteArray &value, qint64 fileSize = -1);
std::tuple<bool, qint64, qint64>        parseContentRangeHeader(const QByteArray &value);

}
