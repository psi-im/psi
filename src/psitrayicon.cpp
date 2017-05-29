#include <QPixmap>
#include <QPixmapCache>
#include <QApplication> // old
#include <QSystemTrayIcon>
#include <QHelpEvent>

#include "psitrayicon.h"
#include "iconset.h"
#include "alerticon.h"
#include "common.h"

// TODO: remove the QPoint parameter from the signals when we finally move
// to the new system.

PsiTrayIcon::PsiTrayIcon(const QString &tip, QMenu *popup, QObject *parent)
	: QObject(parent)
	, icon_(NULL)
	, trayicon_(new QSystemTrayIcon())
	, realIcon_(0)
{	
	trayicon_->setContextMenu(popup);
	setToolTip(tip);
	connect(trayicon_,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),SLOT(trayicon_activated(QSystemTrayIcon::ActivationReason)));
	trayicon_->installEventFilter(this);
}

PsiTrayIcon::~PsiTrayIcon()
{
	delete trayicon_;
	if ( icon_ ) {
		icon_->disconnect();
		icon_->stop();
		delete icon_;
	}
}

void PsiTrayIcon::setContextMenu(QMenu* menu)
{
	trayicon_->setContextMenu(menu);
}

void PsiTrayIcon::setToolTip(const QString &str)
{
#ifndef HAVE_X11
	trayicon_->setToolTip(str);
#else
	Q_UNUSED(str)
#endif
}

void PsiTrayIcon::setIcon(const PsiIcon *icon, bool alert)
{
	if ( icon_ ) {
		icon_->disconnect();
		icon_->stop();

		delete icon_;
		icon_ = 0;
	}

	realIcon_ = quintptr(icon);
	if ( icon ) {
		if ( !alert )
			icon_ = new PsiIcon(*icon);
		else
			icon_ = new AlertIcon(icon);

		connect(icon_, SIGNAL(pixmapChanged()), SLOT(animate()));
		icon_->activated();
	}
	else
		icon_ = new PsiIcon();

	animate();
}

void PsiTrayIcon::setAlert(const PsiIcon *icon)
{
	setIcon(icon, true);
}

bool PsiTrayIcon::isAnimating() const
{
	return icon_->isAnimated();
}

bool PsiTrayIcon::isWMDock()
{
	return false;
}

void PsiTrayIcon::show()
{
	trayicon_->show();
}

void PsiTrayIcon::hide()
{
	trayicon_->hide();
}


// a function to blend 2 pixels taking their alpha channels
// into consideration
// p1 is in the 1st layer, p2 is in the 2nd layer (over p1)
QRgb PsiTrayIcon::pixelBlend(QRgb p1, QRgb p2)
{
	int a2 = qAlpha(p2);
	if (a2 == 255) return p2; // don't calculate anything if p2 is completely opaque
	int a1 = qAlpha(p1);
	double prop1 = double(a1*(255-a2))/double(255*255);
	double prop2 = double(a2)/255.0;
	int r = int( qRed(p1)*prop1 + qRed(p2)*prop2 );
	int g = int( qGreen(p1)*prop1 + qGreen(p2)*prop2 );
	int b = int( qBlue(p1)*prop1 + qBlue(p2)*prop2 );
	return qRgba(r, g, b, (a1>a2) ? a1:a2);
}


QPixmap PsiTrayIcon::makeIcon()
{
	if ( !icon_ )
		return QPixmap();

#ifdef HAVE_X11
	// on X11, the KDE dock is 22x22.  let's make our icon_ "seem" bigger.
	QImage real(22,22,QImage::Format_ARGB32);
	QImage in = icon_->image();
	in.detach();

	// make sure it is no bigger than 16x16
	if(in.width() > 16 || in.height() > 16)
		in = in.scaled(16,16,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

	int xo = (real.width() - in.width()) / 2;
	int yo = (real.height() - in.height()) / 2;

	int n, n2;

	// clear the output and make it transparent
	// deprecates real.fill(0)
	for(n2 = 0; n2 < real.height(); ++n2)
		for(n = 0; n < real.width(); ++n)
			real.setPixel(n, n2, qRgba(0,0,0,0));

	// draw a dropshadow
	for(n2 = 0; n2 < in.height(); ++n2) {
		for(n = 0; n < in.width(); ++n) {
			if(int a = qAlpha(in.pixel(n,n2)) / 2) {
				int x = n + xo + 2;
				int y = n2 + yo + 2;
				real.setPixel(x, y, qRgba(0,0,0,a));
			}
		}
	}

	// draw the image
	for(n2 = 0; n2 < in.height(); ++n2) {
		for(n = 0; n < in.width(); ++n) {
			if(qAlpha(in.pixel(n,n2))) {
				QRgb pold = real.pixel(n + xo , n2 + yo);
				QRgb pnew = in.pixel(n , n2);
				real.setPixel(n + xo, n2 + yo, pixelBlend(pold, pnew));
			}
		}
	}

	return QPixmap::fromImage(real);
#else
	return icon_->pixmap();
#endif
}

void PsiTrayIcon::trayicon_activated(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MAC
	Q_UNUSED(reason)
#else
	if (reason == QSystemTrayIcon::Trigger)
		emit clicked(QPoint(),Qt::LeftButton);
	else if (reason == QSystemTrayIcon::MiddleClick || (isKde() && reason == QSystemTrayIcon::Context))
		emit clicked(QPoint(),Qt::MidButton);
	else if (reason == QSystemTrayIcon::DoubleClick)
		emit doubleClicked(QPoint());
#endif
}

void PsiTrayIcon::animate()
{
	if ( !icon_ )
		return;

	QString cachedName = "PsiTray/" + icon_->name() + "/" + QString::number(realIcon_) + "/"
			+ QString::number( icon_->frameNumber() );

	QPixmap p;
	if ( !QPixmapCache::find(cachedName, p) ) {
		p = makeIcon();
		QPixmapCache::insert( cachedName, p );
	}
	trayicon_->setIcon(p);
}

bool PsiTrayIcon::eventFilter(QObject *obj, QEvent *event)
{
	if(obj == trayicon_ && event->type() == QEvent::ToolTip) {
		doToolTip(obj, ((QHelpEvent*)event)->globalPos());
		return true;
	}
	return QObject::eventFilter(obj, event);
}
