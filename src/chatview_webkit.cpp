/*
 * chatview_webkit.cpp - Webkit based chatview
 * Copyright (C) 2010  Sergey Ilinykh
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

#include "chatview_webkit.h"

#include "applicationinfo.h"
#include "avatars.h"
#include "chatviewtheme.h"
#include "chatviewthemeprovider.h"
#include "desktoputil.h"
#include "filesharingmanager.h"
#include "jsutil.h"
#include "messageview.h"
#include "msgmle.h"
#include "networkaccessmanager.h"
#ifdef PSI_PLUGINS
#include "pluginmanager.h"
#endif
#include "psiaccount.h"
// #include "psiapplication.h"
#include "psicon.h"
#include "psioptions.h"
#include "psithememanager.h"
#include "textutil.h"
#include "webview.h"
#include "xmpp_tasks.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMetaObject>
#include <QMetaProperty>
#include <QNetworkReply>
#include <QPalette>
#include <QWidget>
#ifdef WEBENGINE
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QWebEngineContextMenuRequest>
#elif QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#include <QWebEngineContextMenuData>
#endif
#include <QWebEngineSettings>
#else
#include <QNetworkRequest>
#include <QWebFrame>
#include <QWebPage>
#endif

class ChatViewJSObject;
class ChatViewThemeSessionBridge;

class ChatViewPrivate {
public:
    ChatViewPrivate(ChatView *q) : q(q) { }

    ChatView *q;

    Theme theme;

    WebView                  *webView     = nullptr;
    QAction                  *quoteAction = nullptr;
    ChatViewJSObject         *jsObject    = nullptr;
    QList<QVariantMap>        jsBuffer_;
    bool                      sessionReady_ = false;
    QPointer<QWidget>         dialog_;
    bool                      isMuc_               = false;
    bool                      isMucPrivate_        = false;
    bool                      isEncryptionEnabled_ = false;
    Jid                       jid_;
    QString                   name_;
    PsiAccount               *account_ = nullptr;
    AvatarFactory::UserHashes remoteIcons;
    AvatarFactory::UserHashes localIcons;
    ChatViewThemeProvider    *themeProvider = nullptr;
    QString                   localNickName;

    static QString closeIconTags(const QString &richText)
    {
        static QRegularExpression mIcon("(<icon [^>]+>)");
        QString                   s(richText);
        s.replace(mIcon, "\\1</icon>");
        return s;
    }

    QString prepareShares(const QString &msg)
    {
        static QRegularExpression re("<share id=\"([^\"]+)\"(?: +text=\"([^\"]+)\")?/>");
        int                       post = 0;
        QString                   ret;
        auto                      it = re.globalMatch(msg);
        while (it.hasNext()) {
            auto match = it.next();
            auto idStr = match.captured(1);
            auto text  = match.captured(2);
            auto id    = XMPP::Hash::from(idStr);
            auto item  = account_->psi()->fileSharingManager()->item(id);

            ret.append(QStringView { msg }.mid(post, match.capturedStart(0) - post));
            if (item) {
                auto    vm = item->metaData();
                QString attrs;
                attrs += QString(" id=\"%1\"").arg(idStr);
                attrs += QString(" text=\"%1\"").arg(text);
                auto metaType = item->mimeType();
                attrs += QString(" type=\"%1\"").arg(metaType);
                if (metaType.startsWith(QLatin1String("audio/"))) {
                    auto hg = vm.value(QLatin1String("amplitudes")).toByteArray();
                    if (hg.size()) {
                        QStringList l;
                        std::transform(hg.constBegin(), hg.constEnd(), std::back_inserter(l),
                                       [](char f) { return QString::number(int(quint8(f) / 2.55)); });
                        attrs += QString(" amplitudes=\"%1\"").arg(l.join(','));
                    }
                }
                ret.append(QString("<share%1></share>").arg(attrs));
            }
            post = match.capturedEnd(0);
        }
        ret.append(QStringView { msg }.mid(post));
        return ret;
    }

    void sendJsObject(const QVariantMap &map)
    {
        jsBuffer_.append(map);
        checkJsBuffer();
    }

    void checkJsBuffer();

    void sendReactionsToUI(const QString &nick, const QString &messageId, const QSet<QString> &reactions)
    {
        QVariantMap m;
        m["type"]      = QLatin1String("reactions");
        m["sender"]    = nick;
        m["messageid"] = messageId;
        auto rl        = q->updateReactions(nick, messageId, reactions);
        auto vl        = QVariantList();
        for (auto &r : std::as_const(rl)) {
            auto vmr = QVariantMap();
            if (!r.base.isEmpty()) {
                vmr[QLatin1String("base")] = r.base;
            }
            vmr[QLatin1String("text")]  = r.code;
            vmr[QLatin1String("nicks")] = r.nicks;
            vl << vmr;
        }
        m[QLatin1String("reactions")] = vl;
        sendJsObject(m);
    }
};

//----------------------------------------------------------------------------
// ChatViewJSObject
// object which will be embed to javascript part of view
//----------------------------------------------------------------------------
class ChatViewJSObject : public ChatViewThemeSession {
    Q_OBJECT

    friend class ChatView; // we have a lot of suc hacks. time to think about
                           // redesign
    ChatView *_view;

    Q_PROPERTY(bool isMuc READ isMuc CONSTANT)
    Q_PROPERTY(QString chatName READ chatName CONSTANT)
    Q_PROPERTY(QString jid READ jid CONSTANT)
    Q_PROPERTY(QString account READ account CONSTANT)
    Q_PROPERTY(QString remoteUserImage READ remoteUserImage NOTIFY
                   remoteUserImageChanged) // associated with chat(e.g. MUC's own avatar)
    Q_PROPERTY(QString remoteUserAvatar READ remoteUserAvatar NOTIFY
                   remoteUserAvatarChanged) // remote avatar. resized vcard or PEP.
    Q_PROPERTY(QString localUserImage READ localUserImage NOTIFY localUserImageChanged) // local image. from vcard
    Q_PROPERTY(QString localUserAvatar READ localUserAvatar NOTIFY
                   localUserAvatarChanged) // local avatar. resized vcard or PEP.

public:
    ChatViewJSObject(ChatView *view) : ChatViewThemeSession(view), _view(view) { }

    // accepts url of http request from chatlog.
    // returns to callback data and content-type.
    // if data is null then it's 404
    bool getContents(const QUrl                                                               &url,
                     std::function<void(bool success, const QByteArray &, const QByteArray &)> callback)
    {
        if (url.path().startsWith("/psibob/")) {
            QString cid    = url.path().mid(sizeof("/psibob/") - 1);
            auto    sender = QUrlQuery(url).queryItemValue("sender", QUrl::FullyDecoded);
            auto    j      = _view->d->jid_;
            if (!sender.isEmpty()) {
                j = j.withResource(sender);
            }
            _view->d->account_->loadBob(j, cid, this, callback);
            return true;
        }
        // qDebug("Unhandled url: %s", qPrintable(url.toString()));
        return false;
    }

    WebView *webView() { return _view->textWidget(); }

    bool isMuc() const { return _view->d->isMuc_; }

    QString chatName() const { return _view->d->name_; }

    QString jid() const { return _view->d->jid_.full(); }

    QString account() const { return _view->d->account_->id(); }

    inline static QString avatarUrl(const QByteArray &hash)
    {
        return hash.isEmpty() ? QString() : QLatin1String("psi/avatar/") + hash.toHex();
    }

    QString remoteUserImage() const { return avatarUrl(_view->d->remoteIcons.vcard); }
    QString remoteUserAvatar() const { return avatarUrl(_view->d->remoteIcons.avatar); }
    QString localUserImage() const { return avatarUrl(_view->d->localIcons.vcard); }
    QString localUserAvatar() const { return avatarUrl(_view->d->localIcons.avatar); }

    void setRemoteUserAvatarHash(const QByteArray &hash)
    {
        emit remoteUserAvatarChanged(hash.isEmpty() ? QString() : avatarUrl(hash));
    }
    void setRemoteUserImageHash(const QByteArray &hash)
    {
        emit remoteUserImageChanged(hash.isEmpty() ? QString() : avatarUrl(hash));
    }
    void setLocalUserAvatarHash(const QByteArray &hash)
    {
        emit localUserAvatarChanged(hash.isEmpty() ? QString() : avatarUrl(hash));
    }
    void setLocalUserImageHash(const QByteArray &hash)
    {
        emit localUserImageChanged(hash.isEmpty() ? QString() : avatarUrl(hash));
    }

    Q_INVOKABLE QString mucNickColor(QString nick, bool isSelf) const { return _view->getMucNickColor(nick, isSelf); }

    Q_INVOKABLE void signalInited() { emit inited(); }

    Q_INVOKABLE QString getFont() const
    {
        QFont   f      = static_cast<ChatView *>(parent())->font();
        QString weight = "normal";
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        switch (f.weight()) {
        case QFont::Light:
            weight = "lighter";
            break;
        case QFont::DemiBold:
            weight = "bold";
            break;
        case QFont::Bold:
        case QFont::ExtraBold:
            weight = "bolder";
            break;
        case QFont::Black:
            weight = "900";
            break;
        }
#else
        weight = QString::number(f.weight());
#endif

        // Workaround.  WebKit works only with 96dpi
        // Need to convert point size to pixel size
        int pixelSize = pointToPixel(f.pointSize());

        return QString("{fontFamily:'%1',fontSize:'%2px',fontStyle:'%3',"
                       "fontVariant:'%4',fontWeight:'%5'}")
            .arg(f.family())
            .arg(pixelSize)
            .arg(f.style() == QFont::StyleNormal ? "normal" : (f.style() == QFont::StyleItalic ? "italic" : "oblique"),
                 f.capitalization() == QFont::SmallCaps ? "small-caps" : "normal", weight);
    }

    Q_INVOKABLE QString getPaletteColor(const QString &name) const
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

    Q_INVOKABLE void nickInsertClick(const QString &nick) { emit _view->nickInsertClick(nick); }

    Q_INVOKABLE void getUrlHeaders(const QString &tId, const QString &url)
    {
        QNetworkRequest req(QUrl::fromEncoded(url.toLatin1()));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setMaximumRedirectsAllowed(2);
        auto reply = _view->d->account_->psi()->networkAccessManager()->head(req);
        reply->setProperty("tranId", tId);
        connect(reply, SIGNAL(finished()), SLOT(onUrlHeadersReady()));
    }

    Q_INVOKABLE void react(const QString &messageId, const QString &reaction)
    {
        if (messageId.isEmpty()) {
            qWarning("empty message id on sending reaction. broken theme?");
            return;
        }
        _view->outgoingReaction(messageId, reaction);
    }

    Q_INVOKABLE void deleteMessage(const QString &messageId)
    {
        emit _view->outgoingMessageRetraction(messageId);
        if (!_view->d->isMuc_) {
            // send back immediately coz it's not it's not iq and not MUC where server sends it back
            QVariantMap vm;
            vm["type"]     = QLatin1String("msgretract");
            vm["targetid"] = messageId;
            emit newMessage(vm);
        }
    }

    Q_INVOKABLE void replyMessage(const QString &messageId, const QString &quotedHtml)
    {
        auto plainText = TextUtil::rich2plain(quotedHtml);
        emit _view->quote(plainText);
    }

    Q_INVOKABLE void copyMessage(const QString &messageId, const QString &html)
    {
        auto plainText = TextUtil::rich2plain(html);
        QApplication::clipboard()->setText(plainText);
    }

    Q_INVOKABLE void showInfo(const QString &nick) { emit _view->openInfoRequested(nick); }

    Q_INVOKABLE void openChat(const QString &nick) { emit _view->openChatRequested(nick); }

    Q_INVOKABLE void kick(const QString &nick) { qDebug() << "kick" << nick; }

    Q_INVOKABLE void editMessage(const QString &messageId, const QString &messageHtml)
    {
        emit _view->editMessageRequested(messageId, TextUtil::rich2plain(messageHtml));
    }

    Q_INVOKABLE void forwardMessage(const QString &messageId, const QString &nick, const QString &messageHtml)
    {
        emit _view->forwardMessageRequested(messageId, nick, TextUtil::rich2plain(messageHtml));
    }

private slots:
    void onUrlHeadersReady()
    {
        QVariantMap    msg;
        QVariantMap    headers;
        QNetworkReply *reply = dynamic_cast<QNetworkReply *>(sender());
        msg.insert("id", reply->property("tranId").toString());

        for (auto &p : reply->rawHeaderPairs()) {
            QString key(QString(p.first).toLower());
            QString value;
#if QT_VERSION < QT_VERSION_CHECK(5, 9, 3)
            if (key == QLatin1String("content-type")) {
                // workaround for qt bug #61300 which put headers from origial request
                // and redirect request in one hash other headers most likely are
                // invalid too, but this one is important for us.
                value = QString::fromLatin1(p.second).section(',', -1).trimmed();
            } else
#endif
                value = QString::fromLatin1(p.second);
            headers.insert(key, value);
        }
        msg.insert("value", headers);
        msg.insert("type", "tranend");
        reply->deleteLater();
        emit newMessage(msg);
    }

signals:
    void inited();             // signal from this object to C++. Means ready to process messages
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
class ExtBrowserPage : public QWebEnginePage {
protected:
    using QWebEnginePage::QWebEnginePage;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
    {
        DesktopUtil::openUrl(url);
        deleteLater();
        return false;
    }
};

class ChatViewPage : public QWebEnginePage {
protected:
    using QWebEnginePage::QWebEnginePage;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
    {
        if (isMainFrame
            && (type == NavigationTypeLinkClicked || type == NavigationTypeFormSubmitted
                || type == NavigationTypeBackForward)) {
            DesktopUtil::openUrl(url);
            return false;
        }
        return true;
    }

    QWebEnginePage *createWindow(WebWindowType type) { return new ExtBrowserPage(); }
};
#else

class ExtBrowserPage : public QWebPage {
protected:
    using QWebPage::QWebPage;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
    {
        DesktopUtil::openUrl(url);
        deleteLater();
        return false;
    }
};

class ChatViewPage : public QWebPage {
    Q_OBJECT

    ChatViewPrivate *cvd;

public:
    using QWebPage::QWebPage;

    void setCVPrivate(ChatViewPrivate *d) { cvd = d; }

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
        if (isMainFrame
            && (type == NavigationTypeLinkClicked || type == NavigationTypeFormSubmitted
                || type == NavigationTypeBackOrForward)) {
            DesktopUtil::openUrl(request.url());
            return false;
        }
        return true;
    }

    QWebPage *createWindow(WebWindowType type) { return new ExtBrowserPage(); }
};

#endif

//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent) : QFrame(parent), d(new ChatViewPrivate(this))
{
    d->jsObject = new ChatViewJSObject(this); /* It's a session bridge between html and c++ part */
    d->webView  = new WebView(this);
    d->webView->setFocusPolicy(Qt::NoFocus);
    d->webView->setPage(new ChatViewPage(d->webView));
    d->webView->connectPageActions();

    d->quoteAction = new QAction(tr("Quote"), this);
    d->quoteAction->setShortcut(QKeySequence(tr("Ctrl+S")));
    d->webView->addContextMenuAction(d->quoteAction);
    connect(d->quoteAction, &QAction::triggered, this, [this](bool) { emit quote(d->webView->selectedText()); });
