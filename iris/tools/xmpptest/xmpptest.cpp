#include <qapplication.h>
#include <qtextedit.h>
#include <qgroupbox.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qmessagebox.h>
#include <qinputdialog.h>
#include <qspinbox.h>
#include <qtimer.h>
#include <qmenubar.h>
#include <qmenu.h>
#include <qtabwidget.h>
#include <QTextBlock>
#include <qca.h>
#include "xmpp.h"
#include "im.h"

#include <stdlib.h>
#include <time.h>

#include "ui_test.h"

#include <stdio.h>

#define AppName "xmpptest"

static QString plain2rich(const QString &plain)
{
	QString rich;
	int col = 0;

	for(int i = 0; i < (int)plain.length(); ++i) {
		if(plain[i] == '\n') {
			rich += "<br>";
			col = 0;
		}
		else if(plain[i] == '\t') {
			rich += QChar::Nbsp;
			while(col % 4) {
				rich += QChar::Nbsp;
				++col;
			}
		}
		else if(plain[i].isSpace()) {
			if(i > 0 && plain[i-1] == ' ')
				rich += QChar::Nbsp;
			else
				rich += ' ';
		}
		else if(plain[i] == '<')
			rich += "&lt;";
		else if(plain[i] == '>')
			rich += "&gt;";
		else if(plain[i] == '\"')
			rich += "&quot;";
		else if(plain[i] == '\'')
			rich += "&apos;";
		else if(plain[i] == '&')
			rich += "&amp;";
		else
			rich += plain[i];
		++col;
	}

	return rich;
}

/*static void showCertInfo(const QCA::Cert &cert)
{
	fprintf(stderr, "-- Cert --\n");
	fprintf(stderr, " CN: %s\n", cert.subject()["CN"].latin1());
	fprintf(stderr, " Valid from: %s, until %s\n",
		cert.notBefore().toString().latin1(),
		cert.notAfter().toString().latin1());
	fprintf(stderr, " PEM:\n%s\n", cert.toPEM().latin1());
}*/

static QString resultToString(int result)
{
	QString s;
	switch(result) {
		case QCA::TLS::NoCertificate:
			s = QObject::tr("No certificate presented.");
			break;
		case QCA::TLS::Valid:
			break;
		case QCA::TLS::HostMismatch:
			s = QObject::tr("Hostname mismatch.");
			break;
		case QCA::TLS::InvalidCertificate:
			s = QObject::tr("Invalid Certificate.");
			break;
			// TODO: Inspect why
//			case QCA::TLS::Untrusted:
//				s = QObject::tr("Not trusted for the specified purpose.");
//				break;
//			case QCA::TLS::SignatureFailed:
//				s = QObject::tr("Invalid signature.");
//				break;
//			case QCA::TLS::InvalidCA:
//				s = QObject::tr("Invalid CA certificate.");
//				break;
//			case QCA::TLS::InvalidPurpose:
//				s = QObject::tr("Invalid certificate purpose.");
//				break;
//			case QCA::TLS::SelfSigned:
//				s = QObject::tr("Certificate is self-signed.");
//				break;
//			case QCA::TLS::Revoked:
//				s = QObject::tr("Certificate has been revoked.");
//				break;
//			case QCA::TLS::PathLengthExceeded:
//				s = QObject::tr("Maximum cert chain length exceeded.");
//				break;
//			case QCA::TLS::Expired:
//				s = QObject::tr("Certificate has expired.");
//				break;
//			case QCA::TLS::Unknown:
		default:
			s = QObject::tr("General validation error.");
			break;
	}
	return s;
}

static QTextBlock get_block(QTextEdit *te, int index)
{
	QTextBlock block = te->document()->begin();
	for(int n = 0; n < index && block.isValid(); ++n)
		block = block.next();
	return block;
}

static int textedit_paragraphLength(QTextEdit *te, int paragraph)
{
	QTextBlock block = get_block(te, paragraph);
	if(block.isValid())
		return block.length();
	else
		return 0;
}

