#include "whatismyip.h"

#include <iris/netnames.h>
#include <iris/stunmessage.h>
#include <iris/stuntransaction.h>
#include <iris/stunbinding.h>

class WhatIsMyIp::Private : public QObject
{
	Q_OBJECT

public:
	WhatIsMyIp *q;
	QString host;
	int port;
	int localPort;
	QHostAddress addr;
	QUdpSocket *sock;
	XMPP::StunTransactionPool *pool;
	XMPP::StunBinding *binding;
	XMPP::NameResolver dns;

	Private(WhatIsMyIp *_q) :
		QObject(_q),
		q(_q),
		localPort(-1),
		dns(this)
	{
		connect(&dns, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)), SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
		connect(&dns, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));
	}

	~Private()
	{
	}

	void start()
	{
		do_resolve();
	}

	void do_resolve()
	{
		dns.start(host.toLatin1(), XMPP::NameRecord::A);
	}

	void do_stun()
	{
		sock = new QUdpSocket(this);
		connect(sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));

		pool = new XMPP::StunTransactionPool(XMPP::StunTransaction::Udp, this);
		connect(pool, SIGNAL(outgoingMessage(const QByteArray &, const QHostAddress &, int)), SLOT(pool_outgoingMessage(const QByteArray &, const QHostAddress &, int)));

		if(!sock->bind(localPort != -1 ? localPort : 0))
		{
			//printf("Error binding to local port.\n");
			//emit quit();
			emit q->addressReady(QHostAddress());
			return;
		}

		//printf("Bound to local port %d.\n", sock->localPort());

		binding = new XMPP::StunBinding(pool);
		connect(binding, SIGNAL(success()), SLOT(binding_success()));
		connect(binding, SIGNAL(error(XMPP::StunBinding::Error)), SLOT(binding_error(XMPP::StunBinding::Error)));
		binding->start();
	}

private slots:
	void dns_resultsReady(const QList<XMPP::NameRecord> &results)
	{
		addr = results.first().address();

		do_stun();
	}

	void dns_error(XMPP::NameResolver::Error e)
	{
		Q_UNUSED(e);
		//printf("Unable to resolve stun host.\n");
		//emit quit();
		emit q->addressReady(QHostAddress());
	}

	void sock_readyRead()
	{
		while(sock->hasPendingDatagrams())
		{
			QByteArray buf(sock->pendingDatagramSize(), 0);
			QHostAddress from;
			quint16 fromPort;

			sock->readDatagram(buf.data(), buf.size(), &from, &fromPort);
			if(from == addr && fromPort == port)
			{
				processDatagram(buf);
			}
			else
			{
				//printf("Response from unknown sender %s:%d, dropping.\n", qPrintable(from.toString()), fromPort);
			}
		}
	}

	void pool_outgoingMessage(const QByteArray &packet, const QHostAddress &toAddr, int toPort)
	{
		Q_UNUSED(toAddr);
		Q_UNUSED(toPort);
	
		sock->writeDatagram(packet, addr, port);
	}

	void binding_success()
	{
		QHostAddress saddr = binding->reflexiveAddress();
		//quint16 sport = binding->reflexivePort();
		printf("Server says we are %s\n", qPrintable(saddr.toString()));
		//emit quit();
		// TODO: DOR-SS
		emit q->addressReady(saddr);
	}

	void binding_error(XMPP::StunBinding::Error e)
	{
		Q_UNUSED(e);
		//printf("Error: %s\n", qPrintable(binding->errorString()));
		//emit quit();
		// TODO: DOR-SS
		emit q->addressReady(QHostAddress());
	}

private:
	void processDatagram(const QByteArray &buf)
	{
		XMPP::StunMessage message = XMPP::StunMessage::fromBinary(buf);
		if(message.isNull())
		{
			//printf("Warning: server responded with what doesn't seem to be a STUN packet, skipping.\n");
			return;
		}

		if(!pool->writeIncomingMessage(message))
		{
			//printf("Warning: received unexpected message, skipping.\n");
		}
	}
};

WhatIsMyIp::WhatIsMyIp(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

WhatIsMyIp::~WhatIsMyIp()
{
	delete d;
}

void WhatIsMyIp::start(const QString &stunHost, int stunPort)
{
	d->host = stunHost;
	d->port = stunPort;
	d->start();
}

#include "whatismyip.moc"
