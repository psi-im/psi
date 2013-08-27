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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _WEBVIEW_H
#define	_WEBVIEW_H

#include <QWebView>
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

class IconHandler : public NAMSchemeHandler
{
	QByteArray data(const QUrl &url) const {
#ifdef HAVE_QT5
		int w = QUrlQuery(url.query()).queryItemValue("w").toInt();
		int h = QUrlQuery(url.query()).queryItemValue("h").toInt();
#else
		int w = url.queryItemValue("w").toInt();
		int h = url.queryItemValue("h").toInt();
#endif
		PsiIcon icon = IconsetFactory::icon(url.path());
		if (w && h && !icon.isAnimated()) {
			QByteArray ba;
			QBuffer buffer(&ba);
			buffer.open(QIODevice::WriteOnly);
			icon.pixmap().scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation)
					.toImage().save(&buffer, "PNG");
			return ba;
		} else { //scaling impossible, return as is. do scaling with help of css or html attributes
			return IconsetFactory::raw(url.path());
		}
	}
};

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
class WebView : public QWebView {

    Q_OBJECT
public:

	WebView(QWidget* parent);

	/** Evaluates JavaScript code */
	void evaluateJS(const QString &scriptSource = "");

	QString selectedText();
	bool isLoading() { return isLoading_; }

public slots:
	void copySelected();

protected:
    /** Creates menu with Copy actions */
	void contextMenuEvent(QContextMenuEvent* event);
	void mousePressEvent ( QMouseEvent * event );
	void mouseReleaseEvent ( QMouseEvent * event );
	void mouseMoveEvent(QMouseEvent *event);

	//QAction* copyAction, *copyLinkAction;

private:
	void convertClipboardHtmlImages(QClipboard::Mode);

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

