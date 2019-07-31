/*
 * chatviewthemeprovider_priv.cpp
 * Copyright (C) 2010-2017  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "chatviewthemeprovider_priv.h"

#include <QBuffer>
#include <QPointer>
#include <QUrlQuery>
#ifdef WEBENGINE
#    include <QWebEngineProfile>
#else
#    include <QNetworkRequest>
#endif

#include "avatars.h"
#include "psicon.h"
#include "psiiconset.h"
#include "psithemeprovider.h"
#include "xmpp_vcard.h"
#ifdef WEBENGINE
#    include "webserver.h"
#else
#    include "networkaccessmanager.h"
#endif

static QPointer<ChatViewCon> cvCon;

#ifdef WEBENGINE

ChatViewUrlRequestInterceptor::ChatViewUrlRequestInterceptor(QObject *parent) :
    QWebEngineUrlRequestInterceptor(parent) {}

void ChatViewUrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    QString q = info.firstPartyUrl().query();
    if (q.startsWith(QLatin1String("psiId="))) {
        // handle urls like this http://127.0.0.1:12345/?psiId=ab4ba
        info.setHttpHeader(QByteArray("PsiId"), q.mid(sizeof("psiId=")-1).toUtf8());
    }
}
#else
class AvatarHandler : public NAMDataHandler
{
public:
    bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
    {
        QString path = req.url().path();
        if (path.startsWith(QLatin1String("/psi/avatar/"))) {
            QString hash = path.mid(sizeof("/psi/avatar")); // no / because of null pointer
            if (hash == QLatin1String("default.png")) {
                QPixmap p;
                QBuffer buffer(&data);
                buffer.open(QIODevice::WriteOnly);
                p = IconsetFactory::icon(QLatin1String("psi/default_avatar")).pixmap();
                if (p.save(&buffer, "PNG")) {
                    mime = "image/png";
                    return true;
                }
            } else {
                AvatarFactory::AvatarData ad = AvatarFactory::avatarDataByHash(hash);
                if (!ad.data.isEmpty()) {
                    data = ad.data;
                    mime = ad.metaType.toLatin1();
                    return true;
                }
            }
        }
        return false;
    }
};

class IconHandler : public NAMDataHandler
{
public:
    bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
    {
        QUrl url = req.url();
        QString path = url.path();
        if (!path.startsWith(QLatin1String("/psi/icon/"))) {
            return false;
        }
        QString iconId = path.mid(sizeof("/psi/icon/") - 1);
        int w = QUrlQuery(url.query()).queryItemValue("w").toInt();
        int h = QUrlQuery(url.query()).queryItemValue("h").toInt();
        PsiIcon icon = IconsetFactory::icon(iconId);
        if (w && h && !icon.isAnimated()) {
            QBuffer buffer(&data);
            buffer.open(QIODevice::WriteOnly);
            if (icon.pixmap().scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                    .toImage().save(&buffer, "PNG") && data.size()) {
                mime = "image/png";
                return true;
            }
        } else { //scaling impossible, return as is. do scaling with help of css or html attributes
            data = icon.raw();
            if (!data.isEmpty()) {
                mime = image2type(data).toLatin1();
                return true;
            }
        }

        return false;
    }
};

class ThemesDirHandler : public NAMDataHandler
{
public:
    bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
    {
        QString path = req.url().path();
        if (path.startsWith("/psi/themes/")) {
            QString fn = path.mid(sizeof("/psi/themes"));
            fn.replace("..", ""); // a little security
            fn = PsiThemeProvider::themePath(fn);

            if (!fn.isEmpty()) {
                QFile f(fn);
                if (f.open(QIODevice::ReadOnly)) {
                    if (fn.endsWith(QLatin1String(".js"))) {
                        mime = "application/javascript;charset=utf-8";
                    }
                    data = f.readAll();
                    f.close();
                    return true;
                }
            }
        }
        return false;
    }
};

#endif

ChatViewCon::ChatViewCon(PsiCon *pc) : QObject(pc), pc(pc)
{
#ifdef WEBENGINE
    // handler reading data from themes directory
    WebServer::Handler themesDirHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        QString fn = req->url().path().mid(sizeof("/psi/themes"));
        fn.replace("..", ""); // a little security
        fn = PsiThemeProvider::themePath(fn);

        if (!fn.isEmpty()) {
            QFile f(fn);
            if (f.open(QIODevice::ReadOnly)) {
                res->setStatusCode(qhttp::ESTATUS_OK);
                if (fn.endsWith(QLatin1String(".js"))) {
                    res->headers().insert("Content-Type", "application/javascript;charset=utf-8");
                }
                if (fn.endsWith(QLatin1String(".css"))) {
                    res->headers().insert("Content-Type", "text/css;charset=utf-8");
                }
                res->end(f.readAll());
                f.close();
                return true;
            }
        }
        return false;
    };

    WebServer::Handler iconsHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        QString name = req->url().path().mid(sizeof("/psi/icon"));
        QByteArray ba = IconsetFactory::raw(name);

        if (!ba.isEmpty()) {
            res->setStatusCode(qhttp::ESTATUS_OK);
            res->end(ba);
            return true;
        }
        return false;
    };

    WebServer::Handler avatarsHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        QString hash = req->url().path().mid(sizeof("/psi/avatar")); // no / because of null pointer
        QByteArray ba;
        if (hash == QLatin1String("default.png")) {
            QPixmap p;
            QBuffer buffer(&ba);
            buffer.open(QIODevice::WriteOnly);
            p = IconsetFactory::icon("psi/default_avatar").pixmap();
            if (p.save(&buffer, "PNG")) {
                res->setStatusCode(qhttp::ESTATUS_OK);
                res->headers().insert("Content-Type", "image/png");
                res->end(ba);
                return true;
            }
        } else {
            AvatarFactory::AvatarData ad = AvatarFactory::avatarDataByHash(hash);
            if (!ad.data.isEmpty()) {
                res->setStatusCode(qhttp::ESTATUS_OK);
                res->headers().insert("Content-Type", ad.metaType.toLatin1());
                res->end(ad.data);
                return true;
            }
        }

        return false;
    };

    WebServer::Handler qwebchannelHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        if (req->method() != qhttp::EHTTP_GET) return false;
        QFile qwcjs(":/qtwebchannel/qwebchannel.js");
        if (qwcjs.open(QIODevice::ReadOnly)) {
            res->setStatusCode(qhttp::ESTATUS_OK);
            res->headers().insert("Content-Type", "application/javascript;charset=utf-8");
            res->end(qwcjs.readAll());
        } else {
            res->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
        }
        return true;
    };

    WebServer::Handler faviconHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        if (req->method() != qhttp::EHTTP_GET) return false;
        res->setStatusCode(qhttp::ESTATUS_OK);
        res->end(IconsetFactory::icon(QLatin1String("psi/logo_16")).raw());
        return true;
    };

    WebServer::Handler defaultHandler = [this](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
    {
        // PsiId identifies certiain element of interface we load contents for.
        // For example it could be currently opened chat window for some jid.
        // This id should be the same for all requests of the element.
        // As fallback url path is also checked for the id, but it's not
        // reliable and it also means PsiId should look like a path.
        // If PsiId is not set (handle not found) then it's an invalid request.
        // HTTP referer header is even less reliable so it's not used here.

        // qhttp::server keeps headers lower-cased
        static QByteArray psiHdr = QByteArray::fromRawData("psiid", sizeof("psiid")-1);

        auto it = req->headers().constFind(psiHdr);
        WebServer::Handler handler;
        if (it != req->headers().constEnd()) {
            handler = sessionHandlers.value(it.value());
        }
        QString path;
        if (!handler) {
            path = req->url().path();
        }
        if (!handler && path.size() > 1) { // if we have something after slash
            QString baPath = path.mid(1);

            // baPath => session base path + remaining path
            //   session base path - is usually "tXXX" where XXX is a sequence number
            //
            // check all session base pathes to find best suitable baPath handler
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

        if (handler)
            return handler(req, res);

        return false;
    };

    auto ws = pc->webServer();
    ws->setDefaultHandler(defaultHandler);
    ws->route("/psi/themes/", themesDirHandler);
    ws->route("/psi/icon/",   iconsHandler);
    ws->route("/psi/avatar/", avatarsHandler);
    ws->route("/psi/static/qwebchannel.js", qwebchannelHandler);
    ws->route("/favicon.ico", faviconHandler);

    requestInterceptor = new ChatViewUrlRequestInterceptor(this);
    QWebEngineProfile::defaultProfile()->setRequestInterceptor(requestInterceptor);
#else
    pc->networkAccessManager()->registerPathHandler(QSharedPointer<NAMDataHandler>(new ThemesDirHandler()));
    pc->networkAccessManager()->registerPathHandler(QSharedPointer<NAMDataHandler>(new IconHandler()));
    pc->networkAccessManager()->registerPathHandler(QSharedPointer<NAMDataHandler>(new AvatarHandler()));
#endif
}

ChatViewCon::~ChatViewCon()
{
#ifdef WEBENGINE
    QWebEngineProfile::defaultProfile()->setRequestInterceptor(nullptr);
    delete requestInterceptor;
#endif
}

ChatViewCon *ChatViewCon::instance()
{
    return cvCon.data();
}

void ChatViewCon::init(PsiCon *pc) {
    if (!cvCon) {
        cvCon = new ChatViewCon(pc);
    }
}

bool ChatViewCon::isReady()
{
    return cvCon;
}

#ifdef WEBENGINE
QString ChatViewCon::registerSessionHandler(const WebServer::Handler &handler)
{
    QString s;
    s.sprintf("t%x", handlerSeed);
    handlerSeed += 0x10;

    sessionHandlers.insert(s, handler);

    return s;
}

void ChatViewCon::unregisterSessionHandler(const QString &path)
{
    sessionHandlers.remove(path);
}

QUrl ChatViewCon::serverUrl() const
{
    return pc->webServer()->serverUrl();
}
#endif