static void textedit_setCursorPosition(QTextEdit *te, int line, int col)
{
	QTextCursor cur = te->textCursor();
	QTextBlock block = get_block(te, line);
	if(!block.isValid())
		return;
	if(col > block.length())
		col = block.length();
	cur.setPosition(block.position() + col);
	te->setTextCursor(cur);
}

static void textedit_setSelection(QTextEdit *te, int startLine, int startCol, int endLine, int endCol)
{
	QTextCursor cur = te->textCursor();

	QTextBlock block = get_block(te, startLine);
	if(!block.isValid())
		return;
	if(startCol > block.length())
		startCol = block.length();
	cur.setPosition(block.position() + startCol); // start

	block = get_block(te, endLine);
	if(!block.isValid())
		return;
	if(endCol > block.length())
		endCol = block.length();
	cur.setPosition(block.position() + endCol, QTextCursor::KeepAnchor); // select

	te->setTextCursor(cur);
}

class TestDebug : public XMPP::Debug
{
public:
	void msg(const QString &);
	void outgoingTag(const QString &);
	void incomingTag(const QString &);
	void outgoingXml(const QDomElement &);
	void incomingXml(const QDomElement &);
};

class TestDlg : public QDialog, public Ui::TestUI
{
	Q_OBJECT
public:
	bool active, connected;
	XMPP::AdvancedConnector *conn;
	QCA::TLS *tls;
	XMPP::QCATLSHandler *tlsHandler;
	XMPP::ClientStream *stream;
	XMPP::Jid jid;

	TestDlg(QWidget *parent=0) : QDialog(parent)
	{
		setupUi(this);
		setWindowTitle(tr("XMPP Test"));

		connect(ck_probe, SIGNAL(toggled(bool)), SLOT(probe_toggled(bool)));
		connect(cb_proxy, SIGNAL(activated(int)), SLOT(proxy_activated(int)));
		connect(pb_go, SIGNAL(clicked()), SLOT(go()));
		connect(pb_send, SIGNAL(clicked()), SLOT(send()));
		connect(pb_im, SIGNAL(clicked()), SLOT(sc_im()));
		connect(pb_msg, SIGNAL(clicked()), SLOT(sc_msg()));
		connect(pb_iqv, SIGNAL(clicked()), SLOT(sc_iqv()));
		connect(pb_about, SIGNAL(clicked()), SLOT(about()));

		sb_ssfmin->setMinimum(0);
		sb_ssfmin->setMaximum(256);
		sb_ssfmax->setMinimum(0);
		sb_ssfmax->setMaximum(256);

		pb_send->setEnabled(false);
		proxy_activated(0);
		ck_probe->setChecked(true);
		ck_mutual->setChecked(false);
		pb_go->setText(tr("&Connect"));

		//le_jid->setText("psitest@jabberd.jabberstudio.org/Test");
		//ck_probe->setChecked(false);
		//le_host->setText("jabberd.jabberstudio.org:15222");
		//ck_mutual->setChecked(false);
		//le_jid->setText("sasltest@e.jabber.ru/Test");
		//le_jid->setText("psitest@jabber.cz/Test");
		//le_pass->setText("psitest");
		//cb_proxy->setCurrentItem(3);
		//le_proxyurl->setText("http://connect.jabber.cz/");

		// setup xmpp
		conn = new XMPP::AdvancedConnector;
		connect(conn, SIGNAL(srvLookup(const QString &)), SLOT(conn_srvLookup(const QString &)));
		connect(conn, SIGNAL(srvResult(bool)), SLOT(conn_srvResult(bool)));
		connect(conn, SIGNAL(httpSyncStarted()), SLOT(conn_httpSyncStarted()));
		connect(conn, SIGNAL(httpSyncFinished()), SLOT(conn_httpSyncFinished()));

		if(QCA::isSupported("tls")) {
			tls = new QCA::TLS;
			tlsHandler = new XMPP::QCATLSHandler(tls);
			tlsHandler->setXMPPCertCheck(true);
			connect(tlsHandler, SIGNAL(tlsHandshaken()), SLOT(tls_handshaken()));
		}
		else {
			tls = 0;
			tlsHandler = 0;
		}

		stream = new XMPP::ClientStream(conn, tlsHandler);
		//stream->setOldOnly(true);
		connect(stream, SIGNAL(connected()), SLOT(cs_connected()));
		connect(stream, SIGNAL(securityLayerActivated(int)), SLOT(cs_securityLayerActivated(int)));
		connect(stream, SIGNAL(needAuthParams(bool, bool, bool)), SLOT(cs_needAuthParams(bool, bool, bool)));
		connect(stream, SIGNAL(authenticated()), SLOT(cs_authenticated()));
		connect(stream, SIGNAL(connectionClosed()), SLOT(cs_connectionClosed()));
		connect(stream, SIGNAL(delayedCloseFinished()), SLOT(cs_delayedCloseFinished()));
		connect(stream, SIGNAL(readyRead()), SLOT(cs_readyRead()));
		connect(stream, SIGNAL(stanzaWritten()), SLOT(cs_stanzaWritten()));
		connect(stream, SIGNAL(warning(int)), SLOT(cs_warning(int)));
		connect(stream, SIGNAL(error(int)), SLOT(cs_error(int)));

		QTimer::singleShot(0, this, SLOT(adjustLayout()));

		le_jid->setFocus();
		active = false;
		connected = false;
	}