#ifndef WEBENGINE
    d->webView->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
#endif
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->webView);
    setLayout(layout);
    setFrameShadow(QFrame::Sunken);
    setFrameShape(QFrame::StyledPanel);
    setLooks(d->webView);

#ifndef HAVE_X11 // linux has this feature built-in
    connect(PsiOptions::instance(), SIGNAL(optionChanged(QString)),
            SLOT(psiOptionChanged(QString)));                        // needed only for save autocopy state atm
    psiOptionChanged("options.ui.automatically-copy-selected-text"); // init autocopy
                                                                     // connection
#endif
    connect(d->jsObject, &ChatViewJSObject::inited, this, &ChatView::sessionInited);

#ifdef PSI_PLUGINS
    QVariantMap m;
    m["type"]  = "receivehooks";
    m["hooks"] = PluginManager::instance()->messageViewJSFilters();
    d->sendJsObject(m);
    connect(PluginManager::instance(), &PluginManager::jsFiltersUpdated, this, [this]() {
        QVariantMap m;
        m["type"]  = "receivehooks";
        m["hooks"] = PluginManager::instance()->messageViewJSFilters();
        d->sendJsObject(m);
    });
#endif
}

ChatView::~ChatView()
{
#if defined(WEBENGINE) && QT_VERSION < QT_VERSION_CHECK(5, 12, 3)
    // next two lines is a workaround to some Qt(?) bug very similar to
    // QTBUG-48014 and bunch of others (deletes QWidget twice).
    // The bug was last time reproduced with Qt-5.9. algo is pretty simple:
    // Connect to any conference and quit Psi.
    d->webView->setParent(nullptr);
    d->webView->deleteLater();
#endif
}

