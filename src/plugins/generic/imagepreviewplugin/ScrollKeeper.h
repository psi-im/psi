/*
 * ScrollKeeper.h
 *
 *  Created on: 30 Oct 2016
 *      Author: rkfg
 */

#ifndef SRC_PLUGINS_GENERIC_IMAGEPREVIEWPLUGIN_SCROLLKEEPER_H_
#define SRC_PLUGINS_GENERIC_IMAGEPREVIEWPLUGIN_SCROLLKEEPER_H_

#include <QWidget>
#include <QTextEdit>
#include <QWebFrame>

class ScrollKeeper {
private:
	QWidget* chatView_;
	int scrollPos_;
	bool scrollToEnd_;
	QTextEdit* ted_;
	QWebFrame* mainFrame_;
public:
	ScrollKeeper(QWidget* chatView);
	virtual ~ScrollKeeper();
};

#endif /* SRC_PLUGINS_GENERIC_IMAGEPREVIEWPLUGIN_SCROLLKEEPER_H_ */
