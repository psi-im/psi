/*
 * httppoll.cpp - HTTP polling proxy
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "httppoll.h"

#include <QUrl>
#include <qstringlist.h>
#include <qtimer.h>
#include <qpointer.h>
#include <QtCrypto>
#include <QByteArray>
#include <stdlib.h>
#include "bsocket.h"

#ifdef PROX_DEBUG
#include <stdio.h>
#endif

#define POLL_KEYS 64

// CS_NAMESPACE_BEGIN

static QByteArray randomArray(int size)
{
	QByteArray a;
  a.resize(size);
	for(int n = 0; n < size; ++n)
		a[n] = (char)(256.0*rand()/(RAND_MAX+1.0));
	return a;
}

//----------------------------------------------------------------------------
// HttpPoll
//----------------------------------------------------------------------------
static QString hpk(int n, const QString &s)
{
	if(n == 0)
		return s;
	else
		return QCA::Base64().arrayToString( QCA::Hash("sha1").hash( hpk(n - 1, s).toLatin1() ).toByteArray() );
}

class HttpPoll::Private
{
public:
	Private() {}

	HttpProxyPost http;
	QString host;
	int port;
	QString user, pass;
	QString url;
	bool use_proxy;

	QByteArray out;

	int state;
	bool closing;
	QString ident;

	QTimer *t;

	QString key[POLL_KEYS];
	int key_n;

	int polltime;
};

HttpPoll::HttpPoll(QObject *parent)
:ByteStream(parent)
{
	d = new Private;

	d->polltime = 30;
	d->t = new QTimer;
  d->t->setSingleShot(true);
	connect(d->t, SIGNAL(timeout()), SLOT(do_sync()));

	connect(&d->http, SIGNAL(result()), SLOT(http_result()));
	connect(&d->http, SIGNAL(error(int)), SLOT(http_error(int)));

	reset(true);
}

HttpPoll::~HttpPoll()
{
	reset(true);
	delete d->t;
	delete d;
}

void HttpPoll::reset(bool clear)
{
	if(d->http.isActive())
		d->http.stop();
	if(clear)
		clearReadBuffer();
	clearWriteBuffer();
	d->out.resize(0);
	d->state = 0;
	d->closing = false;
	d->t->stop();
}

void HttpPoll::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

void HttpPoll::connectToUrl(const QString &url)
{
	connectToHost("", 0, url);
}

void HttpPoll::connectToHost(const QString &proxyHost, int proxyPort, const QString &url)
{
	reset(true);

	// using proxy?
	if(!proxyHost.isEmpty()) {
		d->host = proxyHost;
		d->port = proxyPort;
		d->url = url;
		d->use_proxy = true;
	}
	else {
		QUrl u = url;
		d->host = u.host();
		if(u.port() != -1)
			d->port = u.port();
		else
			d->port = 80;
		d->url = u.path() + "?" + u.encodedQuery();
		d->use_proxy = false;
	}

	resetKey();
	bool last;
	QString key = getKey(&last);

#ifdef PROX_DEBUG
	fprintf(stderr, "HttpPoll: Connecting to %s:%d [%s]", d->host.latin1(), d->port, d->url.latin1());
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	QPointer<QObject> self = this;
	syncStarted();
	if(!self)
		return;

	d->state = 1;
	d->http.setAuth(d->user, d->pass);
	d->http.post(d->host, d->port, d->url, makePacket("0", key, "", QByteArray()), d->use_proxy);
}

QByteArray HttpPoll::makePacket(const QString &ident, const QString &key, const QString &newkey, const QByteArray &block)
{
	QString str = ident;
	if(!key.isEmpty()) {
		str += ';';
		str += key;
	}
	if(!newkey.isEmpty()) {
		str += ';';
		str += newkey;
	}
	str += ',';
	QByteArray cs = str.toLatin1();
	int len = cs.length();

	QByteArray a;
  a.resize(len + block.size());
	memcpy(a.data(), cs.data(), len);
	memcpy(a.data() + len, block.data(), block.size());
	return a;
}

int HttpPoll::pollInterval() const
{
	return d->polltime;
}

void HttpPoll::setPollInterval(int seconds)
{
	d->polltime = seconds;
}

bool HttpPoll::isOpen() const
{
	return (d->state == 2 ? true: false);
}

void HttpPoll::close()
{
	if(d->state == 0 || d->closing)
		return;

	if(bytesToWrite() == 0)
		reset();
	else
		d->closing = true;
}

void HttpPoll::http_result()
{
	// check for death :)
	QPointer<QObject> self = this;
	syncFinished();
	if(!self)
		return;

	// get id and packet
	QString id;
	QString cookie = d->http.getHeader("Set-Cookie");
	int n = cookie.indexOf("ID=");
	if(n == -1) {
		reset();
		error(ErrRead);
		return;
	}
	n += 3;
	int n2 = cookie.indexOf(';', n);
	if(n2 != -1)
		id = cookie.mid(n, n2-n);
	else
		id = cookie.mid(n);
	QByteArray block = d->http.body();

	// session error?
	if(id.right(2) == ":0") {
		if(id == "0:0" && d->state == 2) {
			reset();
			connectionClosed();
			return;
		}
		else {
			reset();
			error(ErrRead);
			return;
		}
	}

	d->ident = id;
	bool justNowConnected = false;
	if(d->state == 1) {
		d->state = 2;
		justNowConnected = true;
	}

	// sync up again soon
	if(bytesToWrite() > 0 || !d->closing) {
		d->t->start(d->polltime * 1000);
  }

	// connecting
	if(justNowConnected) {
		connected();
	}
	else {
		if(!d->out.isEmpty()) {
			int x = d->out.size();
			d->out.resize(0);
			takeWrite(x);
			bytesWritten(x);
		}
	}

	if(!self)
		return;

	if(!block.isEmpty()) {
		appendRead(block);
		readyRead();
	}

	if(!self)
		return;

	if(bytesToWrite() > 0) {
		do_sync();
	}
	else {
		if(d->closing) {
			reset();
			delayedCloseFinished();
			return;
		}
	}
}

void HttpPoll::http_error(int x)
{
	reset();
	if(x == HttpProxyPost::ErrConnectionRefused)
		error(ErrConnectionRefused);
	else if(x == HttpProxyPost::ErrHostNotFound)
		error(ErrHostNotFound);
	else if(x == HttpProxyPost::ErrSocket)
		error(ErrRead);
	else if(x == HttpProxyPost::ErrProxyConnect)
		error(ErrProxyConnect);
	else if(x == HttpProxyPost::ErrProxyNeg)
		error(ErrProxyNeg);
	else if(x == HttpProxyPost::ErrProxyAuth)
		error(ErrProxyAuth);
}

int HttpPoll::tryWrite()
{
	if(!d->http.isActive())
		do_sync();
	return 0;
}

void HttpPoll::do_sync()
{
	if(d->http.isActive())
		return;

	d->t->stop();
	d->out = takeWrite(0, false);

	bool last;
	QString key = getKey(&last);
	QString newkey;
	if(last) {
		resetKey();
		newkey = getKey(&last);
	}

	QPointer<QObject> self = this;
	syncStarted();
	if(!self)
		return;

	d->http.post(d->host, d->port, d->url, makePacket(d->ident, key, newkey, d->out), d->use_proxy);
}

void HttpPoll::resetKey()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpPoll: reset key!\n");
#endif
	QByteArray a = randomArray(64);
	QString str = QString::fromLatin1(a.data(), a.size());

	d->key_n = POLL_KEYS;
	for(int n = 0; n < POLL_KEYS; ++n)
		d->key[n] = hpk(n+1, str);
}

const QString & HttpPoll::getKey(bool *last)
{
	*last = false;
	--(d->key_n);
	if(d->key_n == 0)
		*last = true;
	return d->key[d->key_n];
}


//----------------------------------------------------------------------------
// HttpProxyPost
//----------------------------------------------------------------------------
static QString extractLine(QByteArray *buf, bool *found)
{
	// scan for newline
	int n;
	for(n = 0; n < (int)buf->size()-1; ++n) {
		if(buf->at(n) == '\r' && buf->at(n+1) == '\n') {
			QByteArray cstr;
			cstr.resize(n);
			memcpy(cstr.data(), buf->data(), n);
			n += 2; // hack off CR/LF

			memmove(buf->data(), buf->data() + n, buf->size() - n);
			buf->resize(buf->size() - n);
			QString s = QString::fromUtf8(cstr);

			if(found)
				*found = true;
			return s;
		}
	}

	if(found)
		*found = false;
	return "";
}

static bool extractMainHeader(const QString &line, QString *proto, int *code, QString *msg)
{
	int n = line.indexOf(' ');
	if(n == -1)
		return false;
	if(proto)
		*proto = line.mid(0, n);
	++n;
	int n2 = line.indexOf(' ', n);
	if(n2 == -1)
		return false;
	if(code)
		*code = line.mid(n, n2-n).toInt();
	n = n2+1;
	if(msg)
		*msg = line.mid(n);
	return true;
}

class HttpProxyPost::Private
{
public:
	Private() {}

	BSocket sock;
	QByteArray postdata, recvBuf, body;
	QString url;
	QString user, pass;
	bool inHeader;
	QStringList headerLines;
	bool asProxy;
	QString host;
};

HttpProxyPost::HttpProxyPost(QObject *parent)
:QObject(parent)
{
	d = new Private;
	connect(&d->sock, SIGNAL(connected()), SLOT(sock_connected()));
	connect(&d->sock, SIGNAL(connectionClosed()), SLOT(sock_connectionClosed()));
	connect(&d->sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
	connect(&d->sock, SIGNAL(error(int)), SLOT(sock_error(int)));
	reset(true);
}

HttpProxyPost::~HttpProxyPost()
{
	reset(true);
	delete d;
}

void HttpProxyPost::reset(bool clear)
{
	if(d->sock.state() != BSocket::Idle)
		d->sock.close();
	d->recvBuf.resize(0);
	if(clear)
		d->body.resize(0);
}

void HttpProxyPost::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

bool HttpProxyPost::isActive() const
{
	return (d->sock.state() == BSocket::Idle ? false: true);
}

void HttpProxyPost::post(const QString &proxyHost, int proxyPort, const QString &url, const QByteArray &data, bool asProxy)
{
	reset(true);

	d->host = proxyHost;
	d->url = url;
	d->postdata = data;
	d->asProxy = asProxy;

#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyPost: Connecting to %s:%d", proxyHost.latin1(), proxyPort);
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	d->sock.connectToHost(proxyHost, proxyPort);
}

void HttpProxyPost::stop()
{
	reset();
}

QByteArray HttpProxyPost::body() const
{
	return d->body;
}

QString HttpProxyPost::getHeader(const QString &var) const
{
	for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it) {
		const QString &s = *it;
		int n = s.indexOf(": ");
		if(n == -1)
			continue;
		QString v = s.mid(0, n);
		if(v.toLower() == var.toLower())
			return s.mid(n+2);
	}
	return "";
}

void HttpProxyPost::sock_connected()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyPost: Connected\n");
#endif
	d->inHeader = true;
	d->headerLines.clear();

	QUrl u = d->url;

	// connected, now send the request
	QString s;
	s += QString("POST ") + d->url + " HTTP/1.0\r\n";
	if(d->asProxy) {
		if(!d->user.isEmpty()) {
			QString str = d->user + ':' + d->pass;
			s += QString("Proxy-Authorization: Basic ") + QCA::Base64().encodeString(str) + "\r\n";
		}
		s += "Pragma: no-cache\r\n";
		s += QString("Host: ") + u.host() + "\r\n";
	}
	else {
		s += QString("Host: ") + d->host + "\r\n";
	}
	s += "Content-Type: application/x-www-form-urlencoded\r\n";
	s += QString("Content-Length: ") + QString::number(d->postdata.size()) + "\r\n";
	s += "\r\n";

	// write request
	d->sock.write(s.toUtf8());

	// write postdata
	d->sock.write(d->postdata);
}

void HttpProxyPost::sock_connectionClosed()
{
	d->body = d->recvBuf;
	reset();
	result();
}

void HttpProxyPost::sock_readyRead()
{
	QByteArray block = d->sock.read();
	ByteStream::appendArray(&d->recvBuf, block);

	if(d->inHeader) {
		// grab available lines
		while(1) {
			bool found;
			QString line = extractLine(&d->recvBuf, &found);
			if(!found)
				break;
			if(line.isEmpty()) {
				d->inHeader = false;
				break;
			}
			d->headerLines += line;
		}

		// done with grabbing the header?
		if(!d->inHeader) {
			QString str = d->headerLines.first();
			d->headerLines.takeFirst();

			QString proto;
			int code;
			QString msg;
			if(!extractMainHeader(str, &proto, &code, &msg)) {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: invalid header!\n");
#endif
				reset(true);
				error(ErrProxyNeg);
				return;
			}
			else {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: header proto=[%s] code=[%d] msg=[%s]\n", proto.latin1(), code, msg.latin1());
				for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it)
					fprintf(stderr, "HttpProxyPost: * [%s]\n", (*it).latin1());
#endif
			}

			if(code == 200) { // OK
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: << Success >>\n");
#endif
			}
			else {
				int err;
				QString errStr;
				if(code == 407) { // Authentication failed
					err = ErrProxyAuth;
					errStr = tr("Authentication failed");
				}
				else if(code == 404) { // Host not found
					err = ErrHostNotFound;
					errStr = tr("Host not found");
				}
				else if(code == 403) { // Access denied
					err = ErrProxyNeg;
					errStr = tr("Access denied");
				}
				else if(code == 503) { // Connection refused
					err = ErrConnectionRefused;
					errStr = tr("Connection refused");
				}
				else { // invalid reply
					err = ErrProxyNeg;
					errStr = tr("Invalid reply");
				}

#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyPost: << Error >> [%s]\n", errStr.latin1());
#endif
				reset(true);
				error(err);
				return;
			}
		}
	}
}

void HttpProxyPost::sock_error(int x)
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyPost: socket error: %d\n", x);
#endif
	reset(true);
	if(x == BSocket::ErrHostNotFound)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrConnectionRefused)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrRead)
		error(ErrProxyNeg);
}

//----------------------------------------------------------------------------
// HttpProxyGetStream
//----------------------------------------------------------------------------
class HttpProxyGetStream::Private
{
public:
	Private() {}

	BSocket sock;
	QByteArray recvBuf;
	QString url;
	QString user, pass;
	bool inHeader;
	QStringList headerLines;
	bool use_ssl;
	bool asProxy;
	QString host;
	int length;

	QCA::TLS *tls;
};

HttpProxyGetStream::HttpProxyGetStream(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->tls = 0;
	connect(&d->sock, SIGNAL(connected()), SLOT(sock_connected()));
	connect(&d->sock, SIGNAL(connectionClosed()), SLOT(sock_connectionClosed()));
	connect(&d->sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
	connect(&d->sock, SIGNAL(error(int)), SLOT(sock_error(int)));
	reset(true);
}

HttpProxyGetStream::~HttpProxyGetStream()
{
	reset(true);
	delete d;
}

void HttpProxyGetStream::reset(bool /*clear*/)
{
	if(d->tls) {
		delete d->tls;
		d->tls = 0;
	}
	if(d->sock.state() != BSocket::Idle)
		d->sock.close();
	d->recvBuf.resize(0);
	//if(clear)
	//	d->body.resize(0);
	d->length = -1;
}

