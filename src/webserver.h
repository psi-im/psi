#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "qhttpserver.hpp"
#include "qhttpserverrequest.hpp"
#include "qhttpserverresponse.hpp"

#include <QObject>
#include <functional>

class WebServer : public qhttp::server::QHttpServer
{
    Q_OBJECT
public:
    typedef std::function<bool(qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res)> Handler;

    WebServer(QObject *parent = nullptr);

    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    QUrl serverUrl();

    void route(const char *path, const Handler &handler);
    void unroute(const char *path);

    inline void setDefaultHandler(const Handler &h) { defaultHandler = h; }
private:
    QList<QPair<QString,Handler>> pathHandlers;
    Handler defaultHandler;
};

#endif // WEBSERVER_H
