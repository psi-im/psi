#include <qapplication.h>
#include "bconsole.h"
#include <QtCrypto>
#include "xmpp.h"

#include <stdio.h>

#define ROOTCERT_PATH "/usr/local/share/psi/certs/rootcert.xml"

QCA::Cert readCertXml(const QDomElement &e)
{
	QCA::Cert cert;
	// there should be one child data tag
	QDomElement data = e.elementsByTagName("data").item(0).toElement();
	if(!data.isNull())
		cert.fromDER(Base64::stringToArray(data.text()));
	return cert;
}

QPtrList<QCA::Cert> getRootCerts(const QString &store)
{
	QPtrList<QCA::Cert> list;

	// open the Psi rootcerts file
	QFile f(store);
	if(!f.open(IO_ReadOnly)) {
		printf("unable to open %s\n", f.name().latin1());
		return list;
	}
	QDomDocument doc;
	doc.setContent(&f);
	f.close();

	QDomElement base = doc.documentElement();
	if(base.tagName() != "store") {
		printf("wrong format of %s\n", f.name().latin1());
		return list;
	}
	QDomNodeList cl = base.elementsByTagName("certificate");
	if(cl.count() == 0) {
		printf("no certs found in %s\n", f.name().latin1());
		return list;
	}

	int num = 0;
	for(int n = 0; n < (int)cl.count(); ++n) {
		QCA::Cert *cert = new QCA::Cert(readCertXml(cl.item(n).toElement()));
		if(cert->isNull()) {
			printf("error reading cert\n");
			delete cert;
			continue;
		}

		++num;
		list.append(cert);
	}
	printf("imported %d root certs\n", num);

	return list;
}

static QString prompt(const QString &s)
{
	printf("* %s ", s.latin1());
	fflush(stdout);
	char line[256];
	fgets(line, 255, stdin);
	QString result = line;
	if(result[result.length()-1] == '\n')
		result.truncate(result.length()-1);
	return result;
}

static void showCertInfo(const QCA::Cert &cert)
{
	fprintf(stderr, "-- Cert --\n");
	fprintf(stderr, " CN: %s\n", cert.subject()["CN"].latin1());
	fprintf(stderr, " Valid from: %s, until %s\n",
		cert.notBefore().toString().latin1(),
		cert.notAfter().toString().latin1());
	fprintf(stderr, " PEM:\n%s\n", cert.toPEM().latin1());
}

static QString resultToString(int result)
{
	QString s;
	switch(result) {
		case QCA::TLS::NoCert:
			s = QObject::tr("No certificate presented.");
			break;
		case QCA::TLS::Valid:
			break;
		case QCA::TLS::HostMismatch:
			s = QObject::tr("Hostname mismatch.");
			break;
		case QCA::TLS::Rejected:
			s = QObject::tr("Root CA rejects the specified purpose.");
			break;
		case QCA::TLS::Untrusted:
			s = QObject::tr("Not trusted for the specified purpose.");
			break;
		case QCA::TLS::SignatureFailed:
			s = QObject::tr("Invalid signature.");
			break;
		case QCA::TLS::InvalidCA:
			s = QObject::tr("Invalid CA certificate.");
			break;
		case QCA::TLS::InvalidPurpose:
			s = QObject::tr("Invalid certificate purpose.");
			break;
		case QCA::TLS::SelfSigned:
			s = QObject::tr("Certificate is self-signed.");
			break;
		case QCA::TLS::Revoked:
			s = QObject::tr("Certificate has been revoked.");
			break;
		case QCA::TLS::PathLengthExceeded:
			s = QObject::tr("Maximum cert chain length exceeded.");
			break;
		case QCA::TLS::Expired:
			s = QObject::tr("Certificate has expired.");
			break;
		case QCA::TLS::Unknown:
		default:
			s = QObject::tr("General validation error.");
			break;
	}
	return s;
}

class App : public QObject
{
	Q_OBJECT
public:
	XMPP::AdvancedConnector *conn;
	QCA::TLS *tls;
	XMPP::QCATLSHandler *tlsHandler;
	XMPP::ClientStream *stream;
	BConsole *c;
	XMPP::Jid jid;
	QPtrList<QCA::Cert> rootCerts;