	~TestDlg()
	{
		delete stream;
		delete tls; // this destroys the TLSHandler also
		delete conn;
	}

private slots:
	void adjustLayout()
	{
		tb_main->setFixedWidth(tb_main->minimumSizeHint().width());
		resize(minimumSizeHint());
		show();
	}

	void about()
	{
		QMessageBox::about(this, tr("About %1").arg(AppName), tr(
			"%1 v1.0\n"
			"\n"
			"Utility to demonstrate the Iris XMPP library.\n"
			"\n"
			"Currently supports:\n"
			"  draft-ietf-xmpp-core-21\n"
			"  JEP-0025\n"
			"\n"
			"Copyright (C) 2003 Justin Karneges").arg(AppName));
	}

	void probe_toggled(bool)
	{
		setHostState();
	}

	void proxy_activated(int x)
	{
		bool ok = (x != 0);
		bool okpoll = (x == 3);
		gb_proxy->setEnabled(ok);
		lb_proxyurl->setEnabled(okpoll);
		le_proxyurl->setEnabled(okpoll);
		ck_probe->setEnabled(!okpoll);
		setHostState();
	}

	void cleanup()
	{
		pb_send->setEnabled(false);
		pb_go->setEnabled(true);
		pb_go->setText(tr("&Connect"));
		pb_go->setFocus();
		gb_server->setEnabled(true);
		active = false;
		connected = false;
	}

