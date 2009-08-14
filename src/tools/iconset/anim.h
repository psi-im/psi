/*
 * anim.h - class for handling animations
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

#ifndef ANIM_H
#define ANIM_H

#include <QByteArray>
#include <QSharedDataPointer>

class QObject;
class QPixmap;
class QImage;
class Impix;
class QThread;

class Anim
{
public:
	Anim();
	Anim(const QByteArray &data);
	Anim(const Anim &anim);
	~Anim();

	const QPixmap &framePixmap() const;
	const QImage &frameImage() const;
	const Impix &frameImpix() const;
	bool isNull() const;

	int frameNumber() const;
	int numFrames() const;
	const Impix &frame(int n) const;

	bool paused() const;
	void unpause();
	void pause();

	void restart();

	void stripFirstFrame();

	static QThread *mainThread();
	static void setMainThread(QThread *);

	void connectUpdate(QObject *receiver, const char *member);
	void disconnectUpdate(QObject *receiver, const char *member = 0);

	Anim & operator= (const Anim &);
	Anim copy() const;
	void detach();

	class Private;
private:
	QSharedDataPointer<Private> d;
};

#endif
