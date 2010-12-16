/*
 * alerticon.cpp - class for handling animating alert icons
 * Copyright (C) 2003  Michail Pishchagin
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

#include "alerticon.h"
#include "psioptions.h"
#include <QTimer>
#include <QApplication>
//Added by qt3to4:
#include <QPixmap>

//----------------------------------------------------------------------------
// MetaAlertIcon
//----------------------------------------------------------------------------

class MetaAlertIcon : public QObject
{
	Q_OBJECT
public:
	MetaAlertIcon();
	~MetaAlertIcon();

	Impix blank16() const;
	int framenumber() const;

signals:
	void updateFrame(int frame);
	void update();

public slots:
	void updateAlertStyle();

private slots:
	void animTimeout();

private:
	QTimer *animTimer;
	int frame;
	Impix _blank16;
};

static MetaAlertIcon *metaAlertIcon = 0;

MetaAlertIcon::MetaAlertIcon()
: QObject(qApp)
{
	animTimer = new QTimer(this);
	connect(animTimer, SIGNAL(timeout()), SLOT(animTimeout()));
	animTimer->start(120 * 5);
	frame = 0;

	// blank icon
	QImage blankImg(16, 16, QImage::Format_ARGB32);
	blankImg.fill(0x00000000);
	_blank16.setImage(blankImg);
}

MetaAlertIcon::~MetaAlertIcon()
{
}

Impix MetaAlertIcon::blank16() const
{
	return _blank16;
}

void MetaAlertIcon::animTimeout()
{
	frame = !frame;
	emit updateFrame(frame);
}

void MetaAlertIcon::updateAlertStyle()
{
	emit update();
}

int MetaAlertIcon::framenumber() const
{
	return frame;
}

//----------------------------------------------------------------------------
// AlertIcon::Private
//----------------------------------------------------------------------------

class AlertIcon::Private : public QObject
{
	Q_OBJECT
public:
	Private(AlertIcon *_ai);
	~Private();

	void init();

public slots:
	void update();
	void activated(bool playSound);
	void stop();

	void updateFrame(int frame);
	void pixmapChanged();

public:
	AlertIcon *ai;
	PsiIcon *real;
	bool isActivated;
	Impix impix;
};

AlertIcon::Private::Private(AlertIcon *_ai)
{
	if ( !metaAlertIcon )
		metaAlertIcon = new MetaAlertIcon();

	ai = _ai;
	real = 0;
	isActivated = false;
}

AlertIcon::Private::~Private()
{
	if ( isActivated )
		stop();

	if ( real )
		delete real;
}

void AlertIcon::Private::init()
{
	connect(metaAlertIcon, SIGNAL(update()), SLOT(update()));
	connect(real, SIGNAL(iconModified()), SLOT(pixmapChanged()));

	if ( PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "animate" && real->isAnimated() )
		impix = real->frameImpix();
	else
		impix = real->impix();
}

void AlertIcon::Private::update()
{
	stop();
	activated(false);
}

void AlertIcon::Private::activated(bool playSound)
{
	if ( PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "animate" && real->isAnimated() ) {
		if ( !isActivated ) {
			connect(real, SIGNAL(pixmapChanged()), SLOT(pixmapChanged()));
			real->activated(playSound);
			isActivated = true;
		}
	}
	else if ( PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "blink" || (PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "animate" && !real->isAnimated()) ) {
		connect(metaAlertIcon, SIGNAL(updateFrame(int)), SLOT(updateFrame(int)));
	}
	else {
		impix = real->impix();
		emit ai->pixmapChanged();
	}
}

void AlertIcon::Private::stop()
{
	disconnect(metaAlertIcon, SIGNAL(updateFrame(int)), this, SLOT(updateFrame(int)));

	if ( isActivated ) {
		disconnect(real, SIGNAL(pixmapChanged()), this, SLOT(pixmapChanged()));
		real->stop();
		isActivated = false;
	}
}

void AlertIcon::Private::updateFrame(int frame)
{
	if ( !metaAlertIcon ) // just in case
		metaAlertIcon = new MetaAlertIcon();

	if ( frame )
		impix = real->impix();
	else
		impix = metaAlertIcon->blank16();

	emit ai->pixmapChanged();
}

void AlertIcon::Private::pixmapChanged()
{
	impix = real->frameImpix();

	emit ai->pixmapChanged();
}

//----------------------------------------------------------------------------
// AlertIcon
//----------------------------------------------------------------------------

AlertIcon::AlertIcon(const PsiIcon *icon)
{
	d = new Private(this);
	if ( icon )
		d->real = new PsiIcon(*icon);
	else
		d->real = new PsiIcon();

	d->init();
}

AlertIcon::~AlertIcon()
{
	delete d;
}

bool AlertIcon::isAnimated() const
{
	return d->real->isAnimated();
}

const QPixmap &AlertIcon::pixmap() const
{
	return d->impix.pixmap();
}

const QImage &AlertIcon::image() const
{
	return d->impix.image();
}

void AlertIcon::activated(bool playSound)
{
	d->activated(playSound);
}

void AlertIcon::stop()
{
	d->stop();
}

const QIcon &AlertIcon::icon() const
{
	return d->real->icon();
}

const Impix &AlertIcon::impix() const
{
	return d->impix;
}

int AlertIcon::frameNumber() const
{
	if ( PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "animate" && d->real->isAnimated() ) {
		return d->real->frameNumber();
	}
	else if ( PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "blink" || (PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString() == "animate" && !d->real->isAnimated()) ) {
		return metaAlertIcon->framenumber();
	}

	return 0;
}

const QString &AlertIcon::name() const
{
	return d->real->name();
}

PsiIcon *AlertIcon::copy() const
{
	return new AlertIcon(d->real);
}

//----------------------------------------------------------------------------

void alertIconUpdateAlertStyle()
{
	if ( !metaAlertIcon )
		metaAlertIcon = new MetaAlertIcon();

	metaAlertIcon->updateAlertStyle();
}

#include "alerticon.moc"
