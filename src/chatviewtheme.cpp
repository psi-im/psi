/*
 * chatviewtheme.cpp - theme for webkit based chatview
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

#include "chatviewtheme.h"

#include "avatars.h"
#include "chatviewtheme_p.h"
#include "chatviewthemeprovider.h"
#include "chatviewthemeprovider_priv.h"
#include "coloropt.h"
#include "common.h"
#include "jsutil.h"
#include "psicon.h"
#include "psioptions.h"
#include "theme_p.h"
#include "webview.h"

#include <QApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QScopedPointer>
#include <QTimer>
#include <time.h>
#include <tuple>
#ifdef WEBENGINE
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <functional>
#else
#include <QNetworkRequest>
#include <QWebFrame>
#include <QWebPage>
#endif

#ifndef WEBENGINE
QVariant ChatViewThemePrivate::evaluateFromFile(const QString fileName, QWebFrame *frame)
{
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly)) {
        return frame->evaluateJavaScript(f.readAll());
    }
    return QVariant();
}
#endif

ChatViewThemePrivate::ChatViewThemePrivate(ChatViewThemeProvider *provider) : ThemePrivate(provider)
{
    nam = provider->psi()->networkAccessManager();
}

ChatViewThemePrivate::~ChatViewThemePrivate() { delete wv; }

bool ChatViewThemePrivate::exists()
{
    if (id.isEmpty()) {
        return false;
    }
    filepath = PsiThemeProvider::themePath(QLatin1String("chatview/") + id);
    return !filepath.isEmpty();
}

/**
 * @brief Sets theme bridge, starts loading procedure from javascript adapter.
 * @param file full path to theme directory
 * @param helperScripts adapter.js and util.js
 * @param adapterPath path to directry with adapter
 * @return true on success
 */
bool ChatViewThemePrivate::load(std::function<void(bool)> loadCallback)
{
    if (!exists()) {
        return false;
    }

    if (state == Theme::State::Loaded) {
        loadCallback(true);
        return true;
    }
    loadCallbacks.push_back(loadCallback);
    if (state == Theme::State::Loading) {
        return true;
    }

    qDebug("Starting loading \"%s\" theme at \"%s\" for %s", qPrintable(id), qPrintable(filepath),
           isMuc() ? "muc" : "chat");
    state = Theme::State::Loading;
    if (jsUtil.isNull()) {
        jsLoader.reset(new ChatViewJSLoader(this));
        jsUtil.reset(new ChatViewThemeJSUtil(this));
    }
    if (wv.isNull()) {
        wv = new WebView(nullptr);
    }

    QString themeType = id.section('/', 0, 0);
#ifdef WEBENGINE
    QWebChannel *channel = new QWebChannel(wv->page());
    wv->page()->setWebChannel(channel);
    channel->registerObject(QLatin1String("srvLoader"), jsLoader.data());
    channel->registerObject(QLatin1String("srvUtil"), jsUtil.data());

    // QString themeServer = ChatViewThemeProvider::serverAddr();
    wv->page()->setHtml(QString("<html><head>\n"
                                "<script src=\"/psi/themes/chatview/moment-with-locales.js\"></script>\n"
                                "<script src=\"/psi/themes/chatview/util.js\"></script>\n"
                                "<script src=\"/psi/themes/chatview/%1/adapter.js\"></script>\n"
                                "<script src=\"/psi/static/qwebchannel.js\"></script>\n"
                                "<script type=\"text/javascript\">\n"
                                "document.addEventListener(\"DOMContentLoaded\", function () {\n"
                                "new QWebChannel(qt.webChannelTransport, function (channel) {\n"
                                "window.srvLoader = channel.objects.srvLoader;\n"
                                "window.srvUtil = channel.objects.srvUtil;\n"
                                "initPsiTheme().adapter.loadTheme();\n"
                                "});\n"
                                "});\n"
                                "</script></head></html>")
                            .arg(themeType),
                        jsLoader->serverUrl());
    return true;
#else
    wv->page()->setNetworkAccessManager(nam);
    QStringList scriptPaths = QStringList()
        << PsiThemeProvider::themePath(QLatin1String("chatview/moment-with-locales.js"))
        << PsiThemeProvider::themePath(QLatin1String("chatview/util.js"))
        << PsiThemeProvider::themePath(QLatin1String("chatview/") + themeType + QLatin1String("/adapter.js"));

    wv->page()->mainFrame()->addToJavaScriptWindowObject("srvLoader", jsLoader.data(), QWebFrame::QtOwnership);
    wv->page()->mainFrame()->addToJavaScriptWindowObject("srvUtil", jsUtil.data(), QWebFrame::QtOwnership);

    foreach (const QString &sp, scriptPaths) {
        evaluateFromFile(sp, wv->page()->mainFrame());
    }

    QString resStr = wv->page()
                         ->mainFrame()
                         ->evaluateJavaScript("try { initPsiTheme().adapter.loadTheme(); \"ok\"; } "
                                              "catch(e) { \"Error:\" + e + \"\\n\" + window.psiim.util.props(e); }")
                         .toString();

    if (resStr == "ok") {
        return true;
    }
    qWarning("javascript part of the theme loader "
             "didn't return expected result: %s",
             qPrintable(resStr));
    return false;
#endif
}

