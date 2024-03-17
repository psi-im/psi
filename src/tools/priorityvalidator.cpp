/*
 * priorityvalidator.cpp - XMPP priority validator
 * Copyright (C) 2010  Dmitriy.trt
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

#include "priorityvalidator.h"

QValidator::State PriorityValidator::validate(QString &input, int & /*pos*/) const
{
    if (input.isEmpty()) {
        return QValidator::Acceptable;
    } else if (input == "-") {
        return QValidator::Intermediate;
    } else {
        bool ok  = false;
        int  val = input.toInt(&ok);
        if (ok && val >= -128 && val <= 127) {
            return QValidator::Acceptable;
        } else {
            return QValidator::Invalid;
        }
    }
}