void HttpProxyGetStream::setAuth(const QString &user, const QString &pass)
{
	d->user = user;
	d->pass = pass;
}

bool HttpProxyGetStream::isActive() const
{
	return (d->sock.state() == BSocket::Idle ? false: true);
}

void HttpProxyGetStream::get(const QString &proxyHost, int proxyPort, const QString &url, bool ssl, bool asProxy)
{
	reset(true);

	d->host = proxyHost;
	d->url = url;
	d->use_ssl = ssl;
	d->asProxy = asProxy;

#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyGetStream: Connecting to %s:%d", proxyHost.latin1(), proxyPort);
	if(d->user.isEmpty())
		fprintf(stderr, "\n");
	else
		fprintf(stderr, ", auth {%s,%s}\n", d->user.latin1(), d->pass.latin1());
#endif
	d->sock.connectToHost(proxyHost, proxyPort);
}

void HttpProxyGetStream::stop()
{
	reset();
}

QString HttpProxyGetStream::getHeader(const QString &var) const
{
	for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it) {
		const QString &s = *it;
		int n = s.indexOf(": ");
		if(n == -1)
			continue;
		QString v = s.mid(0, n);
		if(v.toLower() == var.toLower())
			return s.mid(n+2);
	}
	return "";
}

int HttpProxyGetStream::length() const
{
	return d->length;
}

