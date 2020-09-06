/*
 * chatview_webkit.cpp - Webkit based chatview
 * Copyright (C) 2010  Rion
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

#include "chatview_webkit.h"

#include "msgmle.h"
#include "psioptions.h"
#include "textutil.h"

#include <QWidget>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QJsonObject>
#include <QMetaObject>
#include <QMetaProperty>
#include <QJsonDocument>
#endif
#ifdef WEBENGINE
#if QT_VERSION >= QT_VERSION_CHECK(5,7,0)
#include <QWebEngineContextMenuData>
#endif
#include <QWebEngineSettings>
#else
#include <QWebPage>
#include <QWebFrame>
#include <QNetworkRequest>
#endif
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QPalette>
#include <QDesktopWidget>
#include <QApplication>
#include <QMetaProperty>
#include <QNetworkReply>


#include "webview.h"
//#include "psiapplication.h"
#include "psiaccount.h"
#include "applicationinfo.h"
#include "networkaccessmanager.h"
#include "jsutil.h"
#include "messageview.h"
#include "psithememanager.h"
#include "chatviewtheme.h"
#include "chatviewthemeprovider.h"
#include "avatars.h"
#include "desktoputil.h"
#include "psicon.h"


class ChatViewThemeSessionBridge;
class ChatViewJSObject;
class ChatViewPrivate
{
public:
	ChatViewPrivate() {}

	Theme theme;

	WebView *webView;
	ChatViewJSObject *jsObject;
	QList<QVariantMap> jsBuffer_;
	bool sessionReady_ = false;
	QPointer<QWidget> dialog_ = 0;
	bool isMuc_ = false;
	bool isEncryptionEnabled_ = false;
	Jid jid_;
	QString name_;
	PsiAccount *account_;
	AvatarFactory::UserHashes remoteIcons;
	AvatarFactory::UserHashes localIcons;

	inline ChatViewThemeProvider* themeProvider() const
	{
		return (ChatViewThemeProvider *)PsiThemeManager::instance()->
			                provider(isMuc_?"groupchatview":"chatview");
	}

	static QString closeIconTags(const QString &richText)
	{
		static QRegExp mIcon("(<icon [^>]+>)");
		QString s(richText);
		s.replace(mIcon, "\\1</icon>");
		return s;
	}
};


//----------------------------------------------------------------------------
// ChatViewJSObject
// object which will be embed to javascript part of view
//----------------------------------------------------------------------------
class ChatViewJSObject : public ChatViewThemeSession
{
	Q_OBJECT

	friend class ChatView; // we have a lot of suc hacks. time to think about redesign
	ChatView *_view;

	Q_PROPERTY(bool isMuc READ isMuc CONSTANT)
	Q_PROPERTY(QString chatName READ chatName CONSTANT)
	Q_PROPERTY(QString jid READ jid CONSTANT)
	Q_PROPERTY(QString account READ account CONSTANT)
	Q_PROPERTY(QString remoteUserImage READ remoteUserImage NOTIFY remoteUserImageChanged) // associated with chat(e.g. MUC's own avatar)
	Q_PROPERTY(QString remoteUserAvatar READ remoteUserAvatar NOTIFY remoteUserAvatarChanged) // remote avatar. resized vcard or PEP.
	Q_PROPERTY(QString localUserImage READ localUserImage NOTIFY localUserImageChanged)    // local image. from vcard
	Q_PROPERTY(QString localUserAvatar READ localUserAvatar NOTIFY localUserAvatarChanged) // local avatar. resized vcard or PEP.


public:
	ChatViewJSObject(ChatView *view) :
		ChatViewThemeSession(view),
		_view(view)
	{

	}

	// returns: data, content-type for http requests
	QPair<QByteArray,QByteArray> getContents(const QUrl &url)
	{
		Q_UNUSED(url)
		return QPair<QByteArray,QByteArray>();
	}

	WebView* webView()
	{
		return _view->textWidget();
	}

	bool isMuc() const
	{
		return _view->d->isMuc_;
	}

	QString chatName() const
	{
		return _view->d->name_;
	}

	QString jid() const
	{
		return _view->d->jid_.full();
	}

	QString account() const
	{
		return _view->d->account_->id();
	}

	inline static QString avatarUrl(const QString &hash)
	{ return hash.isEmpty()? QString() : QLatin1String("psiglobal/avatar/") + hash; }

	QString remoteUserImage()  const { return avatarUrl(_view->d->remoteIcons.vcard);  }
	QString remoteUserAvatar() const { return avatarUrl(_view->d->remoteIcons.avatar); }
	QString localUserImage()   const { return avatarUrl(_view->d->localIcons.vcard);   }
	QString localUserAvatar()  const { return avatarUrl(_view->d->localIcons.avatar);  }

	void setRemoteUserAvatarHash(const QString &hash)
	{ emit remoteUserAvatarChanged(hash.isEmpty()? hash : avatarUrl(hash)); }
	void setRemoteUserImageHash(const QString &hash)
	{ emit remoteUserImageChanged(hash.isEmpty()? hash : avatarUrl(hash)); }
	void setLocalUserAvatarHash(const QString &hash)
	{ emit localUserAvatarChanged(hash.isEmpty()? hash : avatarUrl(hash)); }
	void setLocalUserImageHash(const QString &hash)
	{ emit localUserImageChanged(hash.isEmpty()? hash : avatarUrl(hash)); }

public slots:
	QString mucNickColor(QString nick, bool isSelf,
						 QStringList validList = QStringList()) const
	{
		return _view->getMucNickColor(nick, isSelf, validList);
	}

	void signalInited()
	{
		emit inited();
	}

	QString getFont() const
	{
		QFont f = ((ChatView*)parent())->font();
		QString weight = "normal";
		switch (f.weight()) {
			case QFont::Light: weight = "lighter"; break;
			case QFont::DemiBold: weight = "bold"; break;
			case QFont::Bold: weight = "bolder"; break;
			case QFont::Black: weight = "900"; break;
		}

		// Workaround.  WebKit works only with 96dpi
		// Need to convert point size to pixel size
		int pixelSize = pointToPixel(f.pointSize());

		return QString("{fontFamily:'%1',fontSize:'%2px',fontStyle:'%3',fontVariant:'%4',fontWeight:'%5'}")
						 .arg(f.family())
						 .arg(pixelSize)
						 .arg(f.style()==QFont::StyleNormal?"normal":(f.style()==QFont::StyleItalic?"italic":"oblique"))
						 .arg(f.capitalization() == QFont::SmallCaps?"small-caps":"normal")
						 .arg(weight);
	}

	QString getPaletteColor(const QString &name) const
	{
		QPalette::ColorRole cr = QPalette::NoRole;

		if (name == "WindowText") {
			cr = QPalette::WindowText;
		} else if (name == "Text") {
			cr = QPalette::Text;
		} else if (name == "Base") {
			cr = QPalette::Base;
		} else if (name == "Window") {
			cr = QPalette::Window;
		} else if (name == "Highlight") {
			cr = QPalette::Highlight;
		} else if (name == "HighlightedText") {
			cr = QPalette::HighlightedText;
		} else if (name.endsWith("Text")) {
			cr = QPalette::Text;
		} else {
			cr = QPalette::Base;
		}

		return _view->palette().color(cr).name();
	}

	void nickInsertClick(const QString &nick)
	{
		emit _view->nickInsertClick(nick);
	}

	void getUrlHeaders(const QString &tId, const QString url)
	{
		QNetworkRequest req(QUrl::fromEncoded(url.toLatin1()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
		req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
		req.setMaximumRedirectsAllowed(2);
#endif
 		auto reply = _view->d->account_->psi()->networkAccessManager()->head(req);
		reply->setProperty("tranId", tId);
		connect(reply, SIGNAL(finished()), SLOT(onUrlHeadersReady()));
	}

private slots:
	void onUrlHeadersReady()
	{
		QVariantMap msg;
		QVariantMap headers;
		QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());
		msg.insert("id", reply->property("tranId").toString());

		for (auto &p : reply->rawHeaderPairs()) {
			QString key(QString(p.first).toLower());
			QString value;
			if (key == QLatin1String("content-type")) {
				// workaround for qt bug #61300 which put headers from origial request and redirect request in one hash
				// other headers most likely are invalid too, but this one is important for us.
				value = QString::fromLatin1(p.second).section(',', -1).trimmed();
			} else {
				value = QString::fromLatin1(p.second);
			}
			headers.insert(key, value);
		}
		msg.insert("value", headers);
		msg.insert("type", "tranend");
		reply->deleteLater();
		emit newMessage(msg);
	}

signals:
	void inited(); // signal from this object to C++. Means ready to process messages
	void scrollRequested(int); // relative shift. signal towards js
	void remoteUserImageChanged(const QString &);
	void remoteUserAvatarChanged(const QString &);
	void localUserImageChanged(const QString &);
	void localUserAvatarChanged(const QString &);
	void newMessage(const QVariant &);
};

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------

#ifdef WEBENGINE
class ChatViewPage : public QWebEnginePage
{
protected:
	using QWebEnginePage::QWebEnginePage;
	bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
	{
		if (isMainFrame && (type == NavigationTypeLinkClicked ||
		                    type == NavigationTypeFormSubmitted ||
		                    type == NavigationTypeBackForward)) {
			DesktopUtil::openUrl(url);
			return false;
		}
		return true;
	}
};
#else
class ChatViewPage : public QWebPage
{
	Q_OBJECT

	ChatViewPrivate *cvd;

public:
	using QWebPage::QWebPage;

	void setCVPrivate(ChatViewPrivate *d)
	{
	    cvd = d;
	}

protected:
	QString userAgentForUrl(const QUrl &url) const
	{
		if (url.host() == QLatin1String("psi")) {
			return cvd->jsObject->sessionId();
		}
		return QWebPage::userAgentForUrl(url);
	}

	bool acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, NavigationType type)
	{
		bool isMainFrame = frame == cvd->webView->page()->mainFrame();
		if (isMainFrame && (type == NavigationTypeLinkClicked ||
		                    type == NavigationTypeFormSubmitted ||
		                    type == NavigationTypeBackOrForward)) {
			DesktopUtil::openUrl(request.url());
			return false;
		}
		return true;
	}
};

#endif

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent) :
    QFrame(parent),
    d(new ChatViewPrivate)
{
	d->jsObject = new ChatViewJSObject(this); /* It's a session bridge between html and c++ part */
	d->webView = new WebView(this);
	d->webView->setFocusPolicy(Qt::NoFocus);
	d->webView->setPage(new ChatViewPage(d->webView));
