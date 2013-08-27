/*
 * webview.cpp - QWebView handling links and copying text
 * Copyright (C) 2010 senu, Rion
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QWebFrame>
#include <QFile>
#include <QMimeData>
#include <QApplication>
#include <QStyle>
#include <QDebug>
#include <QDrag>

#include "webview.h"
#include "urlobject.h"
#include "textutil.h"

WebView::WebView(QWidget* parent) : QWebView(parent), possibleDragging(false), isLoading_(false)
{

    settings()->setAttribute(QWebSettings::JavaEnabled, false);
    settings()->setAttribute(QWebSettings::PluginsEnabled, false);
	setAcceptDrops(false);

	page()->setNetworkAccessManager(NetworkAccessManager::instance());
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

	connect(page(), SIGNAL(linkClicked(const QUrl&)), this, SLOT(linkClickedEvent(const QUrl&)));
	connect(page()->action(QWebPage::Copy), SIGNAL(triggered()), SLOT(textCopiedEvent()));
	connect(page()->action(QWebPage::Cut), SIGNAL(triggered()), SLOT(textCopiedEvent()));
	connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStartedEvent()));
	connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinishedEvent(bool)));
}

void WebView::linkClickedEvent(const QUrl& url)
{
	//qDebug()<<"clicked link: "<<url.toString();
	URLObject::getInstance()->popupAction(url.toEncoded());
}

void WebView::loadStartedEvent()
{
	//qDebug("page load started");
	isLoading_ = true;
}

void WebView::loadFinishedEvent(bool success)
{
	//qDebug("page load finished");
	if (!success) {
		qDebug("webview page load failed");
	}
	isLoading_ = false;
}

void WebView::contextMenuEvent(QContextMenuEvent* event)
{
	if (isLoading_) return;
	QWebHitTestResult r = page()->mainFrame()->hitTestContent(event->pos());
	QMenu *menu;

	if (!r.linkUrl().isEmpty()) {
		menu = URLObject::getInstance()->createPopupMenu(r.linkUrl().toEncoded());
		//menu->addAction(pageAction(QWebPage::CopyLinkToClipboard));
	} else {
		menu = new QMenu(this);
		if (!page()->selectedText().isEmpty()) {
			menu->addAction(pageAction(QWebPage::Copy));
		} else {
			if (!menu->isEmpty()) {
				menu->addSeparator();
			}
#if QT_VERSION >= 0x040500
			menu->addAction(pageAction(QWebPage::SelectAll));
#endif
		}
		if (settings()->testAttribute(QWebSettings::DeveloperExtrasEnabled)) {
			menu->addAction(pageAction(QWebPage::InspectElement));
		}
	}
	menu->exec(mapToGlobal(event->pos()));
	event->accept();
	delete menu;
}

void WebView::mousePressEvent ( QMouseEvent * event )
{
	if (isLoading_) return;
	QWebView::mousePressEvent(event);
	if (event->buttons() & Qt::LeftButton) {
		QWebHitTestResult r = page()->mainFrame()->hitTestContent(event->pos());
		QSize cs = page()->mainFrame()->contentsSize();
		QSize vs = page()->viewportSize();
		possibleDragging = r.isContentSelected() &&
			QRect(QPoint(0,0),
				  cs - QSize(cs.width()>vs.width()?1:0, cs.height()>vs.height()?1:0) *
					style()->pixelMetric(QStyle::PM_ScrollBarExtent)
				 ).contains(event->pos());
		dragStartPosition = event->pos();
	} else {
		possibleDragging = false;
	}
}

void WebView::mouseReleaseEvent ( QMouseEvent * event )
{
	QWebView::mouseReleaseEvent(event);
	possibleDragging = false;
#ifdef HAVE_X11
	if (!page()->selectedHtml().isEmpty()) {
		convertClipboardHtmlImages(QClipboard::Selection);
	}
#endif
}

void WebView::mouseMoveEvent(QMouseEvent *event)
 {
	//QWebView::mouseMoveEvent(event);
	if (!possibleDragging || !(event->buttons() & Qt::LeftButton)) {
		QWebView::mouseMoveEvent(event);
		return;
	}
	if ((event->pos() - dragStartPosition).manhattanLength()
		< QApplication::startDragDistance())
		return;

	QDrag *drag = new QDrag(this);
	QMimeData *mimeData = new QMimeData;

	QString html = TextUtil::img2title(selectedHtml());
	mimeData->setHtml(html);
	mimeData->setText(TextUtil::rich2plain(html));

	drag->setMimeData(mimeData);
	drag->exec(Qt::CopyAction);
}

void WebView::convertClipboardHtmlImages(QClipboard::Mode mode)
{
	QClipboard *cb = QApplication::clipboard();
	QString html = TextUtil::img2title(selectedHtml());
	QMimeData *data = new QMimeData;
	data->setHtml(html);
	data->setText(TextUtil::rich2plain(html, false));
	cb->setMimeData(data, mode);
}

void WebView::evaluateJS(const QString &scriptSource)
{
	//qDebug()<< "EVALUATE: " << (scriptSource.size()>200?scriptSource.mid(0,200)+"...":scriptSource);
	page()->mainFrame()->evaluateJavaScript(scriptSource);
}

QString WebView::selectedText()
{
	return TextUtil::rich2plain(TextUtil::img2title(selectedHtml()));
}

void WebView::copySelected()
{
	// use native selectedText w/o clipboard hacks.
	// ideally we should call something like hasSelection() but there is no such method in Qt API for webkit classes.
	if (!page()->selectedText().isEmpty()) {
		page()->triggerAction(QWebPage::Copy);
		textCopiedEvent();
	}
	//qDebug("copied text: %s", qPrintable(QApplication::clipboard()->text(QClipboard::Clipboard)));
}

void WebView::textCopiedEvent()
{
	convertClipboardHtmlImages(QClipboard::Clipboard);
}