// something after we know isMuc and dialog is set. kind of final step
void ChatView::init()
{
    Theme curTheme = d->themeProvider->current();
    // qDebug() << "Init chatview with theme" << curTheme.name();
    if (curTheme.state() != Theme::State::Loaded) {
        qDebug("ChatView theme is not loaded. this is fatal");
        return;
    }
    d->theme = curTheme;

#ifndef WEBENGINE
    ((ChatViewPage *)d->webView->page())->setCVPrivate(d.data());
#endif
    d->jsObject->init(d->theme);

    /* this commented out code is broken. try to fix it if you can.
    bool tbg = d->jsObject->isTransparentBackground();
    QWidget *w = this;
    while (w) {
        w->setAttribute(Qt::WA_TranslucentBackground, tbg);
        w = w->parentWidget();
    }*/
}

void ChatView::setEncryptionEnabled(bool enabled) { d->isEncryptionEnabled_ = enabled; }

void ChatView::markReceived(QString id)
{
    QVariantMap m;
    m["type"]      = "receipt";
    m["id"]        = id;
    m["encrypted"] = d->isEncryptionEnabled_;
    d->sendJsObject(m);
}

QSize ChatView::sizeHint() const { return minimumSizeHint(); }

void ChatView::setDialog(QWidget *dialog) { d->dialog_ = dialog; }