#ifndef WEBENGINE
    d->webView->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
#endif
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0,0,0,0);
	layout->addWidget(d->webView);
	setLayout(layout);
	setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	setLooks(d->webView);

#ifndef HAVE_X11	// linux has this feature built-in
	connect( PsiOptions::instance(), SIGNAL(optionChanged(QString)), SLOT(psiOptionChanged(QString)) ); //needed only for save autocopy state atm
	psiOptionChanged("options.ui.automatically-copy-selected-text"); // init autocopy connection
#endif
	connect(d->jsObject, SIGNAL(inited()), SLOT(sessionInited()));
}

ChatView::~ChatView()
{
#if defined(WEBENGINE) && QT_VERSION < QT_VERSION_CHECK(5,12,3)
	// next two lines is a workaround to some Qt(?) bug very similar to
	// QTBUG-48014 and bunch of others (deletes QWidget twice).
	// The bug was last time reproduced with Qt-5.9. algo is pretty simple:
	// Connect to any conference and quit Psi.
	d->webView->setParent(0);
	d->webView->deleteLater();
#endif
}

// something after we know isMuc and dialog is set. kind of final step
void ChatView::init()
{
	Theme curTheme = d->themeProvider()->current();
	if (curTheme.state() != Theme::Loaded) {
		qDebug("ChatView theme is not loaded. this is fatal");
		return;
	}
	d->theme = curTheme;// TODO rewrite this pointer magic

#ifndef WEBENGINE
	((ChatViewPage*)d->webView->page())->setCVPrivate(d.data());
#endif
	d->jsObject->init(d->theme);

	bool tbg = d->jsObject->isTransparentBackground();
	QWidget *w = this;
	while (w) {
		w->setAttribute(Qt::WA_TranslucentBackground, tbg);
		w = w->parentWidget();
	}
}

