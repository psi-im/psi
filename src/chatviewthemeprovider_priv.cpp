#include <QPointer>

#include "psicon.h"
#include "themeserver.h"
#include "chatviewthemeprovider_priv.h"
#include "psithemeprovider.h"

static QPointer<ChatViewCon> cvCon;

#if QT_WEBENGINEWIDGETS_LIB

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

#endif

ChatViewCon::ChatViewCon(PsiCon *pc) : QObject(pc), pc(pc)
{
#if QT_WEBENGINEWIDGETS_LIB
	// init something here?
	themeServer = new ThemeServer(this);

	// handler reading data from themes directory
	ThemeServer::Handler handler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
	{
		QString fn = req->url().path().mid(sizeof("/psithemes"));
		fn.replace("..", ""); // a little security
		fn = PsiThemeProvider::themePath(fn);

		if (!fn.isEmpty()) {
			QFile f(fn);
			if (f.open(QIODevice::ReadOnly)) {
				res->setStatusCode(qhttp::ESTATUS_OK);
				if (fn.endsWith(QLatin1String(".js"))) {
					res->headers().insert("Content-Type", "application/javascript;charset=utf-8");
				}
				res->end(f.readAll());
				f.close();
				return true;
			}
		}
		return false;
	};
	themeServer->registerPathHandler("/psithemes/", handler);

	requestInterceptor = new ChatViewUrlRequestInterceptor(this);
#else
	NetworkAccessManager::instance()->setSchemeHandler(
			"avatar", new PsiWKAvatarHandler(pc));
	NetworkAccessManager::instance()->setSchemeHandler(
			"theme", new ChatViewThemeUrlHandler());
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

