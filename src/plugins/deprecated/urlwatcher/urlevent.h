/*
 * urlevent.h - simple URL container
 * Copyright (C) 2006  Kevin Smith
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
#ifndef URLEVENT_H
#define URLEVENT_H

#include "QtCore"

class URLEvent : public QObject {
    Q_OBJECT
public:
    URLEvent(QString sender, QString url, QObject *parent = 0) : QObject(parent)
    {
        sender_ = sender;
        url_    = url;
        viewed_ = false;
    }
    ~URLEvent() {};

    QString sender_;
    QString url_;
    bool    viewed_;
};

#endif // URLEVENT_H
