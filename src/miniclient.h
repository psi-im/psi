/*
 * miniclient.h
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef MINICLIENT_H
#define MINICLIENT_H

#include <QObject>
#include <QString>

#include "xmpp_jid.h"

namespace XMPP {
	class Client;
	class ClientStream;
	class AdvancedConnector;
	class QCATLSHandler;
}
namespace QCA {
	class TLS;
}
class ProxyManager;
class QString;
class QByteArray;

class MiniClient : public QObject
{
	Q_OBJECT
public:
	MiniClient(QObject *parent=0);
	~MiniClient();

	void reset();
	void connectToServer(const XMPP::Jid &j, bool legacy_ssl_probe, bool legacy_ssl, bool force_ssl, const QString &host, int port, ProxyManager *pm, QString proxy, QString *pass = NULL);
	void close();
	XMPP::Client *client();
	void setErrorOnDisconnect(bool);

	QString tlsOverrideDomain;
	QByteArray tlsOverrideCert;
signals:
	void handshaken();
	void error();
	void disconnected();

private slots:
	void tls_handshaken();
	void cs_connected();
	void cs_securityLayerActivated(int);
	void cs_needAuthParams(bool, bool, bool);
	void cs_authenticated();
	void cs_connectionClosed();
	void cs_delayedCloseFinished();
	void cs_warning(int);
	void cs_error(int);
	void sessionStart_finished();

private:
	XMPP::AdvancedConnector *conn;
	XMPP::ClientStream *stream;
	QCA::TLS *tls;
	XMPP::QCATLSHandler *tlsHandler;
	XMPP::Client *_client;
	XMPP::Jid j;
	QString pass;
	bool auth, force_ssl, error_disconnect;
};


#endif