void ChatView::setEncryptionEnabled(bool enabled)
{
	d->isEncryptionEnabled_ = enabled;
}

void ChatView::markReceived(QString id)
{
	QVariantMap m;
	m["type"] = "receipt";
	m["id"] = id;
	m["encrypted"] = d->isEncryptionEnabled_;
	sendJsObject(m);
}

QSize ChatView::sizeHint() const
{
	return minimumSizeHint();
}

void ChatView::setDialog(QWidget* dialog)
{
	d->dialog_ = dialog;
}

void ChatView::setSessionData(bool isMuc, const Jid &jid, const QString name)
{
	d->isMuc_ = isMuc;
	d->jid_ = jid;
	d->name_ = name;

	connect(d->themeProvider(), SIGNAL(themeChanged()), SLOT(init()));
}

void ChatView::setAccount(PsiAccount *acc)
{
	d->account_ = acc;
	d->remoteIcons = acc->avatarFactory()->userHashes(d->jid_);
	d->localIcons = acc->avatarFactory()->userHashes(acc->jid());
}

void ChatView::contextMenuEvent(QContextMenuEvent *e)
{
#if defined(WEBENGINE) && QT_VERSION < QT_VERSION_CHECK(5,7,0)
	Q_UNUSED(e)
	qDebug("Can't check menu hit point. Calling default handler");
#else
	QUrl linkUrl;
# ifdef WEBENGINE
	QWebEngineContextMenuData cmd = d->webView->page()->contextMenuData();
	linkUrl = cmd.linkUrl();
# else
	linkUrl = d->webView->page()->mainFrame()->hitTestContent(e->pos()).linkUrl();
# endif
	if ( linkUrl.scheme() == "addnick" ) {
		showNM(linkUrl.path().mid(1));
		e->accept();
	}
#endif
}

