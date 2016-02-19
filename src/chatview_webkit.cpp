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
#include <QWebFrame>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QPalette>
#include <QDesktopWidget>
#include <QApplication>

#include "webview.h"
//#include "psiapplication.h"
#include "psiaccount.h"
#include "applicationinfo.h"
#include "networkaccessmanager.h"
#include "jsutil.h"
#include "messageview.h"
#include "psithememanager.h"
#include "chatviewtheme.h"



//----------------------------------------------------------------------------
// ChatViewJSObject
// object which will be embed to javascript part of view
//----------------------------------------------------------------------------
class ChatViewJSObject : public QObject
{
	Q_OBJECT

	ChatView *_view;

public:
	ChatViewJSObject(ChatView *view) :
		QObject(view),
		_view(view)
	{

	}

public slots:
	QString mucNickColor(QString nick, bool isSelf,
						 QStringList validList = QStringList()) const
	{
		return _view->getMucNickColor(nick, isSelf, validList);
	}

	bool isMuc() const
	{
		return _view->isMuc_;
	}

	QString chatName() const
	{
		return _view->name_;
	}

	QString jid() const
	{
		return _view->jid_;
	}

	QString account() const
	{
		return _view->account_->id();
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

		// In typography 1 point (also called PostScript point)
		// is 1/72 of an inch
		const float postScriptPoint = 1 / 72.;

		// Workaround.  WebKit works only with 96dpi
		// Need to convert point size to pixel size
		int pixelSize = qRound(f.pointSize() * qApp->desktop()->logicalDpiX() * postScriptPoint);

		return QString("{fontFamily:'%1',fontSize:'%2px',fontStyle:'%3',fontVariant:'%4',fontWeight:'%5'}")
						 .arg(f.family())
						 .arg(pixelSize)
						 .arg(f.style()==QFont::StyleNormal?"normal":(f.style()==QFont::StyleItalic?"italic":"oblique"))
						 .arg(f.capitalization() == QFont::SmallCaps?"small-caps":"normal")
						 .arg(weight);
	}

	void setTransparent()
	{
		QPalette palette = _view->webView->palette();
		palette.setBrush(QPalette::Base, Qt::transparent);
		_view->webView->page()->setPalette(palette);
		_view->webView->setAttribute(Qt::WA_OpaquePaintEvent, false);

		palette = _view->palette();
		palette.setBrush(QPalette::Base, Qt::transparent);
		_view->setPalette(palette);
		_view->setAttribute(Qt::WA_TranslucentBackground, true);
	}

signals:
	void inited();
};


//----------------------------------------------------------------------------
// ChatView
//----------------------------------------------------------------------------
ChatView::ChatView(QWidget *parent)
	: QFrame(parent)
	, sessionReady_(false)
	, dialog_(0)
	, isMuc_(false)
{
	jsObject = new ChatViewJSObject(this);
	webView = new WebView(this);
	webView->setFocusPolicy(Qt::NoFocus);
	webView->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0,0,0,0);
	layout->addWidget(webView);
	setLayout(layout);
	setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	setLooks(webView);

#ifndef HAVE_X11	// linux has this feature built-in
	connect( PsiOptions::instance(), SIGNAL(optionChanged(QString)), SLOT(psiOptionChanged(QString)) ); //needed only for save autocopy state atm
	psiOptionChanged("options.ui.automatically-copy-selected-text"); // init autocopy connection
#endif
	connect(jsObject, SIGNAL(inited()), SLOT(sessionInited()));
	connect(webView->page()->mainFrame(),
			SIGNAL(javaScriptWindowObjectCleared()), SLOT(embedJsObject()));
}

ChatView::~ChatView()
{
}

ChatViewTheme* ChatView::currentTheme()
{
	return (ChatViewTheme *)PsiThemeManager::instance()->
			provider(isMuc_?"groupchatview":"chatview")->current();
}

// something after we know isMuc and dialog is set
void ChatView::init()
{
	ChatViewTheme *theme = currentTheme();
	QString html = theme->html(jsObject);
	//qDebug() << "Set html:" << html;
	webView->page()->mainFrame()->setHtml(
		html, theme->baseUrl()
	);
}

