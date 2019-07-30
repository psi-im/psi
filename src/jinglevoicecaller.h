/*
 * jinglevoicecaller.h
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2006  Remko Troncon
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

#ifndef JINGLEVOICECALLER_H
#define JINGLEVOICECALLER_H

#include <QMap>

#include "voicecaller.h"

namespace cricket {
    class BasicPortAllocator;
    class Call;
    class NetworkManager;
    class PhoneSessionClient;
    class SessionManager;
    class SocketAddress;
    class SocketServer;
    class Thread;
}

namespace XMPP {
    class Jid;
}
using namespace XMPP;

class JingleCallSlots;
class JingleClientSlots;
class PsiAccount;

class JingleVoiceCaller : public VoiceCaller
{
    Q_OBJECT

    friend class JingleClientSlots;

public:
    JingleVoiceCaller(PsiAccount* account);
    ~JingleVoiceCaller();

    virtual bool calling(const Jid&);

    virtual void initialize();
    virtual void deinitialize();

    virtual void call(const Jid&);
    virtual void accept(const Jid&);
    virtual void reject(const Jid&);
    virtual void terminate(const Jid&);

protected:
    void sendStanza(const char*);
    void registerCall(const Jid&, cricket::Call*);
    void removeCall(const Jid&);

protected slots:
    void receiveStanza(const QString&);

private:
    bool initialized_;
    static cricket::SocketServer *socket_server_;
    static cricket::Thread *thread_;
    static cricket::NetworkManager *network_manager_;
    static cricket::BasicPortAllocator *port_allocator_;
    static cricket::SocketAddress *stun_addr_;
    cricket::SessionManager *session_manager_;
    cricket::PhoneSessionClient *phone_client_;
    JingleClientSlots *slots_;
    QMap<QString,cricket::Call*> calls_;
};

#endif