	void start()
	{
		if(active)
			return;

		jid = XMPP::Jid(le_jid->text());
		if(jid.domain().isEmpty() || jid.node().isEmpty() || jid.resource().isEmpty()) {
			QMessageBox::information(this, tr("Error"), tr("Please enter the Full JID to connect with."));
			return;
		}

		int p = cb_proxy->currentIndex();
		XMPP::AdvancedConnector::Proxy proxy;
		if(p > 0) {
			QString s = le_proxyhost->text();
			QString url = le_proxyurl->text();
			if(p != 3 && s.isEmpty()) {
				QMessageBox::information(this, tr("Error"), tr("You must specify a host:port for the proxy."));
				return;
			}
			if(p == 3 && s.isEmpty() && url.isEmpty()) {
				QMessageBox::information(this, tr("Error"), tr("You must at least enter a URL to use http poll."));
				return;
			}
			QString host;
			int port = 0;
			if(!s.isEmpty()) {
				int n = s.indexOf(':');
				if(n == -1) {
					QMessageBox::information(this, tr("Error"), tr("Please enter the proxy host in the form 'host:port'."));
					return;
				}
				host = s.mid(0, n);
				port = s.mid(n+1).toInt();
			}
			if(p == 1)
				proxy.setHttpConnect(host, port);
			else if(p == 2)
				proxy.setSocks(host, port);
			else if(p == 3) {
				proxy.setHttpPoll(host, port, url);
				proxy.setPollInterval(2); // fast during login
			}
			proxy.setUserPass(le_proxyuser->text(), le_proxypass->text());
		}
		bool probe = (p != 3 && ck_probe->isChecked());
		bool useHost = (!probe && !le_host->text().isEmpty());
		QString host;
		int port = 0;
		bool ssl = false;
		if(useHost) {
			QString s = le_host->text();
			int n = s.indexOf(':');
			if(n == -1) {
				QMessageBox::information(this, tr("Error"), tr("Please enter the host in the form 'host:port'."));
				return;
			}
			host = s.mid(0, n);
			port = s.mid(n+1).toInt();

			if(ck_ssl->isChecked())
				ssl = true;
		}
		if(sb_ssfmin->value() > sb_ssfmax->value()) {
			QMessageBox::information(this, tr("Error"), tr("Error: SSF Min is greater than SSF Max."));
			return;
		}

		if((probe || ssl) && !tls) {
			QMessageBox::information(this, tr("Error"), tr("Error: TLS not available.  Disable any TLS options."));
			return;
		}

		// prepare
		conn->setProxy(proxy);
		if(useHost)
			conn->setOptHostPort(host, port);
		else
			conn->setOptHostPort("", 0);
		conn->setOptProbe(probe);
		conn->setOptSSL(ssl);

		if(tls) {
			tls->setTrustedCertificates(QCA::systemStore());
		}

		stream->setNoopTime(55000); // every 55 seconds
		stream->setAllowPlain(ck_plain->isChecked() ? XMPP::ClientStream::AllowPlain : XMPP::ClientStream::NoAllowPlain);
		stream->setRequireMutualAuth(ck_mutual->isChecked());
		stream->setSSFRange(sb_ssfmin->value(), sb_ssfmax->value());
		//stream->setOldOnly(true);
		stream->setCompress(true);
		
		gb_server->setEnabled(false);
		pb_go->setText(tr("&Disconnect"));
		pb_go->setFocus();
		active = true;

		appendSysMsg("Connecting...");
		stream->connectToServer(jid);
	}

	void stop()
	{
		if(!active)
			return;

		if(connected) {
			pb_go->setEnabled(false);
			appendSysMsg("Disconnecting...");
			stream->close();
		}
		else {
			stream->close();
			appendSysMsg("Disconnected");
			cleanup();
		}
	}

	void go()
	{
		if(active)
			stop();
		else
			start();
	}

	void send()
	{
		if(te_input->toPlainText().isEmpty())
			return;

		// construct a "temporary" document to parse the input
		QString str = "<stream xmlns=\"jabber:client\">\n";
		str += te_input->toPlainText() + '\n';
		str += "</stream>";

		QDomDocument doc;
		QString errMsg;
		int errLine, errCol;
		if(!doc.setContent(str, true, &errMsg, &errLine, &errCol)) {
			int lines = str.split('\n', QString::KeepEmptyParts).count();
			--errLine; // skip the first line
			if(errLine == lines-1) {
				errLine = lines-2;
				errCol = textedit_paragraphLength(te_input, errLine-1)+1;
				errMsg = "incomplete input";
			}
			textedit_setCursorPosition(te_input, errLine-1, errCol-1);
			QMessageBox::information(this, tr("Error"), tr("Bad XML input (%1,%2): %3\nPlease correct and try again.").arg(errCol).arg(errLine).arg(errMsg));
			return;
		}
		QDomElement e = doc.firstChild().toElement();

		int num = 0;
		QDomNodeList nl = e.childNodes();
		QList<XMPP::Stanza> stanzaList;
		for(int x = 0; x < nl.count(); ++x) {
			QDomNode n = nl.item(x);
			if(n.isElement()) {
				QDomElement e = n.toElement();
				XMPP::Stanza s = stream->createStanza(e);
				if(s.isNull()) {
					QMessageBox::information(this, tr("Error"), tr("Bad Stanza '%1'.  Must be 'message', 'presence', or 'iq'").arg(e.tagName()));
					return;
				}
				stanzaList += s;
				++num;
			}
		}
		if(num == 0) {
			QMessageBox::information(this, tr("Error"), tr("You must enter at least one stanza!"));
			return;
		}

		// out the door
		for(QList<XMPP::Stanza>::ConstIterator it = stanzaList.begin(); it != stanzaList.end(); ++it) {
			appendXmlOut(XMPP::Stream::xmlToString((*it).element(), true));
			stream->write(*it);
		}

		te_input->setText("");
	}