	App(const XMPP::Jid &_jid, const XMPP::AdvancedConnector::Proxy &proxy, const QString &host, int port, bool opt_ssl, bool opt_probe)
	:QObject(0)
	{
		c = 0;
		jid = _jid;

		// Connector
		conn = new XMPP::AdvancedConnector;
		conn->setProxy(proxy);
		if(!host.isEmpty())
			conn->setOptHostPort(host, port);
		conn->setOptProbe(opt_probe);
		conn->setOptSSL(opt_ssl);

		// TLSHandler
		tls = 0;
		tlsHandler = 0;
		rootCerts.setAutoDelete(true);
		if(QCA::isSupported(QCA::CAP_TLS)) {
			rootCerts = getRootCerts(ROOTCERT_PATH);
			tls = new QCA::TLS;
			tls->setCertificateStore(rootCerts);
			tlsHandler = new XMPP::QCATLSHandler(tls);
			connect(tlsHandler, SIGNAL(tlsHandshaken()), SLOT(tls_handshaken()));
		}

		// Stream
		stream = new XMPP::ClientStream(conn, tlsHandler);
		connect(stream, SIGNAL(connected()), SLOT(cs_connected()));
		connect(stream, SIGNAL(securityLayerActivated(int)), SLOT(cs_securityLayerActivated()));
		connect(stream, SIGNAL(needAuthParams(bool, bool, bool)), SLOT(cs_needAuthParams(bool, bool, bool)));
		connect(stream, SIGNAL(authenticated()), SLOT(cs_authenticated()));
		connect(stream, SIGNAL(connectionClosed()), SLOT(cs_connectionClosed()));
		connect(stream, SIGNAL(readyRead()), SLOT(cs_readyRead()));
		connect(stream, SIGNAL(stanzaWritten()), SLOT(cs_stanzaWritten()));
		connect(stream, SIGNAL(warning(int)), SLOT(cs_warning(int)));
		connect(stream, SIGNAL(error(int)), SLOT(cs_error(int)));

		fprintf(stderr, "conntest: Connecting ...\n");
		stream->setSSFRange(0, 256);
		stream->connectToServer(jid);
	}

	~App()
	{
		delete stream;
		delete tls; // this destroys the TLSHandler also
		delete conn;
		delete c;
	}

signals:
	void quit();

private slots:
	void tls_handshaken()
	{
		QCA::Cert cert = tls->peerCertificate();
		int vr = tls->certificateValidityResult();

		fprintf(stderr, "conntest: Successful TLS handshake.\n");
		if(!cert.isNull())
			showCertInfo(cert);
		if(vr == QCA::TLS::Valid)
			fprintf(stderr, "conntest: Valid certificate.\n");
		else
			fprintf(stderr, "conntest: Invalid certificate: %s\n", resultToString(vr).latin1());

		tlsHandler->continueAfterHandshake();
	}

	void cs_connected()
	{
		fprintf(stderr, "conntest: Connected\n");
	}

	void cs_securityLayerActivated()
	{
		fprintf(stderr, "conntest: Security layer activated (%s)\n", tls->isHandshaken() ? "TLS": "SASL");
	}

	void cs_needAuthParams(bool user, bool pass, bool realm)
	{
		fprintf(stderr, "conntest: need auth params -");
		if(user)
			fprintf(stderr, " (user)");
		if(pass)
			fprintf(stderr, " (pass)");
		if(realm)
			fprintf(stderr, " (realm)");
		fprintf(stderr, "\n");

		if(user)
			stream->setUsername(jid.node());
		if(pass)
			stream->setPassword(prompt("Password (not hidden!) :"));
		stream->continueAfterParams();
	}

	void cs_authenticated()
	{
		fprintf(stderr, "conntest: <<< Authenticated >>>\n");

		// console
		c = new BConsole;
		connect(c, SIGNAL(connectionClosed()), SLOT(con_connectionClosed()));
		connect(c, SIGNAL(readyRead()), SLOT(con_readyRead()));
	}

	void cs_connectionClosed()
	{
		fprintf(stderr, "conntest: Disconnected by peer\n");
		quit();
	}

	void cs_readyRead()
	{
		for(XMPP::Stanza s; !(s = stream->read()).isNull();) {
			QString str = s.toString();
			printf("%s\n", str.local8Bit().data());
		}
	}

	void cs_stanzaWritten()
	{
		fprintf(stderr, "conntest: Stanza written\n");
	}

	void cs_warning(int warn)
	{
		if(warn == XMPP::ClientStream::WarnOldVersion) {
			fprintf(stderr, "conntest: Warning: pre-1.0 protocol server\n");
		}
		else if(warn == XMPP::ClientStream::WarnNoTLS) {
			fprintf(stderr, "conntest: Warning: TLS not available!\n");
		}
		stream->continueAfterWarning();
	}

