#include "webserver.h"

#include <QFile>
#include <QTcpServer>

WebServer::WebServer(QObject *parent) : qhttp::server::QHttpServer(parent)
{
    using namespace qhttp::server;
    listen( // listening on 0.0.0.0:8080
        QHostAddress::LocalHost, 0, [this](QHttpRequest *req, QHttpResponse *res) {
            // very global stuff first
            QString path = req->url().path();
            // qDebug() << "LOADING: " << path << serverPort();

            for (auto &h : pathHandlers) {
                if (path.startsWith(h.first)) {
                    if (h.second(req, res)) {
                        return;
                    }
                }
            }

            if (!defaultHandler || !defaultHandler(req, res)) {
                res->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
                res->end();
            }
        });
}

quint16 WebServer::serverPort() const { return tcpServer()->serverPort(); }

QHostAddress WebServer::serverAddress() const { return tcpServer()->serverAddress(); }

QUrl WebServer::serverUrl()
{
    QUrl u;
    u.setScheme(QLatin1String("http"));
    u.setHost(tcpServer()->serverAddress().toString());
    u.setPort(tcpServer()->serverPort());
    return u;
}

void WebServer::route(const char *path, const WebServer::Handler &handler)
{
    pathHandlers.append(QPair<QString, Handler>(QLatin1String(path), handler));
}

void WebServer::unroute(const char *path)
{
    QString pstr = QLatin1String(path);
    auto    it
        = std::remove_if(pathHandlers.begin(), pathHandlers.end(), [pstr](const auto &v) { return v.first == pstr; });
    pathHandlers.erase(it, pathHandlers.end());
}