bool ChatViewThemePrivate::hasPreview() const
{
    if (id.isNull())
        return false;

    QScopedPointer<Theme::ResourceLoader> loader(resourceLoader());
    return loader->fileExists("screenshot.png");
}

QWidget *ChatViewThemePrivate::previewWidget()
{
    QLabel *l = new QLabel;
    l->setPixmap(QPixmap::fromImage(QImage::fromData(loadData("screenshot.png"))));
    if (l->pixmap()->isNull()) {
        delete l;
        return nullptr;
    }
    return l;
}

bool ChatViewThemePrivate::isMuc() const { return dynamic_cast<GroupChatViewThemeProvider *>(provider); }

QVariantMap ChatViewThemePrivate::loadFromCacheMulti(const QVariantList &list)
{
    QVariantMap ret;
    for (auto &item : list) {
        QString key = item.toString();
        ret[key]    = cache.value(key);
        // if (key.endsWith("html"))
        //    qDebug() << "Loaded from cache" << key << ret[key].toString().left(50) << "priv=" << (void*)this;
    }
    return ret;
}

bool ChatViewThemePrivate::isTransparentBackground() const { return transparentBackground; }

#ifndef WEBENGINE
void ChatViewThemePrivate::embedSessionJsObject(ChatViewThemeSession *session)
{
    QWebFrame *wf = session->webView()->page()->mainFrame();
    wf->addToJavaScriptWindowObject("srvUtil", new ChatViewThemeJSUtil(this, session->webView()));
    wf->addToJavaScriptWindowObject("srvSession", session);

    QStringList scriptPaths = QStringList()
        << PsiThemeProvider::themePath(QLatin1String("chatview/moment-with-locales.js"))
        << PsiThemeProvider::themePath(QLatin1String("chatview/util.js"))
        << PsiThemeProvider::themePath(QLatin1String("chatview/") + id.section('/', 0, 0)
                                       + QLatin1String("/adapter.js"));

    foreach (const QString &script, scriptPaths) {
        evaluateFromFile(script, wf);
    }
}
#endif