	void cs_error(int err)
	{
		if(err == XMPP::ClientStream::ErrParse) {
			fprintf(stderr, "conntest: XML parsing error\n");
		}
		else if(err == XMPP::ClientStream::ErrProtocol) {
			fprintf(stderr, "conntest: XMPP protocol error\n");
		}
		else if(err == XMPP::ClientStream::ErrStream) {
			int x = stream->errorCondition();
			QString s;
			if(x == XMPP::Stream::GenericStreamError)
				s = "generic stream error";
			else if(x == XMPP::ClientStream::Conflict)
				s = "conflict (remote login replacing this one)";
			else if(x == XMPP::ClientStream::ConnectionTimeout)
				s = "timed out from inactivity";
			else if(x == XMPP::ClientStream::InternalServerError)
				s = "internal server error";
			else if(x == XMPP::ClientStream::InvalidXml)
				s = "invalid XML";
			else if(x == XMPP::ClientStream::PolicyViolation)
				s = "policy violation.  go to jail!";
			else if(x == XMPP::ClientStream::ResourceConstraint)
				s = "server out of resources";
			else if(x == XMPP::ClientStream::SystemShutdown)
				s = "system is shutting down NOW";
			fprintf(stderr, "conntest: XMPP stream error: %s\n", s.latin1());
		}
		else if(err == XMPP::ClientStream::ErrConnection) {
			int x = conn->errorCode();
			QString s;
			if(x == XMPP::AdvancedConnector::ErrConnectionRefused)
				s = "unable to connect to server";
			else if(x == XMPP::AdvancedConnector::ErrHostNotFound)
				s = "host not found";
			else if(x == XMPP::AdvancedConnector::ErrProxyConnect)
				s = "proxy connect";
			else if(x == XMPP::AdvancedConnector::ErrProxyNeg)
				s = "proxy negotiating";
			else if(x == XMPP::AdvancedConnector::ErrProxyAuth)
				s = "proxy authorization";
			else if(x == XMPP::AdvancedConnector::ErrStream)
				s = "stream error";
			fprintf(stderr, "conntest: connection error: %s\n", s.latin1());
		}
		else if(err == XMPP::ClientStream::ErrNeg) {
			int x = stream->errorCondition();
			QString s;
			if(x == XMPP::ClientStream::HostGone)
				s = "host no longer hosted";
			else if(x == XMPP::ClientStream::HostUnknown)
				s = "host unknown";
			else if(x == XMPP::ClientStream::RemoteConnectionFailed)
				s = "a required remote connection failed";
			else if(x == XMPP::ClientStream::SeeOtherHost)
				s = QString("see other host: [%1]").arg(stream->errorText());
			else if(x == XMPP::ClientStream::UnsupportedVersion)
				s = "server does not support proper xmpp version";
			fprintf(stderr, "conntest: stream negotiation error: %s\n", s.latin1());
		}
		else if(err == XMPP::ClientStream::ErrTLS) {
			int x = stream->errorCondition();
			QString s;
			if(x == XMPP::ClientStream::TLSStart)
				s = "server rejected STARTTLS";
			else if(x == XMPP::ClientStream::TLSFail) {
				int t = tlsHandler->tlsError();
				if(t == QCA::TLS::ErrHandshake)
					s = "TLS handshake error";
				else
					s = "broken security layer (TLS)";
			}
			fprintf(stderr, "conntest: %s\n", s.latin1());
		}
		else if(err == XMPP::ClientStream::ErrAuth) {
			int x = stream->errorCondition();
			QString s;
			if(x == XMPP::ClientStream::GenericAuthError)
				s = "unable to login";
			else if(x == XMPP::ClientStream::NoMech)
				s = "no appropriate auth mechanism available for given security settings";
			else if(x == XMPP::ClientStream::BadProto)
				s = "bad server response";
			else if(x == XMPP::ClientStream::BadServ)
				s = "server failed mutual authentication";
			else if(x == XMPP::ClientStream::EncryptionRequired)
				s = "encryption required for chosen SASL mechanism";
			else if(x == XMPP::ClientStream::InvalidAuthzid)
				s = "invalid authzid";
			else if(x == XMPP::ClientStream::InvalidMech)
				s = "invalid SASL mechanism";
			else if(x == XMPP::ClientStream::InvalidRealm)
				s = "invalid realm";
			else if(x == XMPP::ClientStream::MechTooWeak)
				s = "SASL mechanism too weak for authzid";
			else if(x == XMPP::ClientStream::NotAuthorized)
				s = "not authorized";
			else if(x == XMPP::ClientStream::TemporaryAuthFailure)
				s = "temporary auth failure";
			fprintf(stderr, "conntest: auth error: %s\n", s.latin1());
		}
		else if(err == XMPP::ClientStream::ErrSecurityLayer)
			fprintf(stderr, "conntest: broken security layer (SASL)\n");
		quit();
	}

	void con_connectionClosed()
	{
		fprintf(stderr, "conntest: Closing.\n");
		stream->close();
		quit();
	}

	void con_readyRead()
	{
		QByteArray a = c->read();
		QCString cs;
		cs.resize(a.size()+1);
		memcpy(cs.data(), a.data(), a.size());
		QString s = QString::fromLocal8Bit(cs);
		stream->writeDirect(s);
	}
};

#include "conntest.moc"

