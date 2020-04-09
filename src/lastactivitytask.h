/*
 * lastactivitytask.h
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef LASTACTIVITYTASK_H
#define LASTACTIVITYTASK_H

#include "xmpp_jid.h"
#include "xmpp_task.h"

#include <QDateTime>
#include <QDomElement>
#include <QString>

class LastActivityTask : public XMPP::Task {
public:
    LastActivityTask(const XMPP::Jid &, Task *);

    void             onGo();
    bool             take(const QDomElement &);
    const XMPP::Jid &jid() const;

    const QString &  status() const;
    const QDateTime &time() const;

private:
    QDomElement iq_;
    XMPP::Jid   jid_;
    QDateTime   last_time_;
    QString     last_status_;
};

#endif // LASTACTIVITYTASK_H