void ChatView::embedJsObject()
{
	ChatViewTheme *theme = currentTheme();
	QWebFrame *wf = webView->page()->mainFrame();
	wf->addToJavaScriptWindowObject("chatServer", theme->jsHelper());
	wf->addToJavaScriptWindowObject("chatSession", jsObject);
	foreach (const QString &script, theme->scripts()) {
		wf->evaluateJavaScript(script);
	}
}

void ChatView::markReceived(QString id)
{
	QVariantMap m;
	m["type"] = "receipt";
	m["id"] = id;
	sendJsObject(m);
}

QSize ChatView::sizeHint() const
{
	return minimumSizeHint();
}

void ChatView::setDialog(QWidget* dialog)
{
	dialog_ = dialog;
}

void ChatView::setSessionData(bool isMuc, const QString &jid, const QString name)
{
	isMuc_ = isMuc;
	jid_ = jid;
	name_ = name;
	connect(PsiThemeManager::instance()->provider(isMuc_?"groupchatview":"chatview"),
			SIGNAL(themeChanged()),
			SLOT(init()));
}

void ChatView::contextMenuEvent(QContextMenuEvent *e)
{
	QWebHitTestResult r = webView->page()->mainFrame()->hitTestContent(e->pos());
	if ( r.linkUrl().scheme() == "addnick" ) {
		showNM(r.linkUrl().path().mid(1));
		e->accept();
	}
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
			connect(webView->page(), SIGNAL(selectionChanged()), webView, SLOT(copySelected()));
		} else {
			disconnect(webView->page(), SIGNAL(selectionChanged()), webView, SLOT(copySelected()));
		}
	}
}

void ChatView::sendJsObject(const QVariantMap &map)
{
	sendJsCommand(QString(currentTheme()->jsNamespace() + ".adapter.receiveObject(%1);")
				  .arg(JSUtil::map2json(map)));
}

void ChatView::sendJsCommand(const QString &cmd)
{
	jsBuffer_.append(cmd);
	checkJsBuffer();
}

void ChatView::checkJsBuffer()
{
	if (sessionReady_) {
		while (!jsBuffer_.isEmpty()) {
			webView->evaluateJS(jsBuffer_.takeFirst());
		}
	}
}

void ChatView::sessionInited()
{
	sessionReady_ = true;
	checkJsBuffer();
}

bool ChatView::handleCopyEvent(QObject *object, QEvent *event, ChatEdit *chatEdit) {
	if (object == chatEdit && event->type() == QEvent::ShortcutOverride &&
		((QKeyEvent*)event)->matches(QKeySequence::Copy)) {

		if (!chatEdit->textCursor().hasSelection() &&
			 !(webView->page()->selectedText().isEmpty()))
		{
			webView->copySelected();
			return true;
		}
	}

	return false;
}

// input point of all messages
void ChatView::dispatchMessage(const MessageView &mv)
{
	if ((mv.type() == MessageView::Message || mv.type() == MessageView::Subject)
			&& updateLastMsgTime(mv.dateTime()))
	{
		QVariantMap m;
		m["date"] = mv.dateTime();
		m["type"] = "message";
		m["mtype"] = "lastDate";
		sendJsObject(m);
	}
	QVariantMap vm = mv.toVariantMap(isMuc_, true);
	vm["mtype"] = vm["type"];
	vm["type"] = "message";
	sendJsObject(vm);
}

void ChatView::scrollUp()
{
	QWebFrame *f = webView->page()->mainFrame();
	f->setScrollBarValue(Qt::Vertical, f->scrollBarValue(Qt::Vertical) - 50);
}

void ChatView::scrollDown()
{
	QWebFrame *f = webView->page()->mainFrame();
	f->setScrollBarValue(Qt::Vertical, f->scrollBarValue(Qt::Vertical) + 50);
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
	bool found = webView->page()->findText(str, startFromBeginning ?
				 QWebPage::FindWrapsAroundDocument : (QWebPage::FindFlag)0);

	if (!found && !startFromBeginning) {
		return internalFind(str, true);
	}

	return found;
}

WebView* ChatView::textWidget()
{
	return webView;
}

#include "chatview_webkit.moc"