int main(int argc, char **argv)
{
	QApplication app(argc, argv, false);

	if(argc < 2) {
		printf("usage: conntest [options] [jid]\n");
		printf("   Options:\n");
		printf("     --host=host:port\n");
		printf("     --sslhost=host:port\n");
		printf("     --probe\n");
		printf("     --proxy=[https|poll|socks],host:port,url\n");
		printf("     --proxy-auth=user,pass\n");
		printf("\n");
		return 0;
	}

	bool have_tls = QCA::isSupported(QCA::CAP_TLS);

	XMPP::Jid jid;
	XMPP::AdvancedConnector::Proxy proxy;
	QString host;
	int port = 0;
	bool opt_ssl = false;
	bool opt_probe = false;

	for(int at = 1; at < argc; ++at) {
		QString s = argv[at];

		// is it an option?
		if(s.left(2) == "--") {
			QString name;
			QStringList args;
			int n = s.find('=', 2);
			if(n != -1) {
				name = s.mid(2, n-2);
				++n;
				args = QStringList::split(',', s.mid(n), true);
			}
			else {
				name = s.mid(2);
				args.clear();
			}

			// eat the arg
			--argc;
			for(int x = at; x < argc; ++x)
				argv[x] = argv[x+1];
			--at; // don't advance

			// process option
			if(name == "proxy") {
				QString proxy_host;
				int proxy_port = 0;
				QString type = args[0];
				QString s = args[1];
				int n = s.find(':');
				if(n == -1) {
					if(type != "poll") {
						printf("Invalid host:port for proxy\n");
						return 0;
					}
				}
				else {
					proxy_host = s.mid(0, n);
					++n;
					proxy_port = s.mid(n).toInt();
				}

				if(type == "https") {
					proxy.setHttpConnect(proxy_host, proxy_port);
				}
				else if(type == "poll") {
					if(args.count() < 3) {
						printf("poll needs more args\n");
						return 0;
					}
					QString proxy_url = args[2];
					proxy.setHttpPoll(proxy_host, proxy_port, proxy_url);
				}
				else if(type == "socks") {
					proxy.setSocks(proxy_host, proxy_port);
				}
				else {
					printf("No such proxy type '%s'\n", type.latin1());
					return 0;
				}
			}
			else if(name == "proxy-auth") {
				proxy.setUserPass(args[0], args[1]);
			}
			else if(name == "host") {
				QString s = args[0];
				int n = s.find(':');
				if(n == -1) {
					printf("Invalid host:port for host\n");
					return 0;
				}
				host = s.mid(0, n);
				++n;
				port = s.mid(n).toInt();
			}
			else if(name == "sslhost") {
				QString s = args[0];
				int n = s.find(':');
				if(n == -1) {
					printf("Invalid host:port for host\n");
					return 0;
				}
				host = s.mid(0, n);
				++n;
				port = s.mid(n).toInt();
				opt_ssl = true;
			}
			else if(name == "probe") {
				opt_probe = true;
			}
			else {
				printf("Unknown option '%s'\n", name.latin1());
				return 0;
			}
		}
	}

	if(argc < 2) {
		printf("No host specified!\n");
		return 0;
	}
	jid = argv[1];

	if((opt_ssl || opt_probe) && !have_tls) {
		printf("TLS not supported, so the sslhost and probe options are not allowed.\n");
		return 0;
	}

	printf("JID: %s\n", jid.full().latin1());
	if(proxy.type() != XMPP::AdvancedConnector::Proxy::None) {
		printf("Proxy: ");
		if(proxy.type() == XMPP::AdvancedConnector::Proxy::HttpConnect)
			printf("HttpConnect (%s:%d)", proxy.host().latin1(), proxy.port());
		else if(proxy.type() == XMPP::AdvancedConnector::Proxy::HttpPoll) {
			printf("HttpPoll {%s}", proxy.url().latin1());
			if(!proxy.host().isEmpty()) {
				printf(" (%s:%d)", proxy.host().latin1(), proxy.port());
			}
		}
		else if(proxy.type() == XMPP::AdvancedConnector::Proxy::Socks)
			printf("Socks (%s:%d)", proxy.host().latin1(), proxy.port());
		printf("\n");
	}
	if(proxy.type() != XMPP::AdvancedConnector::Proxy::HttpPoll) {
		if(!host.isEmpty()) {
			printf("Host: %s:%d", host.latin1(), port);
			if(opt_ssl)
				printf(" (ssl)");
			printf("\n");
		}
		else {
			if(opt_probe)
				printf("Probe active\n");
		}
	}
	printf("----------\n");

	App *a = new App(jid, proxy, host, port, opt_ssl, opt_probe);
	QObject::connect(a, SIGNAL(quit()), &app, SLOT(quit()));
	app.exec();
	delete a;

	return 0;
}