void ChatView::setSessionData(bool isMuc, bool isMucPrivate, const Jid &jid, const QString name)
{
    d->isMuc_        = isMuc;
    d->isMucPrivate_ = isMucPrivate;
    d->jid_          = jid;
    d->name_         = name;
}

void ChatView::setAccount(PsiAccount *acc)
{
    d->account_ = acc;
    d->remoteIcons
        = acc->avatarFactory()->userHashes((d->isMuc_ || d->isMucPrivate_) ? d->jid_ : d->jid_.withResource(QString()));
    d->localIcons = acc->avatarFactory()->userHashes(acc->jid());

    d->themeProvider = static_cast<ChatViewThemeProvider *>(
        acc->psi()->themeManager()->provider(d->isMuc_ ? "groupchatview" : "chatview"));
    connect(d->themeProvider, SIGNAL(themeChanged()), SLOT(init()));
}

void ChatView::setLocalNickname(const QString &nickname) { d->localNickName = nickname; }

void ChatView::contextMenuEvent(QContextMenuEvent *e)
{
    QUrl linkUrl;
#ifdef WEBENGINE
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    linkUrl = d->webView->lastContextMenuRequest()->linkUrl();
#else
    QWebEngineContextMenuData cmd = d->webView->page()->contextMenuData();
    linkUrl                       = cmd.linkUrl();
#endif
#else
    linkUrl = d->webView->page()->mainFrame()->hitTestContent(e->pos()).linkUrl();
#endif
    if (linkUrl.scheme() == "addnick") {
        emit showNickMenu(linkUrl.path().mid(1));
        e->accept();
    }
}