bool ChatViewThemePrivate::applyToSession(ChatViewThemeSession *session)
{
#ifdef WEBENGINE
    QWebEnginePage *page = session->webView()->page();
    if (transparentBackground) {
        page->setBackgroundColor(Qt::transparent);
    }

    QWebChannel *channel = page->webChannel();
    QObject *    oldUtil = nullptr;
    if (channel) {
        oldUtil = channel->registeredObjects()[QLatin1String("srvUtil")];
        oldUtil->deleteLater();
        channel->deleteLater();
    }
    channel = new QWebChannel(session->webView());
    channel->registerObject(QLatin1String("srvUtil"), new ChatViewThemeJSUtil(this, session->webView()));
    channel->registerObject(QLatin1String("srvSession"), session);
    page->setWebChannel(channel);

    QPointer<ChatViewThemeSession> weakSession(session);
    auto handler = [weakSession, this](qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res) -> bool {
        auto session = weakSession.data();
        if (!weakSession) {
            return false;
        }
        bool handled
            = session->getContents(req->url(), [res](bool success, const QByteArray &data, const QByteArray &ctype) {
                  if (success) {
                      res->setStatusCode(qhttp::ESTATUS_OK);
                      res->headers().insert("Content-Type", ctype);
                  } else {
                      res->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
                  }
                  res->end(data);
              });
        if (handled) {
            return true;
        }
        // not handled by chat. try handle by theme
        QString path = req->url().path(); // fully decoded
        if (path.isEmpty() || path == QLatin1String("/")) {
            res->setStatusCode(qhttp::ESTATUS_OK);
            res->headers().insert("Content-Type", "text/html;charset=utf-8");

            if (prepareSessionHtml) { // html should be prepared for ech individual session
                // Even crazier stuff starts here.
                // Basically we send to theme's webview instance a signal to
                // generate html for specific session. It does it async.
                // Then javascript sends a signal the html is ready,
                // indicating sessionId and html contents.
                // And only then we close the request with hot html.

                jsLoader->connect(jsLoader.data(), &ChatViewJSLoader::sessionHtmlReady, session,
                                  [weakSession, res, this](const QString &sessionId, const QString &html) {
                                      auto session = weakSession.data();
                                      if (!weakSession) {
                                          return;
                                      }
                                      if (session->sessId == sessionId) {
                                          res->end(html.toUtf8()); // return html to client
                                          // and disconnect from loader
                                          jsLoader->disconnect(jsLoader.data(), &ChatViewJSLoader::sessionHtmlReady,
                                                               session, nullptr);
                                          jsLoader->unregisterSession(session->sessId);
                                      }
                                  });
                jsLoader->registerSession(session);
                QString basePath = req->property("basePath").toString();
                wv->page()->runJavaScript(
                    QString(QLatin1String("psiim.adapter.generateSessionHtml(\"%1\", %2, \"%3\")"))
                        .arg(session->sessId, session->propsAsJsonString(), basePath));

            } else {
                res->end(html.toUtf8());
            }
            return true;
        } else {
            bool       loaded;
            QByteArray data = loadData(httpRelPath + path, &loaded);
            if (loaded) {
                if (!data.isNull() && path.endsWith(QLatin1String(".tiff"))) {
                    // seems like we are loading tiff image which is supported by safari only.
                    // let's convert it
                    QImage     image(QImage::fromData(data));
                    QByteArray ba;
                    QBuffer    buffer(&ba);
                    buffer.open(QIODevice::WriteOnly);
                    image.save(&buffer, "PNG");
                    if (!ba.isNull()) {
                        data = ba;
                    }
                }
                if (path.endsWith(QLatin1String(".css"))) {
                    res->headers().insert("Content-Type", "text/css;charset=utf-8");
                }
                res->setStatusCode(qhttp::ESTATUS_OK);
                res->end(data);
                return true;
            }
        }
        return false;
    };

    session->sessId = ChatViewCon::instance()->registerSessionHandler(handler);
    QUrl      url   = ChatViewCon::instance()->serverUrl();
    QUrlQuery q;
    q.addQueryItem(QLatin1String("psiId"), session->sessId);
    url.setQuery(q);

    page->load(url);

    // QString id = provider->themeServer()->registerHandler(sessionObject);
    return true;
#else
    QWebPage *page = session->webView()->page();
    if (transparentBackground) {
        QPalette palette;
        palette = session->webView()->palette();
        palette.setBrush(QPalette::Base, Qt::transparent);
        page->setPalette(palette);
        session->webView()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    page->setNetworkAccessManager(nam);

    session->sessId = nam->registerSessionHandler(
        [session](const QNetworkRequest &req, QByteArray &data, QByteArray &mime) {
            Q_UNUSED(mime)
            data = session->theme.loadData(session->theme.priv<ChatViewThemePrivate>()->httpRelPath + req.url().path());
            if (!data.isNull()) {
                return true;
            }
            return false;
        });

    QString html;
    if (prepareSessionHtml) {
        QString basePath = "";
        jsLoader->registerSession(session);
        html = wv->page()
                   ->mainFrame()
                   ->evaluateJavaScript(QString(QLatin1String("psiim.adapter.generateSessionHtml(\"%1\", %2, \"%3\")"))
                                            .arg(session->sessId, session->propsAsJsonString(), basePath))
                   .toString();
        jsLoader->unregisterSession(session->sessId);
    } else {
        html = this->html;
    }

    page->mainFrame()->setHtml(html, jsLoader->serverUrl());

    return true;
#endif
}

//------------------------------------------------------------------------------
// ChatViewThemeJSLoader
//------------------------------------------------------------------------------
ChatViewJSLoader::ChatViewJSLoader(ChatViewThemePrivate *theme, QObject *parent) : QObject(parent), theme(theme) { }

const QString ChatViewJSLoader::themeId() const { return theme->id; }

bool ChatViewJSLoader::isMuc() const { return theme->isMuc(); }

QString ChatViewJSLoader::serverUrl() const
{
#ifdef WEBENGINE
    QUrl url = ChatViewCon::instance()->serverUrl();
    return url.url();
#else
    static QString url("http://psi");
    return url;
#endif
}

void ChatViewJSLoader::registerSession(ChatViewThemeSession *session)
{
    _sessions.insert(session->sessionId(), session);
}

void ChatViewJSLoader::unregisterSession(const QString &sessId) { _sessions.remove(sessId); }

void ChatViewJSLoader::_callFinishLoadCalbacks()
{
    for (auto &cb : theme->loadCallbacks) {
        cb(theme->state == Theme::State::Loaded);
    }
    theme->loadCallbacks.clear();
}

void ChatViewJSLoader::setMetaData(const QVariantMap &map)
{
    QString v = map["name"].toString();
    if (!v.isEmpty()) {
        theme->name = v;
    }

    v = map["description"].toString();
    if (!v.isEmpty()) {
        theme->description = v;
    }

    v = map["version"].toString();
    if (!v.isEmpty()) {
        theme->version = v;
    }

    QStringList vl = map["authors"].toStringList();
    if (vl.count()) {
        theme->authors = vl;
    }

    v = map["url"].toString();
    if (!v.isEmpty()) {
        theme->homeUrl = v;
    }
}

void ChatViewJSLoader::finishThemeLoading()
{
    qDebug("%s theme is successfully loaded for %s", qPrintable(theme->id), theme->isMuc() ? "muc" : "chat");
    Theme(theme).setState(Theme::State::Loaded);
#ifdef WEBENGINE
    _callFinishLoadCalbacks();
#else
    QTimer::singleShot(0, this, SLOT(_callFinishLoadCalbacks())); // let event loop do its job
#endif
}

void ChatViewJSLoader::errorThemeLoading(const QString &error)
{
    _loadError = error;
    Theme(theme).setState(Theme::State::NotLoaded);
#ifdef WEBENGINE
    _callFinishLoadCalbacks();
#else
    QTimer::singleShot(0, this, SLOT(_callFinishLoadCalbacks())); // let event loop do its job
#endif
}

void ChatViewJSLoader::setHtml(const QString &h) { theme->html = h; }

void ChatViewJSLoader::setHttpResourcePath(const QString &relPath) { theme->httpRelPath = relPath; }

void ChatViewJSLoader::toCache(const QString &name, const QVariant &data) { theme->cache.insert(name, data); }

void ChatViewJSLoader::saveFilesToCache(const QVariantMap &map)
{
    auto it = map.constBegin();
    while (it != map.constEnd()) {
        bool       loaded;
        QByteArray ba = Theme(theme).loadData(it.value().toString(), &loaded);
        if (loaded) {
            theme->cache.insert(it.key(), QString::fromUtf8(ba));
            // qDebug() << "Caching" << it.value() << "from" << Theme(theme).filePath()
            //         << theme->cache[it.key()] << "priv=" << (void*)theme;
        }
        ++it;
    }
}

QVariantMap ChatViewJSLoader::sessionProperties(const QString &sessionId, const QVariantList &props)
{
    auto        sess = _sessions.value(sessionId);
    QVariantMap ret;
    if (sess) {
        for (auto &p : props) {
            QString key = p.toString();
            ret.insert(key, sess->property(key.toUtf8().data()));
        }
    }
    return ret;
}

void ChatViewJSLoader::setCaseInsensitiveFS(bool state) { Theme(theme).setCaseInsensitiveFS(state); }

void ChatViewJSLoader::setPrepareSessionHtml(bool enabled) { theme->prepareSessionHtml = enabled; }

void ChatViewJSLoader::setSessionHtml(const QString &sessionId, const QString &html)
{
    emit sessionHtmlReady(sessionId, html);
}

QVariantMap ChatViewJSLoader::checkFilesExist(const QStringList &files, const QString baseDir)
{
    QVariantMap                           ret;
    QScopedPointer<Theme::ResourceLoader> loader(Theme(theme).resourceLoader());

    QString d(baseDir);
    if (!d.isEmpty()) {
        d += QLatin1Char('/');
    }
    foreach (const QString &f, files) {
        ret.insert(f, loader->fileExists(d + f));
    }

    return ret;
}

QString ChatViewJSLoader::getFileContents(const QString &name) const { return QString(Theme(theme).loadData(name)); }

QString ChatViewJSLoader::getFileContentsFromAdapterDir(const QString &name) const
{
    QString relPath  = QLatin1String("chatview/") + theme->id.split('/').first() + QLatin1Char('/') + name;
    QString filePath = theme->provider->themePath(relPath);
    if (filePath.isEmpty()) {
        qDebug("%s is not found", qPrintable(relPath));
        return QString();
    }
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray result = file.readAll();
        file.close();
        return QString::fromUtf8(result.constData(), result.size());
    } else {
        qDebug("Failed to open file %s: %s", qPrintable(file.fileName()), qPrintable(file.errorString()));
    }
    return QString();
}

