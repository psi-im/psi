/*
 * ScrollKeeper.cpp
 *
 *  Created on: 30 Oct 2016
 *      Author: rkfg
 */

#include "ScrollKeeper.h"
#include <QWebView>
#include <QScrollBar>

//#define SCROLL_DEBUG

ScrollKeeper::ScrollKeeper(QWidget* chatView) :
		chatView_(chatView), scrollPos_(0), scrollToEnd_(false), ted_(0), mainFrame_(0) {
	ted_ = qobject_cast<QTextEdit*>(chatView);
	if (ted_) {
		scrollPos_ = ted_->verticalScrollBar()->value();
		if (scrollPos_ == ted_->verticalScrollBar()->maximum()) {
			scrollToEnd_ = true;
		}
#ifdef SCROLL_DEBUG
		qDebug() << "QTED Scroll pos:" << scrollPos_ << "to end:" << scrollToEnd_ << "max:"
				<< ted_->verticalScrollBar()->maximum();
#endif
	} else {
		QWebView* wv = qobject_cast<QWebView*>(chatView);
		if (!wv) {
			return;
		}
		mainFrame_ = wv->page()->mainFrame();
		scrollPos_ = mainFrame_->scrollBarValue(Qt::Vertical);
		if (scrollPos_ == mainFrame_->scrollBarMaximum(Qt::Vertical)) {
			scrollToEnd_ = true;
		}
#ifdef SCROLL_DEBUG
		qDebug() << "QWV Scroll pos:" << scrollPos_ << "to end:" << scrollToEnd_ << "max:"
				<< mainFrame_->scrollBarMaximum(Qt::Vertical) << "min:" << mainFrame_->scrollBarMinimum(Qt::Vertical);
#endif
	}
}

ScrollKeeper::~ScrollKeeper() {
	if (ted_) {
#ifdef SCROLL_DEBUG
		qDebug() << "QTED restoring scroll pos:" << scrollPos_ << "to end:" << scrollToEnd_ << "max:"
				<< ted_->verticalScrollBar()->maximum();
#endif
		ted_->verticalScrollBar()->setValue(scrollToEnd_ ? ted_->verticalScrollBar()->maximum() : scrollPos_);
	}
	if (mainFrame_) {
#ifdef SCROLL_DEBUG
		qDebug() << "QWV restoring scroll pos:" << scrollPos_ << "to end:" << scrollToEnd_ << "max:"
				<< mainFrame_->scrollBarMaximum(Qt::Vertical);
#endif
		mainFrame_->setScrollBarValue(Qt::Vertical,	scrollToEnd_ ? mainFrame_->scrollBarMaximum(Qt::Vertical) : scrollPos_);
	}
}

