/*
 * ahcexecutetask.h - Ad-Hoc Command Execute Task
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2005  Remko Troncon
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

#ifndef AHCEXECUTETASK_H
#define AHCEXECUTETASK_H

#include "ahcommand.h"
#include "xmpp_jid.h"
#include "xmpp_task.h"

class QDomElement;

class AHCExecuteTask : public XMPP::Task
{
public:
    AHCExecuteTask(const XMPP::Jid& j, const AHCommand&, XMPP::Task* t);

    void onGo();
    bool take(const QDomElement &x);

    inline const XMPP::Jid &receiver() const { return receiver_; }
    inline const AHCommand &resultCommand() const { return resultCommand_; }
    inline bool hasCommandPayload() const { return hasPayload_; } // true if result command has children

private:
    bool hasPayload_;
    XMPP::Jid receiver_;
    AHCommand command_;
    AHCommand resultCommand_;
};

#endif