	void sc_im()
	{
		/*XMPP::Message m("justin@andbit.net/Psi");
		m.setSubject("Hi");
		m.setBody("I send you this in order to have your advice.");
		m.setBody("Escucha lechuga!", "es");
		XMPP::Stanza stanza = m.toStanza(stream);
		QString str = stanza.toString();
		printf("[%s]\n", str.latin1());

		XMPP::Message n;
		n.fromStanza(stanza);
		printf("subject: [%s]\n", n.subject().latin1());
		printf("body: [%s]\n", n.body().latin1());
		printf("body-es: [%s]\n", n.body("es").latin1());*/

		QString s;
		s += "<iq type='set' id='sess_1'>\n";
		s += " <session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>\n";
		s += "</iq>";
		te_input->setText(s);
		te_input->setFocus();
	}

	void sc_msg()
	{
		QString to = le_to->text();
		QString s;
		if(!to.isEmpty())
			s += QString("<message to=\"%1\">\n").arg(to);
		else
			s += QString("<message to=\"\">\n");
		s += " <body>hello world</body>\n";
		s += "</message>";
		te_input->setText(s);
		if(!to.isEmpty()) {
			textedit_setCursorPosition(te_input, 1, 7);
			textedit_setSelection(te_input, 1, 7, 1, 18);
		}
		else
			textedit_setCursorPosition(te_input, 0, 13);
		te_input->setFocus();
	}

	void sc_iqv()
	{
		QString to = le_to->text();
		QString s;
		if(!to.isEmpty())
			s += QString("<iq to=\"%1\" type=\"get\" id=\"abcde\">\n").arg(to);
		else
			s += QString("<iq to=\"\" type=\"get\" id=\"abcde\">\n");
		s += " <query xmlns=\"jabber:iq:version\"/>\n";
		s += "</iq>";
		te_input->setText(s);
		if(!to.isEmpty()) {
			textedit_setCursorPosition(te_input, 0, 8);
			textedit_setSelection(te_input, 0, 8, 0, 8 + to.length());
		}
		else
			textedit_setCursorPosition(te_input, 0, 8);
		te_input->setFocus();
	}

	void conn_srvLookup(const QString &server)
	{
		appendLibMsg(QString("SRV lookup on [%1]").arg(server));
	}

	void conn_srvResult(bool b)
	{
		if(b)
			appendLibMsg("SRV lookup success!");
		else
			appendLibMsg("SRV lookup failed");
	}

	void conn_httpSyncStarted()
	{
		appendLibMsg("HttpPoll: syncing");
	}

	void conn_httpSyncFinished()
	{
		appendLibMsg("HttpPoll: done");
	}

	void tls_handshaken()
	{
		//QCA::Certificate cert = tls->peerCertificate();
		int vr = tls->peerIdentityResult();
		if (vr == QCA::TLS::Valid && !tlsHandler->certMatchesHostname()) vr = QCA::TLS::HostMismatch;

		appendSysMsg("Successful TLS handshake.");
		if(vr == QCA::TLS::Valid)
			appendSysMsg("Valid certificate.");
		else {
			appendSysMsg(QString("Invalid certificate: %1").arg(resultToString(vr)), Qt::red);
			appendSysMsg("Continuing anyway");
		}

		tlsHandler->continueAfterHandshake();
	}