bool ChatView::focusNextPrevChild(bool next) { return QWidget::focusNextPrevChild(next); }

void ChatView::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::FontChange) {
        QVariantMap m;
        m["type"] = "settings";
        d->sendJsObject(m);
    }
    QFrame::changeEvent(event);
}

void ChatView::psiOptionChanged(const QString &option)
{
    if (option == "options.ui.automatically-copy-selected-text") {
        if (PsiOptions::instance()->getOption("options.ui.automatically-copy-selected-text").toBool()) {
            connect(d->webView->page(), SIGNAL(selectionChanged()), d->webView, SLOT(copySelected()));
        } else {
            disconnect(d->webView->page(), SIGNAL(selectionChanged()), d->webView, SLOT(copySelected()));
        }
    }
}

void ChatView::sessionInited()
{
    qDebug("Session is initialized");
    d->sessionReady_ = true;
    d->checkJsBuffer();
}

bool ChatView::handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit)
{
    if (object == chatEdit && event->type() == QEvent::ShortcutOverride
        && static_cast<QKeyEvent *>(event)->matches(QKeySequence::Copy)) {

        if (!chatEdit->textCursor().hasSelection() && !(d->webView->page()->selectedText().isEmpty())) {
            d->webView->copySelected();
            return true;
        }
    }

    return false;
}

