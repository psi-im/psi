/*
 * fancypopup.cpp - the FancyPopup passive popup widget
 * Copyright (C) 2003-2005  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "fancypopup.h"
#include "ui_fancypopup.h"

#include <QApplication>
#include <QTimer>
#include <QList>
#include <QStyle>
#include <QDesktopWidget>
#include <QMouseEvent>
#include <QList>

#include "iconset.h"
#include "fancylabel.h"
//#include "iconlabel.h"
#include "psitooltip.h"
#include "psioptions.h"

#define BUTTON_WIDTH	16
#define BUTTON_HEIGHT	14

/*static int checkComponent(int b)
{
	int c = b;
	if ( c > 0xFF )
		c = 0xFF;
	return c;
}

static QColor makeColor(QColor baseColor, int percent)
{
	float p = (float)percent/100;
	int r = checkComponent( (int)(baseColor.red()   + ((float)p * baseColor.red())) );
	int g = checkComponent( (int)(baseColor.green() + ((float)p * baseColor.green())) );
	int b = checkComponent( (int)(baseColor.blue()  + ((float)p * baseColor.blue())) );

	return QColor(r, g, b);
}*/

//----------------------------------------------------------------------------
// FancyPopup::Private
//----------------------------------------------------------------------------

class FancyPopup::Private : public QObject
{
	Q_OBJECT
public:
	Private(FancyPopup *p);
	~Private();

	QPoint position();

	/*class BackgroundWidget : public QWidget
	{
	public:
		BackgroundWidget(QWidget *parent)
			: QWidget(parent)
		{
		}

		void setBackground(QPixmap pix)
		{
			background = pix;
		}

	private:
		QPixmap background;

	protected:
		void paintEvent(QPaintEvent *)
		{
			QPainter *p = new QPainter(this);
			p->drawTiledPixmap(0, 0, width(), height(), background);
			delete p;
		}
	};*/

	bool eventFilter(QObject *o, QEvent *e);

public slots:
	void popupDestroyed(QObject *);

public:
	void initContents(QString title, const PsiIcon *icon, bool copyIcon);

	// parameters
	static int hideTimeout;
	static QColor backgroundColor;

	enum PopupLayout {
		TopToBottom = 1,
		BottomToTop = -1
	};
	PopupLayout popupLayout;

	QList<FancyPopup *> prevPopups;
	//QBoxLayout *layout;
	FancyPopup *popup;
	QTimer *hideTimer;
	Ui::Frame ui_;
};

int  FancyPopup::Private::hideTimeout = 5 * 1000; // 5 seconds
QColor FancyPopup::Private::backgroundColor = QColor (0x52, 0x97, 0xF9);

FancyPopup::Private::Private(FancyPopup *p)
: QObject(p)
{
	popup = p;

	hideTimer = new QTimer(this);
	connect(hideTimer, SIGNAL(timeout()), popup, SLOT(hide()));
}

FancyPopup::Private::~Private()
{
}

void FancyPopup::Private::popupDestroyed(QObject *obj)
{
	if ( prevPopups.contains((FancyPopup *)obj) ) {
		prevPopups.removeAll((FancyPopup *)obj);
		popup->move( position() );
	}
}

QPoint FancyPopup::Private::position()
{
	QRect geom = qApp->desktop()->availableGeometry(popup);
	bool topToBottom = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.top-to-bottom").toBool();
	bool atLeft = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.at-left-corner").toBool();
	QPoint destination(geom.x() + (atLeft ? 0 : geom.width()),
			   geom.y() + (topToBottom ? 0 : geom.height()) ); // in which corner popup should appear

	/*if ( destination.y() > (qApp->desktop()->screenGeometry().height()/2) )
		popupLayout = Private::BottomToTop;
	else
		popupLayout = Private::TopToBottom;*/

	popupLayout = topToBottom ? Private::TopToBottom : Private::BottomToTop;

	if ( (destination.x() + popup->width()) > (geom.x() + geom.width()) )
		destination.setX( geom.x() + geom.width() - popup->width() );

	if ( destination.x() < 0 )
		destination.setX( 0 );

	if ( (destination.y() + popup->height()) > (geom.y() + geom.height()) )
		destination.setY( geom.y() + geom.height() - popup->height() );

	if ( destination.y() < 0 )
		destination.setY( 0 );

	foreach ( FancyPopup *p, prevPopups )
		destination.setY( destination.y() + popupLayout * p->height() );

	return destination;
}