void ChatViewJSLoader::setTransparent() { theme->transparentBackground = true; }

//------------------------------------------------------------------------------
// ChatViewThemeJSUtil
//------------------------------------------------------------------------------
ChatViewThemeJSUtil::ChatViewThemeJSUtil(ChatViewThemePrivate *theme, QObject *parent) : QObject(parent), theme(theme)
{
    psiDefaultAvatarUrl = "psi/avatar/default.png"; // relative to session url
    // may be in the future we can make different defaults. per transport for example

    optChangeTimer.setSingleShot(true);
    optChangeTimer.setInterval(0);
    connect(&optChangeTimer, SIGNAL(timeout()), SLOT(sendOptionsChanges()));
    connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString &)), SLOT(optionsChanged(const QString &)));
}

void ChatViewThemeJSUtil::sendOptionsChanges()
{
    emit optionsChanged(changedOptions);
    changedOptions.clear();
}

void ChatViewThemeJSUtil::optionsChanged(const QString &option)
{
    changedOptions.append(option);
    optChangeTimer.start();
}

void ChatViewThemeJSUtil::putToCache(const QString &key, const QVariant &data) { theme->cache.insert(key, data); }

QVariantMap ChatViewThemeJSUtil::loadFromCacheMulti(const QVariantList &list)
{
    return theme->loadFromCacheMulti(list);
}