// input point of all messages
void ChatView::dispatchMessage(const MessageView &mv)
{
    static QHash<MessageView::Type, QString> types;
    if (types.isEmpty()) {
        types.insert(MessageView::Message, "message");
        types.insert(MessageView::System, "system");
        types.insert(MessageView::Status, "status");
        types.insert(MessageView::Subject, "subject");
        types.insert(MessageView::Urls, "urls");
        types.insert(MessageView::MUCJoin, "join");
        types.insert(MessageView::MUCPart, "part");
        types.insert(MessageView::FileTransferRequest, "ftreq");
        types.insert(MessageView::FileTransferFinished, "ftfin");
        types.insert(MessageView::NickChange, "newnick");
        types.insert(MessageView::Reactions, "reactions");
        types.insert(MessageView::MessageRetraction, "msgretract");
    }
    QVariantMap m;
    switch (mv.type()) {
    case MessageView::Message:
        m["message"] = d->prepareShares(ChatViewPrivate::closeIconTags(mv.formattedText()));
        m["emote"]   = mv.isEmote();
        m["local"]   = mv.isLocal();
        m["sender"]  = mv.nick();
        m["userid"]  = mv.userId();
        m["spooled"] = mv.isSpooled();
        m["id"]      = mv.messageId();
        if (d->isMuc_) { // maybe w/o conditions ?
            m["alert"] = mv.isAlert();
        } else {
            m["awaitingReceipt"] = mv.isAwaitingReceipt();
        }
        if (mv.references().count()) {
            QVariantMap rvm;
            for (auto const &r : mv.references()) {
                auto md = r->metaData();
                md.insert("type", r->mimeType());
                rvm.insert(r->sums()[0].toString(), md);
            }
            m["references"] = rvm;
        }
        break;
    case MessageView::NickChange:
        m["sender"]  = mv.nick();
        m["newnick"] = mv.userText();
        m["message"] = ChatViewPrivate::closeIconTags(mv.text());
        break;
    case MessageView::MUCJoin: {
        Jid j          = d->jid_.withResource(mv.nick());
        m["avatar"]    = ChatViewJSObject::avatarUrl(d->account_->avatarFactory()->userHashes(j).avatar);
        m["nickcolor"] = getMucNickColor(mv.nick(), mv.isLocal());
    }
    case MessageView::MUCPart:
        m["nopartjoin"] = mv.isJoinLeaveHidden();
        PSI_FALLSTHROUGH; // falls through
    case MessageView::Status:
        m["sender"]   = mv.nick();
        m["status"]   = mv.status();
        m["priority"] = mv.statusPriority();
        m["message"]  = ChatViewPrivate::closeIconTags(mv.text());
        m["usertext"] = ChatViewPrivate::closeIconTags(mv.formattedUserText());
        m["nostatus"] = mv.isStatusChangeHidden(); // looks strange? but chatview can use status for something anyway
        break;
    case MessageView::System:
    case MessageView::Subject:
        m["message"]  = ChatViewPrivate::closeIconTags(mv.formattedText());
        m["usertext"] = ChatViewPrivate::closeIconTags(mv.formattedUserText());
        break;
    case MessageView::Urls: {
        QVariantMap vmUrls;
        for (auto it = mv.urls().constBegin(); it != mv.urls().constEnd(); ++it) {
            vmUrls.insert(it.key(), it.value());
        }
        m["urls"] = vmUrls;
        break;
    }
    case MessageView::Reactions: {
        auto n = d->isMuc_ ? mv.nick() : QString::fromLatin1(mv.isLocal() ? "l" : "r");
        d->sendReactionsToUI(n, mv.reactionsId(), mv.reactions());
        return;
    }
    case MessageView::MessageRetraction:
        m["targetid"] = mv.retractionId();
        break;
    case MessageView::FileTransferRequest:
    case MessageView::FileTransferFinished:
        break;
    }

    m["time"]         = mv.dateTime();
    m["type"]         = types.value(mv.type());
    QString replaceId = mv.replaceId();
    if (replaceId.isEmpty() && (mv.type() == MessageView::Message || mv.type() == MessageView::Subject)
        && updateLastMsgTime(mv.dateTime())) {
        QVariantMap m;
        m["date"]  = mv.dateTime();
        m["type"]  = "message";
        m["mtype"] = "lastDate";
        d->sendJsObject(m);
    }

    m["encrypted"] = d->isEncryptionEnabled_;
    if (!replaceId.isEmpty()) {
        m["type"]      = "replace";
        m["replaceId"] = replaceId;
    } else if (mv.type() != MessageView::MessageRetraction) {
        m["mtype"] = m["type"];
        m["type"]  = "message";
    }
    d->sendJsObject(m);
}