void HttpProxyGetStream::sock_connected()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyGetStream: Connected\n");
#endif
	if(d->use_ssl) {
		d->tls = new QCA::TLS;
		connect(d->tls, SIGNAL(readyRead()), SLOT(tls_readyRead()));
		connect(d->tls, SIGNAL(readyReadOutgoing()), SLOT(tls_readyReadOutgoing()));
		connect(d->tls, SIGNAL(error()), SLOT(tls_error()));
		d->tls->startClient();
	}

	d->inHeader = true;
	d->headerLines.clear();

	QUrl u = d->url;

	// connected, now send the request
	QString s;
	s += QString("GET ") + d->url + " HTTP/1.0\r\n";
	if(d->asProxy) {
		if(!d->user.isEmpty()) {
			QString str = d->user + ':' + d->pass;
			s += QString("Proxy-Authorization: Basic ") + QCA::Base64().encodeString(str) + "\r\n";
		}
		s += "Pragma: no-cache\r\n";
		s += QString("Host: ") + u.host() + "\r\n";
	}
	else {
		s += QString("Host: ") + d->host + "\r\n";
	}
	s += "\r\n";

	// write request
	if(d->use_ssl)
		d->tls->write(s.toUtf8());
	else
		d->sock.write(s.toUtf8());
}

