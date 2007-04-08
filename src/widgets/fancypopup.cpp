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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "fancypopup.h"

#include <QPixmap>
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QTimer>
#include <QPainter>
#include <QList>
#include <QToolButton>
#include <QStyle>
#include <QDesktopWidget>
#include <QMouseEvent>

#include "iconset.h"
#include "fancylabel.h"
#include "iconlabel.h"
#include "psitooltip.h"

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

	class BackgroundWidget : public QWidget
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
	};

public slots:
	void hide();
	void popupDestroyed(QObject *);

public:
	void initContents(QString title, const Icon *icon, bool copyIcon);

	// parameters
	static int hideTimeout;
	static QColor backgroundColor;

	enum PopupLayout {
		TopToBottom = 1,
		BottomToTop = -1
	};
	PopupLayout popupLayout;

	QList<FancyPopup *> prevPopups;
	QBoxLayout *layout;
	FancyPopup *popup;
	QTimer *hideTimer;
};

int  FancyPopup::Private::hideTimeout = 5 * 1000; // 5 seconds
QColor FancyPopup::Private::backgroundColor = QColor (0x52, 0x97, 0xF9);

FancyPopup::Private::Private(FancyPopup *p)
: QObject(p)
{
	popup = p;

	hideTimer = new QTimer(this);
	connect(hideTimer, SIGNAL(timeout()), SLOT(hide()));
}

FancyPopup::Private::~Private()
{
}

void FancyPopup::Private::hide()
{
	popup->hide();
}

void FancyPopup::Private::popupDestroyed(QObject *obj)
{
	if ( prevPopups.contains((FancyPopup *)obj) ) {
		prevPopups.remove((FancyPopup *)obj);
		popup->move( position() );
	}
}