void ChatView::sendJsCode(const QString &js)
{
    QVariantMap m;
    m["type"] = "js";
    m["js"]   = js;
    d->sendJsObject(m);
}

void ChatView::scrollUp() { emit d->jsObject->scrollRequested(-50); }

void ChatView::scrollDown() { emit d->jsObject->scrollRequested(50); }

void ChatView::updateAvatar(const Jid &jid, UserType utype)
{
    bool avatarChanged = false;
    bool vcardChanged  = false;

    if (utype == RemoteParty) { // remote party but not muc participant
        auto h         = d->account_->avatarFactory()->userHashes(jid);
        avatarChanged  = h.avatar != d->remoteIcons.avatar;
        vcardChanged   = h.vcard != d->remoteIcons.vcard;
        d->remoteIcons = h;
        if (avatarChanged) {
            d->jsObject->setRemoteUserAvatarHash(h.avatar);
        }
        if (vcardChanged) {
            d->jsObject->setRemoteUserAvatarHash(h.avatar);
        }
    } else if (utype == LocalParty) { // local party
        auto h        = d->account_->avatarFactory()->userHashes(jid);
        avatarChanged = (h.avatar != d->localIcons.avatar);
        vcardChanged  = (h.vcard != d->localIcons.vcard);
        d->localIcons = h;
        if (avatarChanged) {
            d->jsObject->setLocalUserAvatarHash(h.avatar);
        }
        if (vcardChanged) {
            d->jsObject->setLocalUserImageHash(h.avatar);
        }
    } else { // muc participant
        QVariantMap m;
        m["type"]   = "avatar";
        m["sender"] = jid.resource();
        m["avatar"] = ChatViewJSObject::avatarUrl(d->account_->avatarFactory()->userHashes(jid).avatar);
        d->sendJsObject(m);
    }
}

void ChatView::clear()
{
    QVariantMap m;
    m["type"] = "clear";
    d->sendJsObject(m);
}

void ChatView::doTrackBar()
{
    QVariantMap m;
    m["type"] = "trackbar";
    d->sendJsObject(m);
}

WebView *ChatView::textWidget() { return d->webView; }

QWidget *ChatView::realTextWidget() { return d->webView; }

QObject *ChatView::jsBridge() { return d->jsObject; }

void ChatView::outgoingReaction(const QString &messageId, const QString &reaction)
{
    auto n         = d->isMuc_ ? d->localNickName : QString::fromLatin1("l");
    auto reactions = onReactionSwitched(n, messageId, reaction);
    emit outgoingReactions(messageId, reactions);
    if (!d->isMuc_) {
        // with private message we are not going to get it back. so back immediately
        d->sendReactionsToUI(n, messageId, reactions);
    }
}

void ChatViewPrivate::checkJsBuffer()
{
    if (sessionReady_) {
        while (!jsBuffer_.isEmpty()) {
            emit jsObject->newMessage(jsBuffer_.takeFirst());
        }
    }
}

#include "chatview_webkit.moc"
