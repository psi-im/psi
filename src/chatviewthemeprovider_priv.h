#ifndef CHATVIEWTHEMEPROVIDER_PRIV_H
#define CHATVIEWTHEMEPROVIDER_PRIV_H

#if WEBENGINE
#include <QWebEngineUrlRequestInterceptor>
#else
#include <QObject> // at least
#endif

class PsiCon;
class ThemeServer;

#if WEBENGINE
class ChatViewUrlRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
	Q_OBJECT
	Q_DISABLE_COPY(ChatViewUrlRequestInterceptor)
public:
	explicit ChatViewUrlRequestInterceptor(QObject *parent = Q_NULLPTR);
	void interceptRequest(QWebEngineUrlRequestInfo &info);
};
#endif

class ChatViewCon : public QObject
{
	Q_OBJECT

	PsiCon *pc;

	ChatViewCon(PsiCon *pc);
public:
#if WEBENGINE
	ThemeServer *themeServer;
	ChatViewUrlRequestInterceptor *requestInterceptor;
#endif
	static ChatViewCon* instance();
	static void init(PsiCon *pc);
	static bool isReady();
};

#endif // CHATVIEWTHEMEPROVIDER_PRIV_H