void FancyPopup::Private::initContents(QString title, const PsiIcon *icon, bool copyIcon)
{
	// TODO: use darker color on popup borders
	QPalette backgroundPalette;
	backgroundPalette.setBrush(QPalette::Background, QBrush(backgroundColor));
	QPixmap back(1, 1);
	back.fill(backgroundColor);

	ui_.setupUi(popup);
	ui_.lb_bottom1->setPalette(backgroundPalette);
	ui_.lb_icon->setPalette(backgroundPalette);
	ui_.lb_mid1->setPalette(backgroundPalette);
	ui_.lb_mid2->setPalette(backgroundPalette);
	ui_.lb_top1->setPalette(backgroundPalette);
	ui_.closeFrame->setPalette(backgroundPalette);

	QBrush titleFontColor;
	if ( (backgroundColor.red() + backgroundColor.green() + backgroundColor.blue())/3 > 128 )
		titleFontColor = QBrush(Qt::white);
	else
		titleFontColor = QBrush(Qt::black);
	QPalette titlePalette = backgroundPalette;
	titlePalette.setBrush(QPalette::Text, titleFontColor);
	ui_.lb_title->setPalette(titlePalette);
	QFont titleFont = ui_.lb_title->font();
	titleFont.setBold(true);
	ui_.lb_title->setFont(titleFont);
	ui_.lb_title->setText( title );

	ui_.lb_icon->setPsiIcon(icon, copyIcon);

	ui_.closeButton->setObjectName("closeButton");
	ui_.closeButton->setToolTip(tr("Close"));
	ui_.closeButton->setFocusPolicy( Qt::NoFocus );
	ui_.closeButton->setIcon( popup->style()->standardPixmap(QStyle::SP_TitleBarCloseButton) );
	ui_.closeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	connect(ui_.closeButton, SIGNAL(clicked()), popup, SLOT(hide()));

	const QString css = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.css").toString();
	if (!css.isEmpty()) {
		popup->setStyleSheet(css);
	}
}

bool FancyPopup::Private::eventFilter(QObject *o, QEvent *e)
{
	if (e->type() == QEvent::MouseButtonRelease)
		popup->mouseReleaseEvent((QMouseEvent *)e);
	return QObject::eventFilter(o, e);
}

//----------------------------------------------------------------------------
// FancyPopup
//----------------------------------------------------------------------------

static const QFlags<Qt::WindowType>
POPUP_FLAGS = Qt::ToolTip | Qt::WindowStaysOnTopHint; // | Qt::X11BypassWindowManagerHint | Qt::FramelessWindowHint;

FancyPopup::FancyPopup(QString title, const PsiIcon *icon, FancyPopup *prev, bool copyIcon)
: QFrame( 0, POPUP_FLAGS )
{
	QWidget::setAttribute(Qt::WA_DeleteOnClose);
	setWindowModality(Qt::NonModal);
	d = new Private(this);

	if ( prev ) {
		QList<FancyPopup *> prevPopups = prev->d->prevPopups;
		prevPopups.append(prev);

		foreach (FancyPopup *popup, prevPopups) {
			d->prevPopups.append( popup );
			connect(popup, SIGNAL(destroyed(QObject *)), d, SLOT(popupDestroyed(QObject *)));
		}
	}

	d->initContents(title, icon, copyIcon);
}

FancyPopup::~FancyPopup()
{
}

void FancyPopup::addLayout(QLayout *layout, int stretch)
{
	d->ui_.layout->addLayout(layout, stretch);
	d->ui_.layout->addSpacing(5);
}

void FancyPopup::show()
{
	if ( size() != sizeHint() )
		resize( sizeHint() ); // minimumSizeHint()

	// QLabels with rich contents don't propagate mouse clicks
	QList<QLabel *> labels = findChildren<QLabel *>();
	foreach(QLabel *label, labels)
		label->installEventFilter(d);

	// position popup
	move ( d->position() );

	// display popup
	restartHideTimer();
	QFrame::show();
}

void FancyPopup::hideEvent(QHideEvent *e)
{
	d->hideTimer->stop();
	deleteLater();

	QFrame::hideEvent(e);
}

void FancyPopup::mouseReleaseEvent(QMouseEvent *e)
{
	if (!isVisible())
		return;

	emit clicked((int)e->button());
	hide();
}

void FancyPopup::restartHideTimer()
{
	d->hideTimer->start( d->hideTimeout );
}

void FancyPopup::setHideTimeout(int time)
{
	FancyPopup::Private::hideTimeout = time;
}

void FancyPopup::setBorderColor(QColor c)
{
	FancyPopup::Private::backgroundColor = c;
}

#include "fancypopup.moc"
