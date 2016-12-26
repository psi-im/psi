#ifndef THEMESERVER_H
#define THEMESERVER_H

#include <QHash>
#include <QSharedPointer>
#include <functional>

#include "qhttpserver.hpp"
#include "qhttpserverrequest.hpp"
#include "qhttpserverresponse.hpp"

class Theme;

class ThemeServer : public qhttp::server::QHttpServer
{
	Q_OBJECT

	int handlerSeed;
public:

	typedef std::function<bool(qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res)> Handler;

	ThemeServer(QObject *parent = 0);
	quint16 serverPort() const;
	QHostAddress serverAddress() const;

	QUrl serverUrl();

	QString registerHandler(const Handler &handler);
	void unregisterHandler(const QString &path);
private:

	QHash<QString,Handler> sessionHandlers;

};

#endif // THEMESERVER_H
