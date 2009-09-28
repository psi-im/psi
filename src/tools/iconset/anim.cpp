/*
 * anim.cpp - class for handling animations
 * Copyright (C) 2003-2006  Michail Pishchagin
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

#include "anim.h"
#include "iconset.h"

#include <QObject>
#include <QImageReader>
#include <QTimer>
//#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QThread>

/**
 * \class Anim
 * \brief Class for handling animations
 *
 * Anim is a class that can load animations. Generally, it looks like
 * QMovie but it loads animations in one pass and stores it in memory.
 *
 * Each frame of Anim is stored as Impix.
 */

static QThread *animMainThread = 0;

//! \if _hide_doc_
class Anim::Private : public QObject, public QSharedData
{
	Q_OBJECT
public:
	QTimer *frametimer;

	bool empty;
	bool paused;

	int speed;
	int lasttimerinterval;

	int looping, loop;

	class Frame {
	public:
		Impix impix;
		int period;
	};

	QList<Frame> frames;
	int frame;

public:
	void init()
	{
		frametimer = new QTimer();
		if (animMainThread && animMainThread != QThread::currentThread()) {
			frametimer->moveToThread(animMainThread);
			moveToThread(animMainThread);
		}
		QObject::connect(frametimer, SIGNAL(timeout()), this, SLOT(refresh()));

		speed = 120;
		lasttimerinterval = -1;

		looping = 0; // MNG movies doesn't have loop flag?
		loop = 0;
		frame = 0;
		paused = true;
	}

	Private()
	{
		init();
	}

	Private(const Private &from)
		: QObject(), QSharedData()
	{
		init();

		speed = from.speed;
		lasttimerinterval = from.lasttimerinterval;
		looping = from.looping;
		loop = from.loop;
		frame = from.frame;
		paused = from.paused;
		frames = from.frames;
		
		if ( !paused )
			unpause();
	}

	Private(const QByteArray *ba)
	{
		init();

		QBuffer buffer((QByteArray *)ba);
		buffer.open(QBuffer::ReadOnly);
		QImageReader reader(&buffer);
		
		while ( reader.canRead() ) {
			QImage image = reader.read();
			if ( !image.isNull() ) {
				Frame newFrame;
				frames.append( newFrame );
				frames.last().impix  = Impix(image);
				frames.last().period = reader.nextImageDelay();
			}
			else {
				break;
			}
		}
		
		looping = reader.loopCount();
		
		if ( !reader.supportsAnimation() && (frames.count() == 1) ) {
			QImage frame = frames[0].impix.image();
			
			// we're gonna slice the single image we've got if we're absolutely sure
			// that it's can be cut into multiple frames
			if ((frame.width() / frame.height() > 0) && !(frame.width() % frame.height())) {
				int h = frame.height();
				QList<Frame> newFrames;
				
				for (int i = 0; i < frame.width() / frame.height(); i++) {
					Frame newFrame;
					newFrames.append( newFrame );
					newFrames.last().impix  = Impix(frame.copy(i * h, 0, h, h));
					newFrames.last().period = 120;
				}
				
				frames  = newFrames;
				looping = 0;
			}
		}
	}
	
	~Private()
	{
		if ( frametimer )
			delete frametimer;
	}

	void pause()
	{
		paused = true;
		frametimer->stop();
	}

	void unpause()
	{
		paused = false;
		restartTimer();
	}

	void restart()
	{
		frame = 0;
		if ( !paused )
			restartTimer();
	}

	int numFrames() const
	{
		return frames.count();
	}

	void restartTimer()
	{
		if ( !paused && speed > 0 ) {
			int frameperiod = frames[frame].period;
			int i = frameperiod >= 0 ? frameperiod * 100/speed : 0;
			if ( i != lasttimerinterval || !frametimer->isActive() ) {
				lasttimerinterval = i;
				frametimer->start( i );
			}
		} else {
			frametimer->stop();
		}
	}

signals:
	void areaChanged();

public slots:
	void refresh()
	{
		frame++;
		if ( frame >= numFrames() ) {
			frame = 0;

			loop++;
			if ( looping && loop >= looping ) {
				frame = numFrames() - 1;
				pause();
				restart();
			}
		}

		emit areaChanged();
		restartTimer();
	}
};
//! \endif