bool ChatView::focusNextPrevChild(bool next)
{
	return QWidget::focusNextPrevChild(next);
}

void ChatView::changeEvent(QEvent * event)
{
	if ( event->type() == QEvent::ApplicationPaletteChange
		|| event->type() == QEvent::PaletteChange
		|| event->type() == QEvent::FontChange ) {
		QVariantMap m;
		m["type"] = "settings";
		sendJsObject(m);
	}
	QFrame::changeEvent(event);
}

void ChatView::psiOptionChanged(const QString &option)
{
	if (option == "options.ui.automatically-copy-selected-text") {
		if (PsiOptions::instance()->
			getOption("options.ui.automatically-copy-selected-text").toBool()) {
			connect(d->webView->page(), SIGNAL(selectionChanged()), d->webView, SLOT(copySelected()));
		} else {
			disconnect(d->webView->page(), SIGNAL(selectionChanged()), d->webView, SLOT(copySelected()));
		}
	}
}

void ChatView::sendJsObject(const QVariantMap &map)
{
	d->jsBuffer_.append(map);
	checkJsBuffer();
}

void ChatView::checkJsBuffer()
{
	if (d->sessionReady_) {
		while (!d->jsBuffer_.isEmpty()) {
			d->jsObject->newMessage(d->jsBuffer_.takeFirst());
		}
	}
}

void ChatView::sessionInited()
{
	qDebug("Session is initialized");
	d->sessionReady_ = true;
	checkJsBuffer();
}

bool ChatView::handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit) {
	if (object == chatEdit && event->type() == QEvent::ShortcutOverride &&
		((QKeyEvent*)event)->matches(QKeySequence::Copy)) {

		if (!chatEdit->textCursor().hasSelection() &&
			 !(d->webView->page()->selectedText().isEmpty()))
		{
			d->webView->copySelected();
			return true;
		}
	}

	return false;
}

