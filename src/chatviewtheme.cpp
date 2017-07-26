/*
 * chatviewtheme.cpp - theme for webkit based chatview
 * Copyright (C) 2010-2017 Sergey Ilinykh
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifdef WEBENGINE
#include <QWebEnginePage>
#include <QWebChannel>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineProfile>
#include <functional>
#else
#include <QWebPage>
#include <QWebFrame>
#include <QNetworkRequest>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QJsonObject>
#include <QJsonDocument>
#endif
#include <QMetaProperty>
#include <QFileInfo>
#include <QApplication>
#include <QScopedPointer>
#include <QTimer>
#include <time.h>
#include <tuple>

#include "chatviewtheme.h"
#include "chatviewtheme_p.h"
#include "psioptions.h"
#include "coloropt.h"
#include "jsutil.h"
#include "webview.h"
#include "chatviewthemeprovider.h"
#ifdef WEBENGINE
# include "themeserver.h"
#endif
#include "avatars.h"
#include "common.h"
#include "psicon.h"
#include "theme_p.h"
#include "jsutil.h"

#ifndef WEBENGINE
# ifdef HAVE_QT5
#  define QT_JS_QTOWNERSHIP QWebFrame::QtOwnership
# else
#  define QT_JS_QTOWNERSHIP QScriptEngine::QtOwnership
# endif
#endif


#ifndef WEBENGINE
class SessionRequestHandler : public NAMDataHandler
{
	ChatViewThemeSession *session;

public:
	SessionRequestHandler(ChatViewThemeSession *session) :
	    session(session) {}

	bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
	{
		Q_UNUSED(mime)
		data = session->theme.loadData(session->theme.priv<ChatViewThemePrivate>()->httpRelPath + req.url().path());
		if (!data.isNull()) {
			return true;
		}
		return false;
	}
};

QVariant ChatViewThemePrivate::evaluateFromFile(const QString fileName, QWebFrame *frame)
{
	QFile f(fileName);
	if (f.open(QIODevice::ReadOnly)) {
		return frame->evaluateJavaScript(f.readAll());
	}
	return QVariant();
}
#endif

ChatViewThemePrivate::ChatViewThemePrivate(ChatViewThemeProvider *provider) :
    ThemePrivate(provider)
{
	nam = provider->psi()->networkAccessManager();
}



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

	if (state == Theme::Loaded) {
		loadCallback(true);
		return true;
	}
	loadCallbacks.push_back(loadCallback);
	if (state == Theme::Loading) {
		return true;
	}

	qDebug("Starting loading \"%s\" theme at \"%s\"", qPrintable(id), qPrintable(filepath));
	state = Theme::Loading;
	if (jsUtil.isNull()) {
		jsLoader.reset(new ChatViewJSLoader(this));
		jsUtil.reset(new ChatViewThemeJSUtil(this));
	}
	if (wv.isNull()) {
		wv = new WebView(0);
	}

	QString themeType = id.section('/', 0, 0);
#ifdef WEBENGINE
	QWebChannel * channel = new QWebChannel(wv->page());
	wv->page()->setWebChannel(channel);
	channel->registerObject(QLatin1String("srvLoader"), jsLoader.data());
	channel->registerObject(QLatin1String("srvUtil"), jsUtil.data());

	//QString themeServer = ChatViewThemeProvider::serverAddr();
	wv->page()->setHtml(QString(
	    "<html><head>\n"
	    "<script src=\"/psithemes/chatview/moment-with-locales.js\"></script>\n"
	    "<script src=\"/psithemes/chatview/util.js\"></script>\n"
	    "<script src=\"/psithemes/chatview/%1/adapter.js\"></script>\n"
	    "<script src=\"/psiglobal/qwebchannel.js\"></script>\n"
		"<script type=\"text/javascript\">\n"
			"document.addEventListener(\"DOMContentLoaded\", function () {\n"
				"new QWebChannel(qt.webChannelTransport, function (channel) {\n"
					"window.srvLoader = channel.objects.srvLoader;\n"
					"window.srvUtil = channel.objects.srvUtil;\n"
	                "initPsiTheme().adapter.loadTheme();\n"
				"});\n"
			"});\n"
		"</script></head></html>").arg(themeType), jsLoader->serverUrl()
	);
	return true;
#else
	wv->page()->setNetworkAccessManager(nam);
	QStringList scriptPaths = QStringList()
	        << PsiThemeProvider::themePath(QLatin1String("chatview/moment-with-locales.js"))
	        << PsiThemeProvider::themePath(QLatin1String("chatview/util.js"))
	        << PsiThemeProvider::themePath(QLatin1String("chatview/") + themeType + QLatin1String("/adapter.js"));


	wv->page()->mainFrame()->addToJavaScriptWindowObject("srvLoader", jsLoader.data(), QT_JS_QTOWNERSHIP);
	wv->page()->mainFrame()->addToJavaScriptWindowObject("srvUtil", jsUtil.data(), QT_JS_QTOWNERSHIP);

	foreach (const QString &sp, scriptPaths) {
		evaluateFromFile(sp, wv->page()->mainFrame());
	}

	QString resStr = wv->page()->mainFrame()->evaluateJavaScript(
				"try { initPsiTheme().adapter.loadTheme(); \"ok\"; } "
				"catch(e) { \"Error:\" + e + \"\\n\" + window.psiim.util.props(e); }").toString();

	if (resStr == "ok") {
		return true;
	}
	qWarning("javascript part of the theme loader "
			 "didn't return expected result: %s", qPrintable(resStr));
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

bool ChatViewThemePrivate::isMuc() const
{
	return dynamic_cast<GroupChatViewThemeProvider*>(provider);
}

QVariantMap ChatViewThemePrivate::loadFromCacheMulti(const QVariantList &list)
{
	QVariantMap ret;
	for (auto &item : list) {
		QString key = item.toString();
		ret[key] = cache.value(key);
	}
	return ret;
}

bool ChatViewThemePrivate::isTransparentBackground() const
{
	return transparentBackground;
}

#ifndef WEBENGINE
void ChatViewThemePrivate::embedSessionJsObject(ChatViewThemeSession *session)
{
	QWebFrame *wf = session->webView()->page()->mainFrame();
	wf->addToJavaScriptWindowObject("srvUtil", new ChatViewThemeJSUtil(this, session->webView()));
	wf->addToJavaScriptWindowObject("srvSession", session);

	QStringList scriptPaths = QStringList()
	        << PsiThemeProvider::themePath(QLatin1String("chatview/moment-with-locales.js"))
	        << PsiThemeProvider::themePath(QLatin1String("chatview/util.js"))
	        << PsiThemeProvider::themePath(QLatin1String("chatview/") + id.section('/', 0, 0) + QLatin1String("/adapter.js"));

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
	if (!channel) {
		channel = new QWebChannel(session->webView());

		channel->registerObject(QLatin1String("srvUtil"), new ChatViewThemeJSUtil(this, session->webView()));
		channel->registerObject(QLatin1String("srvSession"), session);

		page->setWebChannel(channel);
		// channel is kept on F5 but all objects are cleared, so will be added later
	}

	ChatViewThemeProvider *cvProvider = static_cast<ChatViewThemeProvider*>(provider);

	auto server = cvProvider->themeServer();
	session->server = server;

	QPointer<ChatViewThemeSession> weakSession(session);
	auto handler = [weakSession,this](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
	{
		auto session = weakSession.data();
		if (!weakSession) {
			return false;
		}
		auto pair = session->getContents(req->url());
		if (pair.first.isNull()) {
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

					jsLoader->connect(jsLoader.data(),
					                  &ChatViewJSLoader::sessionHtmlReady,
					                  session,
					[weakSession, res, this](const QString &sessionId, const QString &html)
					{
						auto session = weakSession.data();
						if (!weakSession) {
							return;
						}
						if (session->sessId == sessionId) {
							res->end(html.toUtf8()); // return html to client
							// and disconnect from loader
							jsLoader->disconnect(
							            jsLoader.data(),
							            &ChatViewJSLoader::sessionHtmlReady,
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
				QByteArray data = Theme(this).loadData(httpRelPath + path);
				if (!data.isNull()) {
					res->setStatusCode(qhttp::ESTATUS_OK);
					res->end(data);
					return true;
				}
			}
			return false;
		}
		res->setStatusCode(qhttp::ESTATUS_OK);
		res->headers().insert("Content-Type", pair.second);
		res->end(pair.first);
		return true;
		// there is a chance the windows is closed already when we come here..
	};
	session->sessId = server->registerSessionHandler(handler);
	QUrl url = server->serverUrl();
	QUrlQuery q;
	q.addQueryItem(QLatin1String("psiId"), session->sessId);
	url.setQuery(q);

	page->load(url);

	//QString id = provider->themeServer()->registerHandler(sessionObject);
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

	SessionRequestHandler *handler = new SessionRequestHandler(session);
	session->sessId = nam->registerSessionHandler(QSharedPointer<NAMDataHandler>(handler));

	QString html;
	if (prepareSessionHtml) {
		QString basePath = "";
		jsLoader->registerSession(session);
		html = wv->page()->mainFrame()->evaluateJavaScript(
	            QString(QLatin1String("psiim.adapter.generateSessionHtml(\"%1\", %2, \"%3\")"))
	            .arg(session->sessId, session->propsAsJsonString(), basePath)).toString();
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
ChatViewJSLoader::ChatViewJSLoader(Theme theme, QObject *parent) :
    QObject(parent),
    theme(theme.priv<ChatViewThemePrivate>())
{}

const QString ChatViewJSLoader::themeId() const
{
	return theme->id;
}

bool ChatViewJSLoader::isMuc() const
{
	return theme->isMuc();
}

QString ChatViewJSLoader::serverUrl() const
{
#ifdef WEBENGINE
	ChatViewThemeProvider *cvProvider = static_cast<ChatViewThemeProvider*>(theme->provider);
	auto server = cvProvider->themeServer();
	QUrl url = server->serverUrl();
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

void ChatViewJSLoader::unregisterSession(const QString &sessId)
{
	_sessions.remove(sessId);
}

void ChatViewJSLoader::_callFinishLoadCalbacks()
{
	for (auto &cb : theme->loadCallbacks) {
		cb(theme->state == Theme::Loaded);
	}
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
	qDebug("%s theme is successfully loaded", qPrintable(theme->id));
	Theme(theme).setState(Theme::Loaded);
#ifdef WEBENGINE
	_callFinishLoadCalbacks();
#else
	QTimer::singleShot(0, this, SLOT(_callFinishLoadCalbacks())); // let event loop do its job
#endif
}

void ChatViewJSLoader::errorThemeLoading(const QString &error)
{
	_loadError = error;
	Theme(theme).setState(Theme::NotLoaded);
#ifdef WEBENGINE
	_callFinishLoadCalbacks();
#else
	QTimer::singleShot(0, this, SLOT(_callFinishLoadCalbacks())); // let event loop do its job
#endif
}

void ChatViewJSLoader::setHtml(const QString &h)
{
	theme->html = h;
}

void ChatViewJSLoader::setHttpResourcePath(const QString &relPath)
{
	theme->httpRelPath = relPath;
}

void ChatViewJSLoader::toCache(const QString &name, const QVariant &data)
{
	theme->cache.insert(name, data);
}

void ChatViewJSLoader::saveFilesToCache(const QVariantMap &map)
{
	auto it = map.constBegin();
	while (it != map.constEnd()) {
		QByteArray ba = Theme(theme).loadData(it.value().toString());
		if (!ba.isNull()) {
			theme->cache.insert(it.key(), QString::fromUtf8(ba));
		}
		++it;
	}
}

QVariantMap ChatViewJSLoader::sessionProperties(const QString &sessionId, const QVariantList &props)
{
	auto sess = _sessions.value(sessionId);
	QVariantMap ret;
	if (sess) {
		for (auto &p : props) {
			QString key = p.toString();
			ret.insert(key, sess->property(key.toUtf8().data()));
		}
	}
	return ret;
}

void ChatViewJSLoader::setCaseInsensitiveFS(bool state)
{
	Theme(theme).setCaseInsensitiveFS(state);
}

void ChatViewJSLoader::setPrepareSessionHtml(bool enabled)
{
	theme->prepareSessionHtml = enabled;
}

void ChatViewJSLoader::setSessionHtml(const QString &sessionId, const QString &html)
{
	emit sessionHtmlReady(sessionId, html);
}

QVariantMap ChatViewJSLoader::checkFilesExist(const QStringList &files, const QString baseDir)
{
	QVariantMap ret;
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

QString ChatViewJSLoader::getFileContents(const QString &name) const
{
	return QString(Theme(theme).loadData(name));
}

QString ChatViewJSLoader::getFileContentsFromAdapterDir(const QString &name) const
{
	QString adapterPath = theme->provider->themePath(QLatin1String("chatview/") + theme->id.split('/').first());
	QFile file(adapterPath + "/" + name);
	if (file.open(QIODevice::ReadOnly)) {
		QByteArray result = file.readAll();
		file.close();
		return QString::fromUtf8(result.constData(), result.size());
	} else {
		qDebug("Failed to open file %s: %s", qPrintable(file.fileName()),
		       qPrintable(file.errorString()));
	}
	return QString();
}

void ChatViewJSLoader::setTransparent()
{
	theme->transparentBackground = true;
}







//------------------------------------------------------------------------------
// ChatViewThemeJSUtil
//------------------------------------------------------------------------------
ChatViewThemeJSUtil::ChatViewThemeJSUtil(Theme theme, QObject *parent) :
    QObject(parent),
    theme(theme)
{
	psiDefaultAvatarUrl = "psiglobal/avatar/default.png"; // relative to session url
	// may be in the future we can make different defaults. per transport for example

	optChangeTimer.setSingleShot(true);
	optChangeTimer.setInterval(0);
	connect(&optChangeTimer, SIGNAL(timeout()), SLOT(sendOptionsChanges()));
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionsChanged(const QString&)));
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

void ChatViewThemeJSUtil::putToCache(const QString &key, const QVariant &data)
{
	theme.priv<ChatViewThemePrivate>()->cache.insert(key, data);
}

QVariantMap ChatViewThemeJSUtil::loadFromCacheMulti(const QVariantList &list)
{
	return theme.priv<ChatViewThemePrivate>()->loadFromCacheMulti(list);
}

QVariant ChatViewThemeJSUtil::cache(const QString &name) const
{
	return theme.priv<ChatViewThemePrivate>()->cache.value(name);
}

QString ChatViewThemeJSUtil::psiOption(const QString &option) const
{
	return JSUtil::variant2js(PsiOptions::instance()->getOption(option));
}

QString ChatViewThemeJSUtil::psiOptions(const QStringList &options) const
{
	QVariantList ret;
	for (auto &option: options) {
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
	char str[256];
	time_t t = dt.toTime_t();
	int s = ::strftime(str, 256, format.toLocal8Bit(), localtime(&t));
	if (s) {
		return QString::fromLocal8Bit(str, s);
	}
	return QString();
}

void ChatViewThemeJSUtil::console(const QString &text) const
{
	qDebug("%s", qPrintable(text));
}

QString ChatViewThemeJSUtil::status2text(int status) const
{
	return ::status2txt(status);
}

QString ChatViewThemeJSUtil::hex2rgba(const QString &hex, float opacity)
{
	QColor color("#" + hex);
	color.setAlphaF(opacity);
	return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green())
	        .arg(color.blue()).arg(color.alpha());
}

//------------------------------------------------------------------------------
// ChatViewTheme
//------------------------------------------------------------------------------
ChatViewThemeSession::ChatViewThemeSession(QObject *parent) :
    QObject(parent)
{

}

ChatViewThemeSession::~ChatViewThemeSession()
{
#ifdef WEBENGINE
	if (server) {
		server->unregisterSessionHandler(sessId);
	}
#else
	theme.priv<ChatViewThemePrivate>()->nam->unregisterSessionHandler(sessId);
#endif
}

QString ChatViewThemeSession::propsAsJsonString()
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
	QJsonObject jsObj;
#else
	QVariantMap jsObj;
#endif
	int pc = metaObject()->propertyCount();
	for (int i = 0; i < pc; i++) {
		QMetaProperty p = metaObject()->property(i);
		if (p.isReadable()) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
			QJsonValue v = QJsonValue::fromVariant(property(p.name()));
#else
			QVariant v = property(p.name());
#endif
			if (!v.isNull()) {
				jsObj.insert(p.name(), v);
			}
		}
	}
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
	QJsonDocument doc(jsObj);
	return QString(doc.toJson(QJsonDocument::Compact));
#else
	return JSUtil::map2json(jsObj);
#endif
}

void ChatViewThemeSession::init(const Theme &theme)
{
	this->theme = theme;
#ifndef WEBENGINE
	connect(webView()->page()->mainFrame(),
			SIGNAL(javaScriptWindowObjectCleared()), SLOT(embedJsObject()), Qt::UniqueConnection);
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
