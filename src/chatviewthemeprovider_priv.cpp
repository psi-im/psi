#include <QPointer>

#include "psicon.h"
#include "themeserver.h"
#include "chatviewthemeprovider_priv.h"

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

