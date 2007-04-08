#include <qapplication.h>
#include <qsocket.h>
#include <qserversocket.h>
#include <qvaluelist.h>
#include <qtimer.h>
#include <qca.h>
#include <stdlib.h>
#include <time.h>
#include "bsocket.h"
#include "xmpp.h"

#include <unistd.h>

char pemdata_cert[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDbjCCAtegAwIBAgIBADANBgkqhkiG9w0BAQQFADCBhzELMAkGA1UEBhMCVVMx\n"
	"EzARBgNVBAgTCkNhbGlmb3JuaWExDzANBgNVBAcTBklydmluZTEYMBYGA1UEChMP\n"
	"RXhhbXBsZSBDb21wYW55MRQwEgYDVQQDEwtleGFtcGxlLmNvbTEiMCAGCSqGSIb3\n"
	"DQEJARYTZXhhbXBsZUBleGFtcGxlLmNvbTAeFw0wMzA3MjQwNzMwMDBaFw0wMzA4\n"
	"MjMwNzMwMDBaMIGHMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEP\n"
	"MA0GA1UEBxMGSXJ2aW5lMRgwFgYDVQQKEw9FeGFtcGxlIENvbXBhbnkxFDASBgNV\n"
	"BAMTC2V4YW1wbGUuY29tMSIwIAYJKoZIhvcNAQkBFhNleGFtcGxlQGV4YW1wbGUu\n"
	"Y29tMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCobzCF268K2sRp473gvBTT\n"
	"4AgSL1kjeF8N57vxS1P8zWrWMXNs4LuH0NRZmKTajeboy0br8xw+smIy3AbaKAwW\n"
	"WZToesxebu3m9VeA8dqWyOaUMjoxAcgVYesgVaMpjRe7fcWdJnX1wJoVVPuIcO8m\n"
	"a+AAPByfTORbzpSTmXAQAwIDAQABo4HnMIHkMB0GA1UdDgQWBBTvFierzLmmYMq0\n"
	"cB/+5rK1bNR56zCBtAYDVR0jBIGsMIGpgBTvFierzLmmYMq0cB/+5rK1bNR566GB\n"
	"jaSBijCBhzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExDzANBgNV\n"
	"BAcTBklydmluZTEYMBYGA1UEChMPRXhhbXBsZSBDb21wYW55MRQwEgYDVQQDEwtl\n"
	"eGFtcGxlLmNvbTEiMCAGCSqGSIb3DQEJARYTZXhhbXBsZUBleGFtcGxlLmNvbYIB\n"
	"ADAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBAUAA4GBAGqGhXf7xNOnYNtFO7gz\n"
	"K6RdZGHFI5q1DAEz4hhNBC9uElh32XGX4wN7giz3zLC8v9icL/W4ff/K5NDfv3Gf\n"
	"gQe/+Wo9Be3H3ul6uwPPFnx4+PIOF2a5TW99H9smyxWdNjnFtcUte4al3RszcMWG\n"
	"x3iqsWosGtj6F+ridmKoqKLu\n"
	"-----END CERTIFICATE-----\n";

char pemdata_privkey[] =
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"MIICXAIBAAKBgQCobzCF268K2sRp473gvBTT4AgSL1kjeF8N57vxS1P8zWrWMXNs\n"
	"4LuH0NRZmKTajeboy0br8xw+smIy3AbaKAwWWZToesxebu3m9VeA8dqWyOaUMjox\n"
	"AcgVYesgVaMpjRe7fcWdJnX1wJoVVPuIcO8ma+AAPByfTORbzpSTmXAQAwIDAQAB\n"
	"AoGAP83u+aYghuIcaWhmM03MLf69z/WztKYSi/fu0BcS977w67bL3MC9CVPoPRB/\n"
	"0nLSt/jZIuRzHKUCYfXLerSU7v0oXDTy6GPzWMh/oXIrpF0tYNbwWF7LSq2O2gGZ\n"
	"XtA9MSmUNNJaKzQQeXjqdVFOY8A0Pho+k2KByBiCi+ChkcECQQDRUuyX0+PKJtA2\n"
	"M36BOTFpy61BAv+JRlXUnHuevOfQWl6NR6YGygqCyH1sWtP1sa9S4wWys3DFH+5A\n"
	"DkuAqk7zAkEAzf4eUH2hp5CIMsXH+WpIzKj09oY1it2CAKjVq4rUELf8iXvmGoFl\n"
	"000spua4MjHNUYm7LR0QaKesKrMyGZUesQJAL8aLdYPJI+SD9Tr/jqLtIkZ4frQe\n"
	"eshw4pvsoyheiHF3zyshO791crAr4EVCx3sMlxB1xnmqLXPCPyCEHxO//QJBAIBY\n"
	"IYkjDZJ6ofGIe1UyXJNvfdkPu9J+ut4wU5jjEcgs6mK62J6RGuFxhy2iOQfFMdjo\n"
	"yL+OCUg7mDCun7uCxrECQAtSvnLOFMjO5qExRjFtwi+b1rcSekd3Osk/izyRFSzg\n"
	"Or+AL56/EKfiogNnFipgaXIbb/xj785Cob6v96XoW1I=\n"
	"-----END RSA PRIVATE KEY-----\n";

QCA::Cert *cert;
QCA::RSAKey *privkey;

using namespace XMPP;

int id_num = 0;

class Session : public QObject
{
	Q_OBJECT
public:
	Session(const QString &host, const QString &defRealm, ByteStream *bs) : QObject(0)
	{
		id = id_num++;

		tls = new QCA::TLS;
		tls->setCertificate(*cert, *privkey);

		cs = new ClientStream(host, defRealm, bs, tls);
		connect(cs, SIGNAL(connectionClosed()), SLOT(cs_connectionClosed()));
		connect(cs, SIGNAL(error(int)), SLOT(cs_error(int)));
	}

	~Session()
	{
		delete cs;
		delete tls;
		printf("[%d]: deleted\n", id);
	}

	void start()
	{
		printf("[%d]: New session!\n", id);
		cs->accept();
	}

signals:
	void done();

private slots:
	void cs_connectionClosed()
	{
		printf("[%d]: Connection closed by peer\n", id);
		done();
	}

	void cs_error(int)
	{
		printf("[%d]: Error\n", id);
		done();
	}

private:
	int id;
	ClientStream *cs;
	QCA::TLS *tls;
};

class ServerTest : public QServerSocket
{
	Q_OBJECT
public:
	enum { Idle, Handshaking, Active, Closing };

	ServerTest(const QString &_host, int _port) : QServerSocket(_port), host(_host), port(_port)
	{
		cert = new QCA::Cert;
		privkey = new QCA::RSAKey;

		cert->fromPEM(pemdata_cert);
		privkey->fromPEM(pemdata_privkey);

		list.setAutoDelete(true);
	}

	~ServerTest()
	{
	}

	void start()
	{
		char buf[256];
		int r = gethostname(buf, sizeof(buf)-1);
		if(r == -1) {
			printf("Error getting hostname!\n");
			QTimer::singleShot(0, this, SIGNAL(quit()));
			return;
		}
		QString myhostname = buf;

		realm = myhostname;
		if(host.isEmpty())
			host = myhostname;

		if(cert->isNull() || privkey->isNull()) {
			printf("Error loading cert and/or private key!\n");
			QTimer::singleShot(0, this, SIGNAL(quit()));
			return;
		}
		if(!ok()) {
			printf("Error binding to port %d!\n", port);
			QTimer::singleShot(0, this, SIGNAL(quit()));
			return;
		}
		printf("Listening on %s:%d ...\n", host.latin1(), port);
	}

	void newConnection(int s)
	{
		BSocket *bs = new BSocket;
		bs->setSocket(s);
		Session *sess = new Session(host, realm, bs);
		list.append(sess);
		connect(sess, SIGNAL(done()), SLOT(sess_done()));
		sess->start();
	}

signals:
	void quit();

private slots:
	void sess_done()
	{
		Session *sess = (Session *)sender();
		list.removeRef(sess);
	}

private:
	QString host, realm;
	int port;
	QPtrList<Session> list;
};

#include "server.moc"

int main(int argc, char **argv)
{
	QApplication app(argc, argv, false);
	QString host = argc > 1 ? QString(argv[1]) : QString();
	int port = argc > 2 ? QString(argv[2]).toInt() : 5222;

	if(!QCA::isSupported(QCA::CAP_TLS)) {
		printf("TLS not supported!\n");
		return 1;
	}

	if(!QCA::isSupported(QCA::CAP_SASL)) {
		printf("SASL not supported!\n");
		return 1;
	}

	srand(time(NULL));

	ServerTest *s = new ServerTest(host, port);
	QObject::connect(s, SIGNAL(quit()), &app, SLOT(quit()));
	s->start();
	app.exec();
	delete s;

	// clean up
	QCA::unloadAllPlugins();

	return 0;
}