QVariant ChatViewThemeJSUtil::cache(const QString &name) const { return theme->cache.value(name); }

QString ChatViewThemeJSUtil::psiOption(const QString &option) const
{
    return JSUtil::variant2js(PsiOptions::instance()->getOption(option));
}

QString ChatViewThemeJSUtil::psiOptions(const QStringList &options) const
{
    QVariantList ret;
    for (auto &option : options) {
        ret.append(PsiOptions::instance()->getOption(option));
    }
    QString retStr = JSUtil::variant2js(ret);
    return retStr;
}

QString ChatViewThemeJSUtil::colorOption(const QString &option) const
{
    return JSUtil::variant2js(ColorOpt::instance()->color(option));
}

QString ChatViewThemeJSUtil::formatDate(const QDateTime &dt, const QString &format) const
{
    return dt.toLocalTime().toString(format);
}

QString ChatViewThemeJSUtil::strftime(const QDateTime &dt, const QString &format) const
{
    char   str[256];
    time_t t = dt.toTime_t();
    int    s = int(::strftime(str, 256, format.toLocal8Bit(), localtime(&t)));
    if (s) {
        return QString::fromLocal8Bit(str, s);
    }
    return QString();
}

void ChatViewThemeJSUtil::console(const QString &text) const { qDebug("%s", qPrintable(text)); }

QString ChatViewThemeJSUtil::status2text(int status) const { return ::status2txt(status); }

QString ChatViewThemeJSUtil::hex2rgba(const QString &hex, float opacity)
{
    QColor color("#" + hex);
    color.setAlphaF(qreal(opacity));
    return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

//------------------------------------------------------------------------------
// ChatViewTheme
//------------------------------------------------------------------------------
ChatViewThemeSession::ChatViewThemeSession(QObject *parent) : QObject(parent) { }

ChatViewThemeSession::~ChatViewThemeSession()
{
#ifdef WEBENGINE
    ChatViewCon::instance()->unregisterSessionHandler(sessId);
#else
    theme.priv<ChatViewThemePrivate>()->nam->unregisterSessionHandler(sessId);
#endif
}

QString ChatViewThemeSession::propsAsJsonString()
{
    QJsonObject jsObj;
    int         pc = metaObject()->propertyCount();
    for (int i = 0; i < pc; i++) {
        QMetaProperty p = metaObject()->property(i);
        if (p.isReadable()) {
            QJsonValue v = QJsonValue::fromVariant(property(p.name()));
            if (!v.isNull()) {
                jsObj.insert(p.name(), v);
            }
        }
    }
    QJsonDocument doc(jsObj);
    return QString(doc.toJson(QJsonDocument::Compact));
}

void ChatViewThemeSession::init(const Theme &theme)
{
    this->theme = theme;
#ifndef WEBENGINE
    connect(webView()->page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), SLOT(embedJsObject()),
            Qt::UniqueConnection);
#endif
    auto priv = theme.priv<ChatViewThemePrivate>();
    priv->applyToSession(this);
}

#ifndef WEBENGINE
void ChatViewThemeSession::embedJsObject()
{
    auto priv = theme.priv<ChatViewThemePrivate>();
    priv->embedSessionJsObject(this);
}
#endif

bool ChatViewThemeSession::isTransparentBackground() const
{
    auto priv = theme.priv<ChatViewThemePrivate>();
    return priv->transparentBackground;
}
