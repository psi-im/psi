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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "alerticon.h"
#include "psioptions.h"
#include <QTimer>
#include <QApplication>
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
	, animTimer(new QTimer(this))
	, frame(0)
{
	connect(animTimer, SIGNAL(timeout()), SLOT(animTimeout()));
	animTimer->start(120 * 5);

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

	static void updateAlertStyle();

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
	static QString alertStyle;
};

QString AlertIcon::Private::alertStyle = QString();

AlertIcon::Private::Private(AlertIcon *_ai)
	: ai(_ai)
	, real(0)
	, isActivated(false)
{
	if ( !metaAlertIcon )
		metaAlertIcon = new MetaAlertIcon();
}

AlertIcon::Private::~Private()
{
	if ( isActivated )
		stop();

	delete real;
}

void AlertIcon::Private::updateAlertStyle()
{
	alertStyle = PsiOptions::instance()->getOption("options.ui.notifications.alert-style").toString();
}

void AlertIcon::Private::init()
{
	updateAlertStyle();

	connect(metaAlertIcon, SIGNAL(update()), SLOT(update()));
	connect(real, SIGNAL(iconModified()), SLOT(pixmapChanged()));

	if ( alertStyle == "animate" && real->isAnimated() )
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
	if ( alertStyle == "animate" && real->isAnimated() ) {
		if ( !isActivated ) {
			connect(real, SIGNAL(pixmapChanged()), SLOT(pixmapChanged()));
			real->activated(playSound);
			isActivated = true;
		}
	}
	else if ( alertStyle == "blink" || (alertStyle == "animate" && !real->isAnimated()) ) {
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
		impix = metaAlertIcon->blank16();
	else
		impix = real->impix();

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
	: d(new Private(this))
{
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
	if ( d->alertStyle == "animate" && d->real->isAnimated() ) {
		return d->real->frameNumber();
	}
	else if ( d->alertStyle == "blink" || (d->alertStyle == "animate" && !d->real->isAnimated()) ) {
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

	AlertIcon::Private::updateAlertStyle();

	metaAlertIcon->updateAlertStyle();
}

#include "alerticon.moc"
