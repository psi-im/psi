/*
 * googleftmanager.h
 * Copyright (C) 2007  Remko Troncon
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#ifndef GOOGLEFTMANAGER_H
#define GOOGLEFTMANAGER_H

#include "talk/base/scoped_ptr.h"
#include "filetransfer.h"

#include <QObject>
#include <QMap>

namespace cricket {
	class HttpPortAllocator;
	class SessionManager;
	class FileShareSessionClient;
	class FileShareSession;
}
namespace talk_base {
	class NetworkManager;
	class PhysicalSocketServer;
	class Thread;
}
namespace XMPP {
	class Jid;
	class Client;
}
class GoogleSessionListener;
class GoogleFileTransferListener;
class GoogleFTManager;

class GoogleFileTransfer /*: public XMPP::AbstractFileTransfer*/
{
	friend class GoogleFileTransferListener;

public: 
	GoogleFileTransfer(cricket::FileShareSession*, GoogleFTManager* manager);
	virtual ~GoogleFileTransfer() {};
	
	virtual XMPP::Jid peer() const;
	virtual QString fileName() const;
	virtual qlonglong fileSize() const;
	virtual QString description() const;
	virtual void accept(qlonglong offset=0, qlonglong length=0);

private:
	cricket::FileShareSession* session_;
	GoogleFTManager* manager_;
	GoogleFileTransferListener* listener_;
};

class GoogleFTManager : public QObject
{
	Q_OBJECT
		
	friend class GoogleSessionListener;
	friend class GoogleFileTransferListener;

public:
	GoogleFTManager(XMPP::Client* client);
	~GoogleFTManager();
	
signals:
	void incomingFileTransfer(GoogleFileTransfer*);

protected:
	void sendStanza(const QString&);

protected slots:
	void receiveStanza(const QString&);
	virtual void initialize();
	virtual void deinitialize();

private:
	bool initialized_;
	XMPP::Client* client_;
	GoogleSessionListener* listener_;

	static talk_base::PhysicalSocketServer *socket_server_;
	static talk_base::Thread *thread_;
 	static talk_base::NetworkManager* network_manager_;
 	static talk_base::scoped_ptr<cricket::HttpPortAllocator> port_allocator_;
 	talk_base::scoped_ptr<cricket::SessionManager> session_manager_;
 	talk_base::scoped_ptr<cricket::FileShareSessionClient> file_share_session_client_;
};

#endif
