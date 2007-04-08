#include <QPixmap>
#include <QPixmapCache>
#include <QApplication> // old
#include <QSystemTrayIcon>

#include "psitrayicon.h"
#include "trayicon.h"
#include "iconset.h"
#include "common.h" // options
#include "alerticon.h"

PsiTrayIcon::PsiTrayIcon(const QString &tip, QMenu *popup, bool old, QObject *parent) : QObject(parent), old_(old)
{
	icon_ = NULL;
	trayicon_ = NULL;
	old_trayicon_ = NULL;
	if (old_) {
		old_trayicon_ = new TrayIcon(makeIcon(), tip, popup);
		old_trayicon_->setWMDock(option.isWMDock);
		connect(old_trayicon_, SIGNAL(clicked(const QPoint &, int)), SIGNAL(clicked(const QPoint &, int)));
		connect(old_trayicon_, SIGNAL(doubleClicked(const QPoint &)), SIGNAL(doubleClicked(const QPoint &)));
		connect(old_trayicon_, SIGNAL(closed()), SIGNAL(closed()));
		connect(qApp, SIGNAL(newTrayOwner()), old_trayicon_, SLOT(newTrayOwner()));
		connect(qApp, SIGNAL(trayOwnerDied()), old_trayicon_, SLOT(hide()));
		old_trayicon_->show();
	}
	else {
		trayicon_ = new QSystemTrayIcon();
		trayicon_->setContextMenu(popup);
		setToolTip(tip);
		connect(trayicon_,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),SLOT(trayicon_activated(QSystemTrayIcon::ActivationReason)));
	}
}

PsiTrayIcon::~PsiTrayIcon()
{
	delete old_trayicon_;
	delete trayicon_;
	delete icon_;
}

void PsiTrayIcon::setToolTip(const QString &str)
{
	if (old_)
		old_trayicon_->setToolTip(str);
	else
		trayicon_->setToolTip(str);
}

void PsiTrayIcon::setIcon(const Icon *icon, bool alert)
{
	if ( icon_ ) {
		disconnect(icon_, 0, this, 0 );
		icon_->stop();

		delete icon_;
		icon_ = 0;
	}

	if ( icon ) {
		if ( !alert )
			icon_ = new Icon(*icon);
		else
			icon_ = new AlertIcon(icon);

		connect(icon_, SIGNAL(pixmapChanged(const QPixmap &)), SLOT(animate()));
		icon_->activated();
	}
	else
		icon_ = new Icon();

	animate();
}

void PsiTrayIcon::setAlert(const Icon *icon)
{
	setIcon(icon, true);
}

bool PsiTrayIcon::isAnimating() const
{
	return icon_->isAnimated();
}

bool PsiTrayIcon::isWMDock()
{
	if (old_)
		return old_trayicon_->isWMDock();
	else
		return false;
}

void PsiTrayIcon::show()
{
	if (old_)
		old_trayicon_->show();
	else
		trayicon_->show();
}

void PsiTrayIcon::hide()
{
	if (old_)
		old_trayicon_->hide();
	else
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

#ifdef Q_WS_X11
	// on X11, the KDE dock is 22x22.  let's make our icon_ "seem" bigger.
	QImage real(22,22,32);
	QImage in = icon_->image();
	in.detach();
	real.setAlphaBuffer(true);

	// make sure it is no bigger than 16x16
	if(in.width() > 16 || in.height() > 16)
		in = in.smoothScale(16,16);

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
			if(int a = qAlpha(in.pixel(n,n2))) {
				int x = n + xo + 2;
				int y = n2 + yo + 2;
				real.setPixel(x, y, qRgba(0x80,0x80,0x80,a));
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

	QPixmap pixmap;
	pixmap.convertFromImage(real);
	return pixmap;
#else
	return icon_->pixmap();
#endif
}

void PsiTrayIcon::trayicon_activated(QSystemTrayIcon::ActivationReason)
{
}

void PsiTrayIcon::animate()
{
	if (old_) {
#ifdef Q_WS_X11
		if ( !icon_ )
			return;

		QString cachedName = "PsiTray/" + option.defaultRosterIconset + "/" + icon_->name() + "/" + QString::number( icon_->frameNumber() );

		QPixmap p;
		if ( !QPixmapCache::find(cachedName, p) ) {
			p = makeIcon();
			QPixmapCache::insert( cachedName, p );
		}

		old_trayicon_->setIcon(p);
#else
		old_trayicon_->setIcon( makeIcon() );
#endif
	}
	else {
		if ( !icon_ )
			return;

		QString cachedName = "PsiTray/" + option.defaultRosterIconset + "/" + icon_->name() + "/" + QString::number( icon_->frameNumber() );

		QPixmap p;
		if ( !QPixmapCache::find(cachedName, p) ) {
			p = icon_->pixmap();
			QPixmapCache::insert( cachedName, p );
		}
		trayicon_->setIcon(p);
	}
}
