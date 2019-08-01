/*
 * ahcservermanager.h - Server implementation of XEP-50 (Ad-Hoc Commands)
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

#ifndef AHCSERVERMANAGER_H
#define AHCSERVERMANAGER_H

#include <QList>

namespace XMPP {
    class Jid;
}

class AHCommand;
class AHCommandServer;
class JT_AHCServer;
class PsiAccount;
class QString;

class AHCServerManager
{
public:
    AHCServerManager(PsiAccount* pa);
    void addServer(AHCommandServer*);
    void removeServer(AHCommandServer*);

    typedef QList<AHCommandServer*> ServerList;
    ServerList commands(const XMPP::Jid&) const;
    void execute(const AHCommand& command, const XMPP::Jid& requester, QString id);
    PsiAccount* account() const { return pa_; }
    bool hasServer(const QString& node, const XMPP::Jid&) const;

protected:
    AHCommandServer* findServer(const QString& node) const;

private:
    PsiAccount* pa_;
    JT_AHCServer* server_task_;
    ServerList servers_;
};

#endif // AHCSERVERMANAGER_H