QPoint FancyPopup::Private::position()
{
	QRect geom = qApp->desktop()->availableGeometry(popup);
	QPoint destination(geom.x() + geom.width(), geom.y() + geom.height()); // in which corner popup should appear

	if ( destination.y() > (qApp->desktop()->screenGeometry().height()/2) )
		popupLayout = Private::BottomToTop;
	else
		popupLayout = Private::TopToBottom;

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

void FancyPopup::Private::initContents(QString title, const Icon *icon, bool copyIcon)
{
	// TODO: use darker color on popup borders
	QPalette backgroundPalette;
	backgroundPalette.setBrush(QPalette::Background, QBrush(backgroundColor));
	QPixmap back(1, 1);
	back.fill(backgroundColor);
	
	QVBoxLayout *vbox = new QVBoxLayout(popup, 0, 0);

	// top row
	QHBoxLayout *tophbox = new QHBoxLayout(vbox);
	QLabel *top1 = new QLabel(popup);
	top1->setAutoFillBackground(true);
	top1->setFixedWidth(3);
	top1->setPalette(backgroundPalette);
	tophbox->addWidget(top1);

	QVBoxLayout *topvbox = new QVBoxLayout(tophbox);
	QLabel *top2 = new QLabel(popup);
	top2->setAutoFillBackground(true);
	top2->setFixedHeight(1);
	top2->setPalette(backgroundPalette);
	topvbox->addWidget(top2);

	QHBoxLayout *tophbox2 = new QHBoxLayout(topvbox);
	
	IconLabel *titleIcon = new IconLabel(popup);
	titleIcon->setAutoFillBackground(true);
	titleIcon->setIcon(icon, copyIcon);
	titleIcon->setPalette(backgroundPalette);
	tophbox2->addWidget(titleIcon);

	QLabel *top5 = new QLabel(popup);
	top5->setAutoFillBackground(true);
	top5->setFixedWidth(3);
	top5->setPalette(backgroundPalette);
	tophbox2->addWidget(top5);

	// title label
	QLabel *titleText = new QLabel(popup);
	titleText->setAutoFillBackground(true);
	QBrush titleFontColor;
	if ( (backgroundColor.red() + backgroundColor.green() + backgroundColor.blue())/3 > 128 )
		titleFontColor = QBrush(Qt::white);
	else
		titleFontColor = QBrush(Qt::black);
	QPalette titlePalette = backgroundPalette;
	titlePalette.setBrush(QPalette::Text, titleFontColor);
	titleText->setPalette(titlePalette);
		
	QFont titleFont = titleText->font();
	titleFont.setBold(true);
	titleText->setFont(titleFont);
	
	titleText->setText( title );
	titleText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tophbox2->addWidget(titleText);

	// 2-pixel space
	Private::BackgroundWidget *spacing = new Private::BackgroundWidget(popup);
	spacing->setBackground(back);
	tophbox2->addWidget(spacing);
	QVBoxLayout *spacingLayout = new QVBoxLayout(spacing);
	spacingLayout->addSpacing(2);

	// close button
	Private::BackgroundWidget *closeButtonBack = new Private::BackgroundWidget(popup);	
	closeButtonBack->setBackground(back);
	tophbox2->addWidget(closeButtonBack);

	QVBoxLayout *closeButtonBackLayout = new QVBoxLayout(closeButtonBack);
	closeButtonBackLayout->setMargin(0);
	closeButtonBackLayout->addStretch();

	QToolButton *closeButton = new QToolButton(closeButtonBack);
	closeButton->setObjectName("closeButton");
	closeButton->setToolTip(tr("Close"));
	closeButtonBackLayout->addWidget( closeButton );
	closeButtonBackLayout->addStretch();
	closeButton->setFocusPolicy( Qt::NoFocus );
	closeButton->setIcon( popup->style()->standardPixmap(QStyle::SP_TitleBarCloseButton) );
	closeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	connect(closeButton, SIGNAL(clicked()), SLOT(hide()));

	QLabel *top3 = new QLabel(popup);
	top3->setAutoFillBackground(true);
	top3->setFixedHeight(1);
	top3->setPalette(backgroundPalette);
	topvbox->addWidget(top3);

	QLabel *top4 = new QLabel(popup);
	top4->setAutoFillBackground(true);
	top4->setFixedWidth(3);
	top4->setPalette(backgroundPalette);
	tophbox->addWidget(top4);

	// middle row
	QHBoxLayout *middlehbox = new QHBoxLayout(vbox);
	QLabel *middle1 = new QLabel(popup);
	middle1->setAutoFillBackground(true);
	middle1->setFixedWidth(4);
	middle1->setPalette(backgroundPalette);
	middlehbox->addWidget(middle1);

	middlehbox->addSpacing(5);
	QVBoxLayout *middlevbox = new QVBoxLayout(middlehbox);
	middlevbox->addSpacing(5);
	layout = middlevbox; // we'll add more items later in addLayout()
	middlehbox->addSpacing(5);

	QLabel *middle3 = new QLabel(popup);
	middle3->setAutoFillBackground(true);
	middle3->setFixedWidth(4);
	middle3->setPalette(backgroundPalette);
	middlehbox->addWidget(middle3);

	// bottom row
	QHBoxLayout *bottomhbox = new QHBoxLayout(vbox);
	QLabel *bottom1 = new QLabel(popup);
	bottom1->setAutoFillBackground(true);
	bottom1->setFixedSize( 4, 4 );
	bottom1->setPalette(backgroundPalette);
	bottomhbox->addWidget(bottom1);

	QLabel *bottom2 = new QLabel(popup);
	bottom2->setAutoFillBackground(true);
	bottom2->setFixedHeight(4);
	bottom2->setPalette(backgroundPalette);
	bottomhbox->addWidget(bottom2);

	QLabel *bottom3 = new QLabel(popup);
	bottom3->setAutoFillBackground(true);
	bottom3->setFixedSize( 4, 4 );
	bottom3->setPalette(backgroundPalette);
	bottomhbox->addWidget(bottom3);
}

//----------------------------------------------------------------------------
// FancyPopup
//----------------------------------------------------------------------------

static const QFlags<Qt::WindowType> 
POPUP_FLAGS = Qt::ToolTip | Qt::WindowStaysOnTopHint; // | Qt::X11BypassWindowManagerHint | Qt::FramelessWindowHint;

FancyPopup::FancyPopup(QString title, const Icon *icon, FancyPopup *prev, bool copyIcon)
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
	d->layout->addLayout(layout, stretch);
	d->layout->addSpacing(5);
}

void FancyPopup::show()
{
	foreach (QObject *obj, children())
		obj->installEventFilter(this);

	if ( size() != sizeHint() )
		resize( sizeHint() ); // minimumSizeHint()

	// position popup
	move ( d->position() );

	// display popup
	d->hideTimer->start( d->hideTimeout );
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
	QWidget *closeButton = (QWidget *)child("closeButton");
	if ( closeButton ) {
		QPoint p = closeButton->mapFromParent( e->pos() );
		if ( p.x() >= 0 && p.y() >= 0 && p.x() < closeButton->width() && p.y() < closeButton->height() )
			QFrame::mouseReleaseEvent(e);
	}

	emit clicked();
	emit clicked((int)e->button());
}

bool FancyPopup::eventFilter(QObject *, QEvent *e)
{
	if (e->type() == QEvent::MouseButtonRelease) {
		mouseReleaseEvent((QMouseEvent *)e);
		return true;
	}
	return false;
}

void FancyPopup::restartHideTimer()
{
	d->hideTimer->stop();
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
