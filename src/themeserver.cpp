#include <QFile>
#include <QTcpServer>

#include "themeserver.h"


ThemeServer::ThemeServer(QObject *parent) :
    qhttp::server::QHttpServer(parent),
    handlerSeed(0)
{
	using namespace qhttp::server;
	listen( // listening on 0.0.0.0:8080
	        QHostAddress::LocalHost, 0,
	        [this](QHttpRequest* req, QHttpResponse* res)
	{
		// very global stuff first
		QString path = req->url().path();
		if (path.startsWith(QLatin1Literal("/psiglobal"))) {
			QString globFile = path.mid(sizeof("/psiglobal")); // no / because of null pointer
			if (globFile == QLatin1Literal("qwebchannel.js")) {
				QFile qwcjs(":/qtwebchannel/qwebchannel.js");
				if (qwcjs.open(QIODevice::ReadOnly)) {
					res->setStatusCode(qhttp::ESTATUS_OK);
					res->headers().insert("Content-Type", "application/javascript;charset=utf-8");
					res->end(qwcjs.readAll());
					return;
				}
			}
		}


		// PsiId identifies certiain element of interface we load contents for.
		// For example it could be currently opened chat window for some jid.
		// This id should be the same for all requests of the element.
		// As fallback url path is also checked for the id, but it's not
		// reliable and it also means PsiId should looks like a path.
		// If PsiId is not set (handle not found) then it's an invalid request.
		// HTTP referer header is even less reliable so it's not used here.

		// qhttp::server keeps headers lower-cased
		static QByteArray psiHdr = QByteArray::fromRawData("psiid", sizeof("psiid")-1);

		auto it = req->headers().constFind(psiHdr);
		Handler handler;
		if (it != req->headers().constEnd()) {
			handler = sessionHandlers.value(it.value());
		}
		if (!handler && path.size() > 1) { // if we have something after slash
			QString baPath = path.mid(1);
			auto it = sessionHandlers.begin();
			while (it != sessionHandlers.end()) {
				if (!it.value()) { /* garbage collecting */
					it = sessionHandlers.erase(it);
				} else {
					if (baPath.startsWith(it.key()) && (baPath.size() == it.key().size() ||
														!baPath[it.key().size()].isLetter()))
					{
						req->setProperty("basePath", QString('/') + it.key());
						const_cast<QUrl&>(req->url()).setPath(baPath.mid(it.key().size()));
						handler = it.value();
					}
					++it;
				}
			}
		}

		if (!handler) {
			qDebug("Handler for %s is not found", qPrintable(req->url().url()));
		}

		if (!handler || !handler(req, res)) {
			res->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
			res->end();
		}

		// http status 200
		//res->setStatusCode(qhttp::ESTATUS_OK);
		// the response body data
		//res->end("Hello World!\n");
		// automatic memory management for req/res
	});
	qDebug() << "Theme server is started at " << serverUrl();

}

quint16 ThemeServer::serverPort() const
{
	return tcpServer()->serverPort();
}

QHostAddress ThemeServer::serverAddress() const
{
	return tcpServer()->serverAddress();
}

QUrl ThemeServer::serverUrl()
{
	QUrl u;
	u.setScheme(QLatin1String("http"));
	u.setHost(tcpServer()->serverAddress().toString());
	u.setPort(tcpServer()->serverPort());
	return u;
}

QString ThemeServer::registerHandler(const Handler &handler)
{
	QString s;
	s.sprintf("t%x", handlerSeed);
	handlerSeed += 0x10;

	sessionHandlers.insert(s, handler);

	return s;
}

void ThemeServer::unregisterHandler(const QString &path)
{
	sessionHandlers.remove(path);
}

//void ThemeServer::registerSession(QObject *session)
//{
//	QString jid = session->property("jid").toString();
//	bool isMuc = session->property("isMuc").toBool();

//	QString key = buildKey(jid, isMuc);
//	sessions.insert(key, session);
//}

//void ThemeServer::unregisterSession(QObject *session)
//{
//	QString jid = session->property("jid").toString();
//	bool isMuc = session->property("isMuc").toBool();

//	QString key = buildKey(jid, isMuc);
//	sessions.remove(key);
//}

//QString ThemeServer::buildKey(const QString jid, bool isMuc) const
//{
//	return QString(QLatin1String("jid=%1&type=%2")).arg(jid, QLatin1String(isMuc?"muc":"chat"));
//}
