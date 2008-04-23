/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef GPGPROC_H
#define GPGPROC_H

#include "qpipe.h"

class QTimer;

namespace gpgQCAPlugin {

// FIXME: Even though deleting an object during a metacall event is supposed
//   to be legal with Qt, it is unfortunately buggy (at least before Qt 4.4).
//   This function performs the following steps:
//     obj->disconnect(owner); // to prevent future signals to owner
//     obj->setParent(0);      // to prevent delete if parent is deleted
//     obj->deleteLater();     // now we can forget about the object
void releaseAndDeleteLater(QObject *owner, QObject *obj);

class SafeTimer : public QObject
{
	Q_OBJECT
public:
	SafeTimer(QObject *parent = 0);
	~SafeTimer();

	int interval() const;
	bool isActive() const;
	bool isSingleShot() const;
	void setInterval(int msec);
	void setSingleShot(bool singleShot);
	int timerId() const;

public slots:
	void start(int msec);
	void start();
	void stop();

signals:
	void timeout();

private:
	QTimer *timer;
};

// GPGProc - executes gpg and provides access to all 6 channels.  NormalMode
//   enables stdout, stderr, and stdin.  ExtendedMode has those 3 plus status
//   aux, and command.  The aux channel is connected to the '-&?' argument.
//   The debug() signal, as well as stderr, can be used for diagnostic text.
class GPGProc : public QObject
{
	Q_OBJECT
public:
	enum Error { FailedToStart, UnexpectedExit, ErrorWrite };
	enum Mode { NormalMode, ExtendedMode };
	GPGProc(QObject *parent = 0);
	~GPGProc();

	void reset();

	bool isActive() const;
	void start(const QString &bin, const QStringList &args, Mode m = ExtendedMode);

	QByteArray readStdout();
	QByteArray readStderr();
	QStringList readStatusLines();
	void writeStdin(const QByteArray &a);
	void writeAux(const QByteArray &a);
#ifdef QPIPE_SECURE
	void writeCommand(const QCA::SecureArray &a);
#else
	void writeCommand(const QByteArray &a);
#endif
	void closeStdin();
	void closeAux();
	void closeCommand();

Q_SIGNALS:
	void error(gpgQCAPlugin::GPGProc::Error error);
	void finished(int exitCode);
	void readyReadStdout();
	void readyReadStderr();
	void readyReadStatusLines();
	void bytesWrittenStdin(int bytes);
	void bytesWrittenAux(int bytes);
	void bytesWrittenCommand(int bytes);
	void debug(const QString &str); // not signal-safe

private:
	class Private;
	friend class Private;
	Private *d;
};

}

#endif
