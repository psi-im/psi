/*
 * Copyright (C) 2007  Barracuda Networks
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

#ifndef CUDASKIN_H
#define CUDASKIN_H

#include <QtCore>
// workaround for qt/mac 4.3
#ifdef check
# undef check
#endif
#include <QtGui>
#include "tabdlg.h"
#include "psitabwidget.h"

class CudaSkin;

class CudaStyle : public QWindowsStyle
{
	Q_OBJECT
public:
	CudaStyle();
	virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const;
	virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const;
	virtual QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const;
	virtual int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
	QPalette standardPalette() const;
};

class CudaContainer;

class CudaFrame : public QFrame
{
	Q_OBJECT
public:
	CudaContainer *container;
	QPoint dragPosition;
	bool resizing;

	CudaFrame(QWidget *parent = 0);
	~CudaFrame();

	void setup();
	virtual void paintEvent(QPaintEvent *);
	virtual void resizeEvent(QResizeEvent *);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);

	QImage makeAlpha(const QImage &in, int percent);

protected:
	virtual bool event(QEvent *event);

private:
	class Private;
	Private *d;
};

class CudaTabDlgDelegate : public TabDlgDelegate
{
	Q_OBJECT
public:
	CudaTabDlgDelegate();
	~CudaTabDlgDelegate();

	virtual Qt::WindowFlags initWindowFlags() const;
	virtual void create(QWidget *widget);
	virtual void destroy(QWidget *widget);
	virtual void tabWidgetCreated(QWidget *widget, PsiTabWidget *tabWidget);
	virtual bool paintEvent(QWidget *widget, QPaintEvent *);
	virtual bool resizeEvent(QWidget *widget, QResizeEvent *);
	virtual bool mousePressEvent(QWidget *widget, QMouseEvent *event);
	virtual bool mouseMoveEvent(QWidget *widget, QMouseEvent *event);
	virtual bool mouseReleaseEvent(QWidget *widget, QMouseEvent *event);
	virtual bool event(QWidget *widget, QEvent *event);

private:
	class Private;
	Private *d;
};

class CudaSubFrame : public QFrame
{
	Q_OBJECT
public:
	CudaContainer *container;

	CudaSubFrame(QWidget *parent = 0);
	void setup();
	virtual void paintEvent(QPaintEvent *);
};

class CudaMenuBar : public QMenuBar
{
	Q_OBJECT
public:
	CudaMenuBar(QWidget *parent = 0);
	virtual void paintEvent(QPaintEvent *pe);
};

class CudaLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	CudaLineEdit(QWidget *parent = 0);
};

bool cuda_loadTheme(const QString &path);
bool cuda_isThemed();
void cuda_unloadTheme();

QImage cuda_defaultAvatar();
void cuda_applyTheme(QWidget *w);
void cuda_applyThemeAll(QWidget *w);

QPixmap get_icon_im();
QPixmap get_icon_file();
QImage get_button_up();
QColor get_frameCol1();
QColor get_windowText();
QColor get_selectBg();
QColor get_contactOnline();
QColor get_expansionLine();
QColor get_expansionText();
CudaTabDlgDelegate *get_tab_delegate();

#endif