/**
 * Creates an empty animation.
 */
Anim::Anim()
{
	d = new Private();
}

/**
 * Creates animation from QByteArray \a data.
 */
Anim::Anim(const QByteArray &data)
{
	d = new Private(&data);
}

/**
 * Creates shared copy of Anim \a anim.
 */
Anim::Anim(const Anim &anim)
{
	d = anim.d;
}

/**
 * Deletes animation.
 */
Anim::~Anim()
{
}

/**
 * Returns QPixmap of current frame.
 */
const QPixmap &Anim::framePixmap() const
{
	return d->frames[d->frame].impix.pixmap();
}

/**
 * Returns QImage of current frame.
 */
const QImage &Anim::frameImage() const
{
	return d->frames[d->frame].impix.image();
}

/**
 * Returns Impix of current frame.
 */
const Impix &Anim::frameImpix() const
{
	return d->frames[d->frame].impix;
}

/**
 * Returns total number of frames in animation.
 */
int Anim::numFrames() const
{
	return d->numFrames();
}

/**
 * Returns the number of current animation frame.
 */
int Anim::frameNumber() const
{
	return d->frame;
}

/**
 * Returns Impix of animation frame number \a n.
 */
const Impix &Anim::frame(int n) const
{
	return d->frames[n].impix;
}

/**
 * Returns \c true if numFrames() == 0 and \c false otherwise.
 */
bool Anim::isNull() const
{
	return !numFrames();
}

/**
 * Returns \c true when animation is paused.
 */
bool Anim::paused() const
{
	return d->paused;
}

/**
 * Continues the animation playback.
 */
void Anim::unpause()
{
	if ( !isNull() && paused() )
		d->unpause();
}

/**
 * Pauses the animation.
 */
void Anim::pause()
{
	if ( !isNull() && !paused() )
		d->pause();
}

/**
 * Starts animation from the very beginning.
 */
void Anim::restart()
{
	if ( !isNull() )
		d->restart();
}

/**
 * Connects internal signal with specified slot \a member of object \a receiver, which
 * is emitted when animation changes its frame.
 *
 * \code
 * class MyObject : public QObject {
 *    Q_OBJECT
 *    // ...
 *    Anim *anim;
 *    void initAnim() {
 *       anim = new Anim( QByteArray(someData()) );
 *       anim->connectUpdate( this, SLOT(animUpdated()) );
 *    }
 * public slots:
 *    void animUpdated();
 *    // ...
 * }
 * \endcode
 *
 * \sa disconnectUpdate()
 */
void Anim::connectUpdate(QObject *receiver, const char *member)
{
	QObject::connect(d, SIGNAL(areaChanged()), receiver, member);
}

/**
 * Disconnects slot \a member, which was prevously connected with connectUpdate().
 * \sa connectUpdate()
 */
void Anim::disconnectUpdate(QObject *receiver, const char *member)
{
	QObject::disconnect(d, SIGNAL(areaChanged()), receiver, member);
}

Anim & Anim::operator= (const Anim &from)
{
	d = from.d;

	return *this;
}

Anim Anim::copy() const
{
	Anim anim( *this );
	anim.d = new Private( *this->d.data() );

	return anim;
}

void Anim::detach()
{
	d.detach();
}

/**
 * Strips the first animation frame, if there is more than one frame.
 */
void Anim::stripFirstFrame()
{
	detach();
	if ( numFrames() > 1 ) {
		d->frames.takeFirst();

		if ( !paused() )
			restart();
	}
}

/**
 * Sets the main thread that will be used to create objects. Useful if you want
 * to create Anim in non-main thread.
 */
void Anim::setMainThread(QThread *thread)
{
	animMainThread = thread;
}

/**
 * Get the Anim's main thread.
 */
QThread *Anim::mainThread()
{
	return animMainThread;
}

#include "anim.moc"
