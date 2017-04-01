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

	QString registerSessionHandler(const Handler &handler);
	void unregisterSessionHandler(const QString &path);
	void registerPathHandler(const char *path, const Handler &handler);
private:

	QHash<QString,Handler> sessionHandlers;
	QList<QPair<QString,Handler>> pathHandlers;

};

#endif // THEMESERVER_H
