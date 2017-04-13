#include <QPointer>
#include <QBuffer>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
# include <QUrlQuery>
#endif

#include "psicon.h"
#include "chatviewthemeprovider_priv.h"
#include "psithemeprovider.h"
#include "psiiconset.h"
#ifdef QT_WEBENGINEWIDGETS_LIB
# include "themeserver.h"
#else
# include "networkaccessmanager.h"
# include <QNetworkRequest>
#endif
#include "xmpp_vcard.h"
#include "avatars.h"

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
#else
class AvatarHandler : public NAMDataHandler
{
public:
	bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
	{
		QString path = req.url().path();
		if (path.startsWith(QLatin1String("/psiglobal/avatar/"))) {
			QString hash = path.mid(sizeof("/psiglobal/avatar")); // no / because of null pointer
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
	bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const
	{
		QUrl url = req.url();
		QString path = url.path();
		if (!path.startsWith(QLatin1String("/psiicon/"))) {
			return false;
		}
		QString iconId = path.mid(sizeof("/psiicon/") - 1);
#ifdef HAVE_QT5
		int w = QUrlQuery(url.query()).queryItemValue("w").toInt();
		int h = QUrlQuery(url.query()).queryItemValue("h").toInt();
#else
		int w = url.queryItemValue("w").toInt();
		int h = url.queryItemValue("h").toInt();
#endif
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
			data = IconsetFactory::raw(url.path());
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
		if (path.startsWith("/psithemes/")) {
			QString fn = path.mid(sizeof("/psithemes"));
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
#if QT_WEBENGINEWIDGETS_LIB
	// init something here?
	themeServer = new ThemeServer(this);

	// handler reading data from themes directory
	ThemeServer::Handler themesDirHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
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

	ThemeServer::Handler iconsHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
	{
		QString name = req->url().path().mid(sizeof("/psiicon"));
		QByteArray ba = IconsetFactory::raw(name);

		if (!ba.isEmpty()) {
			res->setStatusCode(qhttp::ESTATUS_OK);
			res->end(ba);
			return true;
		}
		return false;
	};

	ThemeServer::Handler avatarsHandler = [&](qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse* res) -> bool
	{
		QString hash = req->url().path().mid(sizeof("/psiglobal/avatar")); // no / because of null pointer
		QString meta;
		QByteArray ba;
		if (hash == QLatin1String("default.png")) {
			QPixmap p;
			QBuffer buffer(&ba);
			buffer.open(QIODevice::WriteOnly);
			p = IconsetFactory::icon(QLatin1String("psi/default_avatar")).pixmap();
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
				res->headers().insert("Content-Type", ad.metaType);
				res->end(ad.data);
				return true;
			}
		}

		return false;
	};

	themeServer->registerPathHandler("/psithemes/", themesDirHandler);
	themeServer->registerPathHandler("/psiicon/", iconsHandler);
	themeServer->registerPathHandler("/psiglobal/avatar/", avatarsHandler);

	requestInterceptor = new ChatViewUrlRequestInterceptor(this);
#else
	NetworkAccessManager::instance()->registerPathHandler(QSharedPointer<NAMDataHandler>(new ThemesDirHandler()));
	NetworkAccessManager::instance()->registerPathHandler(QSharedPointer<NAMDataHandler>(new IconHandler()));
	NetworkAccessManager::instance()->registerPathHandler(QSharedPointer<NAMDataHandler>(new AvatarHandler()));
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