// input point of all messages
void ChatView::dispatchMessage(const MessageView &mv)
{
	QString replaceId = mv.replaceId();
	if (replaceId.isEmpty() && (mv.type() == MessageView::Message || mv.type() == MessageView::Subject)
			&& updateLastMsgTime(mv.dateTime())) {
		QVariantMap m;
		m["date"] = mv.dateTime();
		m["type"] = "message";
		m["mtype"] = "lastDate";
		sendJsObject(m);
	}
	QVariantMap vm = mv.toVariantMap(d->isMuc_, true);
	if (mv.type() == MessageView::MUCJoin) {
		Jid j = d->jid_.withResource(mv.nick());
		vm["avatar"] = ChatViewJSObject::avatarUrl(d->account_->avatarFactory()->userHashes(j).avatar);
		vm["nickcolor"] = getMucNickColor(mv.nick(), mv.isLocal());
	}
	auto it = vm.find(QLatin1String("usertext"));
	if (it != vm.end()) {
		*it = ChatViewPrivate::closeIconTags(it.value().toString());
	}
	it = vm.find(QLatin1String("message"));
	if (it != vm.end()) {
		*it = ChatViewPrivate::closeIconTags(it.value().toString());
	}

	vm["encrypted"] = d->isEncryptionEnabled_;
	if (!replaceId.isEmpty()) {
		vm["type"] = "replace";
		vm["replaceId"] = replaceId;
	} else {
		vm["mtype"] = vm["type"];
		vm["type"] = "message";
	}
	sendJsObject(vm);
}

void ChatView::scrollUp()
{
	emit d->jsObject->scrollRequested(-50);
}

void ChatView::scrollDown()
{
	emit d->jsObject->scrollRequested(50);
}

void ChatView::updateAvatar(const Jid &jid, ChatViewCommon::UserType utype)
{
	bool avatarChanged = false;
	bool vcardChanged = false;

	if (utype == RemoteParty) { // remote party but not muc participant
		auto h = d->account_->avatarFactory()->userHashes(jid);
		avatarChanged = h.avatar != d->remoteIcons.avatar;
		vcardChanged = h.vcard != d->remoteIcons.vcard;
		d->remoteIcons = h;
		if (avatarChanged) {
			d->jsObject->setRemoteUserAvatarHash(h.avatar);
		}
		if (vcardChanged) {
			d->jsObject->setRemoteUserAvatarHash(h.avatar);
		}
	} else if (utype == LocalParty) { // local party
		auto h = d->account_->avatarFactory()->userHashes(jid);
		avatarChanged = (h.avatar != d->localIcons.avatar);
		vcardChanged = (h.vcard != d->localIcons.vcard);
		d->localIcons = h;
		if (avatarChanged) {
			d->jsObject->setLocalUserAvatarHash(h.avatar);
		}
		if (vcardChanged) {
			d->jsObject->setLocalUserImageHash(h.avatar);
		}
	} else { // muc participant
		QVariantMap m;
		m["type"] = "avatar";
		m["sender"] = jid.resource();
		m["avatar"] = ChatViewJSObject::avatarUrl(d->account_->avatarFactory()->userHashes(jid).avatar);
		sendJsObject(m);
	}
}

void ChatView::clear()
{
	QVariantMap m;
	m["type"] = "clear";
	sendJsObject(m);
}

void ChatView::doTrackBar()
{
	QVariantMap m;
	m["type"] = "trackbar";
	sendJsObject(m);
}

bool ChatView::internalFind(QString str, bool startFromBeginning)
{
#ifdef WEBENGINE
	d->webView->page()->findText(str, QWebEnginePage::FindFlags(), [this, startFromBeginning](bool found) {
		if (!found && startFromBeginning) {
			d->webView->page()->findText(QString());
		}
    });
	return false;
#ifdef __GNUC__
#warning "TODO: make search asynchronous in all cases"
#endif
#else
	bool found = d->webView->page()->findText(str, startFromBeginning ?
				 QWebPage::FindWrapsAroundDocument : (QWebPage::FindFlag)0);

	if (!found && !startFromBeginning) {
		return internalFind(str, true);
	}

	return found;
#endif
}

WebView* ChatView::textWidget()
{
	return d->webView;
}

QWidget* ChatView::realTextWidget()
{
	return d->webView;
}

QObject *ChatView::jsBridge()
{
	return d->jsObject;
}

#include "chatview_webkit.moc"
