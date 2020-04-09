/*
 * statuscombobox.h - helper class that displays available statuses using QComboBox
 * Copyright (C) 2008  Maciej Niedzielski
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

#ifndef STATUSCOMBOBOX_H
#define STATUSCOMBOBOX_H

#include "xmpp_status.h"

#include <QComboBox>

class StatusComboBox : public QComboBox {
    Q_OBJECT
public:
    StatusComboBox(QWidget *parent, XMPP::Status::Type type = XMPP::Status::Offline);

    void               setStatus(XMPP::Status::Type type);
    XMPP::Status::Type status() const;

signals:
    void statusChanged(XMPP::Status::Type type);

private:
    void addStatus(XMPP::Status::Type type);

private slots:
    void onCurrentIndexChanged(int);
};

#endif // STATUSCOMBOBOX_H