	void cs_connected()
	{
		QString s = "Connected";
		if(conn->havePeerAddress())
			s += QString(" (%1:%2)").arg(conn->peerAddress().toString()).arg(conn->peerPort());
		if(conn->useSSL())
			s += " [ssl]";
		appendSysMsg(s);
	}

	void cs_securityLayerActivated(int type)
	{
		appendSysMsg(QString("Security layer activated (%1)").arg((type == XMPP::ClientStream::LayerTLS) ? "TLS": "SASL"));
	}

	void cs_needAuthParams(bool user, bool pass, bool realm)
	{
		QString s = "Need auth parameters -";
		if(user)
			s += " (Username)";
		if(pass)
			s += " (Password)";
		if(realm)
			s += " (Realm)";
		appendSysMsg(s);

		if(user) {
			if(!le_user->text().isEmpty())
				stream->setUsername(le_user->text());
			else
				stream->setUsername(jid.node());
		}
		if(pass) {
			if(!le_pass->text().isEmpty())
				stream->setPassword(le_pass->text());
			else {
				conn->changePollInterval(10); // slow down during prompt
				bool ok;
				QString s = QInputDialog::getText(this, tr("Password"), tr("Enter the password for %1").arg(jid.full()), QLineEdit::Password, QString(), &ok);
				if(!ok) {
					stop();
					return;
				}
				stream->setPassword(s);

				conn->changePollInterval(2); // resume speed
			}
		}
		if(realm)
			stream->setRealm(jid.domain());

		stream->continueAfterParams();
	}

	void cs_authenticated()
	{
		connected = true;
		pb_send->setEnabled(true);
		conn->changePollInterval(10); // slow down after login
		appendSysMsg("Authenticated");
	}

	void cs_connectionClosed()
	{
		appendSysMsg("Disconnected by peer");
		cleanup();
	}

	void cs_delayedCloseFinished()
	{
		appendSysMsg("Disconnected");
		cleanup();
	}

	void cs_readyRead()
	{
		while(stream->stanzaAvailable()) {
			XMPP::Stanza s = stream->read();
			appendXmlIn(XMPP::Stream::xmlToString(s.element(), true));
		}
	}

	void cs_stanzaWritten()
	{
		appendSysMsg("Stanza sent");
	}

	void cs_warning(int warn)
	{
		if(warn == XMPP::ClientStream::WarnOldVersion) {
			appendSysMsg("Warning: pre-1.0 protocol server", Qt::red);
		}
		else if(warn == XMPP::ClientStream::WarnNoTLS) {
			appendSysMsg("Warning: TLS not available!", Qt::red);
		}
		stream->continueAfterWarning();
	}

	void cs_error(int err)
	{
		if(err == XMPP::ClientStream::ErrParse) {
			appendErrMsg("XML parsing error");
		}
		else if(err == XMPP::ClientStream::ErrProtocol) {
			appendErrMsg("XMPP protocol error");
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
			else if(x == XMPP::ClientStream::InvalidFrom)
				s = "invalid from address";
			else if(x == XMPP::ClientStream::InvalidXml)
				s = "invalid XML";
			else if(x == XMPP::ClientStream::PolicyViolation)
				s = "policy violation.  go to jail!";
			else if(x == XMPP::ClientStream::ResourceConstraint)
				s = "server out of resources";
			else if(x == XMPP::ClientStream::SystemShutdown)
				s = "system is shutting down NOW";
			appendErrMsg(QString("XMPP stream error: %1").arg(s));
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
			appendErrMsg(QString("Connection error: %1").arg(s));
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
			appendErrMsg(QString("Stream negotiation error: %1").arg(s));
		}
		else if(err == XMPP::ClientStream::ErrTLS) {
			int x = stream->errorCondition();
			QString s;
			if(x == XMPP::ClientStream::TLSStart)
				s = "server rejected STARTTLS";
			else if(x == XMPP::ClientStream::TLSFail) {
				int t = tlsHandler->tlsError();
				if(t == QCA::TLS::ErrorHandshake)
					s = "TLS handshake error";
				else
					s = "broken security layer (TLS)";
			}
			appendErrMsg(s);
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
			appendErrMsg(QString("Auth error: %1").arg(s));
		}
		else if(err == XMPP::ClientStream::ErrSecurityLayer)
			appendErrMsg("Broken security layer (SASL)");
		cleanup();
	}

private:
	void setHostState()
	{
		bool ok = false;
		if(!ck_probe->isChecked() && cb_proxy->currentIndex() != 3)
			ok = true;
		lb_host->setEnabled(ok);
		le_host->setEnabled(ok);
		ck_ssl->setEnabled(ok);
	}