void HttpProxyGetStream::sock_connectionClosed()
{
	//d->body = d->recvBuf;
	reset();
	emit finished();
}

void HttpProxyGetStream::sock_readyRead()
{
	QByteArray block = d->sock.read();

	if(d->use_ssl)
		d->tls->writeIncoming(block);
	else
		processData(block);
}

void HttpProxyGetStream::processData(const QByteArray &block)
{
	printf("processData: %d bytes\n", block.size());
	if(!d->inHeader) {
		emit dataReady(block);
		return;
	}

	ByteStream::appendArray(&d->recvBuf, block);

	if(d->inHeader) {
		// grab available lines
		while(1) {
			bool found;
			QString line = extractLine(&d->recvBuf, &found);
			if(!found)
				break;
			if(line.isEmpty()) {
				printf("empty line\n");
				d->inHeader = false;
				break;
			}
			d->headerLines += line;
			printf("headerLine: [%s]\n", qPrintable(line));
		}

		// done with grabbing the header?
		if(!d->inHeader) {
			QString str = d->headerLines.first();
			d->headerLines.takeFirst();

			QString proto;
			int code;
			QString msg;
			if(!extractMainHeader(str, &proto, &code, &msg)) {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyGetStream: invalid header!\n");
#endif
				reset(true);
				error(ErrProxyNeg);
				return;
			}
			else {
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyGetStream: header proto=[%s] code=[%d] msg=[%s]\n", proto.latin1(), code, msg.latin1());
				for(QStringList::ConstIterator it = d->headerLines.begin(); it != d->headerLines.end(); ++it)
					fprintf(stderr, "HttpProxyGetStream: * [%s]\n", (*it).latin1());
#endif
			}

			if(code == 200) { // OK
#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyGetStream: << Success >>\n");
#endif

				bool ok;
				int x = getHeader("Content-Length").toInt(&ok);
				if(ok)
					d->length = x;

				QPointer<QObject> self = this;
				emit handshaken();
				if(!self)
					return;
			}
			else {
				int err;
				QString errStr;
				if(code == 407) { // Authentication failed
					err = ErrProxyAuth;
					errStr = tr("Authentication failed");
				}
				else if(code == 404) { // Host not found
					err = ErrHostNotFound;
					errStr = tr("Host not found");
				}
				else if(code == 403) { // Access denied
					err = ErrProxyNeg;
					errStr = tr("Access denied");
				}
				else if(code == 503) { // Connection refused
					err = ErrConnectionRefused;
					errStr = tr("Connection refused");
				}
				else { // invalid reply
					err = ErrProxyNeg;
					errStr = tr("Invalid reply");
				}

#ifdef PROX_DEBUG
				fprintf(stderr, "HttpProxyGetStream: << Error >> [%s]\n", errStr.latin1());
#endif
				reset(true);
				error(err);
				return;
			}

			if(!d->recvBuf.isEmpty()) {
				QByteArray a = d->recvBuf;
				d->recvBuf.clear();
				emit dataReady(a);
			}
		}
	}
}

void HttpProxyGetStream::sock_error(int x)
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyGetStream: socket error: %d\n", x);
#endif
	reset(true);
	if(x == BSocket::ErrHostNotFound)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrConnectionRefused)
		error(ErrProxyConnect);
	else if(x == BSocket::ErrRead)
		error(ErrProxyNeg);
}

void HttpProxyGetStream::tls_readyRead()
{
	//printf("tls_readyRead\n");
	processData(d->tls->read());
}

void HttpProxyGetStream::tls_readyReadOutgoing()
{
	//printf("tls_readyReadOutgoing\n");
	d->sock.write(d->tls->readOutgoing());
}

void HttpProxyGetStream::tls_error()
{
#ifdef PROX_DEBUG
	fprintf(stderr, "HttpProxyGetStream: ssl error: %d\n", d->tls->errorCode());
#endif
	reset(true);
	error(ErrConnectionRefused); // FIXME: bogus error
}

// CS_NAMESPACE_END
