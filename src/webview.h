/*
 * webview.h - QWebView handling links and copying text
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _WEBVIEW_H
#define	_WEBVIEW_H

#ifdef WEBENGINE
#include <QWebEngineView>
#else
#include <QWebView>
#endif
#include <QMessageBox>
#include <QMenu>
#include <QContextMenuEvent>
#include <QClipboard>
#include <QBuffer>
#ifdef HAVE_QT5
#include <QUrlQuery>
#endif

#include "networkaccessmanager.h"
#include "iconset.h"

/**
 * Extended QWebView.
 *
 * It's used in EventView and HTMLChatView.
 * Provides evaluateJavaScript escaping and secure NetworkManager with icon:// URL
 * support and \<img\> whitelisting.
 *
 * Better name for it would be: PsiWebView, but it's used in HTMLChatView which is
 * Psi-unaware.
 */
#ifdef WEBENGINE
class WebView : public QWebEngineView {
#else
class WebView : public QWebView {
#endif
    Q_OBJECT
public:

	WebView(QWidget* parent);

	/** Evaluates JavaScript code */
	void evaluateJS(const QString &scriptSource = "");

#ifndef WEBENGINE
	QString selectedText();
#endif
	bool isLoading() { return isLoading_; }

public slots:
	void copySelected();

protected:
    /** Creates menu with Copy actions */
	void contextMenuEvent(QContextMenuEvent* event);
#ifndef WEBENGINE
	void mousePressEvent ( QMouseEvent * event );
	void mouseReleaseEvent ( QMouseEvent * event );
	void mouseMoveEvent(QMouseEvent *event);
#endif
	//QAction* copyAction, *copyLinkAction;

private:
#ifndef WEBENGINE
	void convertClipboardHtmlImages(QClipboard::Mode);
#endif
	bool possibleDragging;
	bool isLoading_;
	QStringList jsBuffer_;
	QPoint dragStartPosition;


protected slots:
	void linkClickedEvent(const QUrl& url);
	void textCopiedEvent();
	void loadStartedEvent();
	void loadFinishedEvent(bool);
};


#endif