	void appendSysMsg(const QString &s, const QColor &_c=QColor())
	{
		QString str;
		QColor c;
		if(_c.isValid())
			c = _c;
		else
			c = Qt::blue;

		if(c.isValid())
			str += QString("<font color=\"%1\">").arg(c.name());
		str += QString("*** %1").arg(s);
		if(c.isValid())
			str += QString("</font>");
		te_log->append(str);
	}

public:
	void appendLibMsg(const QString &s)
	{
		appendSysMsg(s, Qt::magenta);
	}

	void appendErrMsg(const QString &s)
	{
		appendSysMsg(s, Qt::red);
	}

	void appendXmlOut(const QString &s)
	{
		QStringList lines = s.split('\n', QString::KeepEmptyParts);
		QString str;
		bool first = true;
		for(QStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it) {
			if(!first)
				str += "<br>";
			str += QString("<font color=\"%1\">%2</font>").arg(QColor(Qt::darkGreen).name()).arg(plain2rich(*it));
			first = false;
		}
		te_log->append(str);
	}

	void appendXmlIn(const QString &s)
	{
		QStringList lines = s.split('\n', QString::KeepEmptyParts);
		QString str;
		bool first = true;
		for(QStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it) {
			if(!first)
				str += "<br>";
			str += QString("<font color=\"%1\">%2</font>").arg(QColor(Qt::darkBlue).name()).arg(plain2rich(*it));
			first = false;
		}
		te_log->append(str);
	}
};

TestDlg *td_glob = 0;

void TestDebug::msg(const QString &s)
{
	if(td_glob)
		td_glob->appendLibMsg(s);
}

void TestDebug::outgoingTag(const QString &s)
{
	if(td_glob)
		td_glob->appendXmlOut(s);
}

void TestDebug::incomingTag(const QString &s)
{
	if(td_glob)
		td_glob->appendXmlIn(s);
}

void TestDebug::outgoingXml(const QDomElement &e)
{
	QString out = XMPP::Stream::xmlToString(e, true);
	if(td_glob)
		td_glob->appendXmlOut(out);
}

void TestDebug::incomingXml(const QDomElement &e)
{
	QString out = XMPP::Stream::xmlToString(e, true);
	if(td_glob)
		td_glob->appendXmlIn(out);
}

#include "xmpptest.moc"

int main(int argc, char **argv)
{
	QCA::Initializer init;

#ifdef Q_OS_WIN32
	QApplication::addLibraryPath(".");
	putenv("SASL_PATH=.\\sasl");
#endif
	QApplication app(argc, argv);

	// seed the random number generator (needed at least for HttpPoll)
	srand(time(NULL));

	TestDlg *w = new TestDlg(0);
	td_glob = w;
	TestDebug *td = new TestDebug;
	XMPP::setDebug(td);
	QObject::connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
	app.exec();
	XMPP::setDebug(0);
	delete td;
	delete w;

	// we need this for a clean exit
	QCA::unloadAllPlugins();

	return 0;
}

#ifdef QCA_STATIC
#include <QtPlugin>
#ifdef HAVE_OPENSSL
Q_IMPORT_PLUGIN(qca_openssl)
#endif
#endif
