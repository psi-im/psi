/*
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
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

/**
   \file qpipe.h

   Header file for the QPipe FIFO class

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QPIPE_H
#define QPIPE_H

#include <QtCore>

#ifndef QPIPE_NO_SECURE
# define QPIPE_SECURE
#endif

#ifdef QPIPE_SECURE
# include <QtCrypto>
#else
# define QCA_EXPORT
#endif

// defs adapted qprocess_p.h
#ifdef Q_OS_WIN
#include <windows.h>
typedef HANDLE Q_PIPE_ID;
#define INVALID_Q_PIPE_ID INVALID_HANDLE_VALUE
#else
typedef int Q_PIPE_ID;
#define INVALID_Q_PIPE_ID -1
#endif

// Note: for Windows console, I/O must be in UTF-8.  Reads are guaranteed to
//   to completely decode (no partial characters).  Likewise, writes must
//   not contain partial characters.

namespace QCA {

// unbuffered direct pipe
class QCA_EXPORT QPipeDevice : public QObject
{
	Q_OBJECT
public:
	enum Type
	{
		Read,
		Write
	};

	QPipeDevice(QObject *parent = 0);
	~QPipeDevice();

	Type type() const;                     // Read or Write
	bool isValid() const;                  // indicates if a pipe is held
	Q_PIPE_ID id() const;                  // pipe id (Win=HANDLE, Unix=int)
	int idAsInt() const;                   // pipe id turned into an integer

	void take(Q_PIPE_ID id, Type t);       // take over the pipe id, close the old
	void enable();                         // enables usage (read/write) of the pipe
	void close();                          // close the pipe
	void release();                        // let go of the pipe but don't close
#ifdef Q_OS_WIN
	bool winDupHandle();                   // DuplicateHandle()
#endif

	int bytesAvailable() const;            // bytes available to read
	int read(char *data, int maxsize);     // return number read, 0 = EOF, -1 = error
	int write(const char *data, int size); // return number taken, ptr must stay valid. -1 on error
	int writeResult(int *written) const;   // 0 = success (wrote all), -1 = error (see written)

Q_SIGNALS:
	void notify();                         // can read or can write, depending on type

private:
	class Private;
	friend class Private;
	Private *d;
};

// buffered higher-level pipe.  use this one.
class QCA_EXPORT QPipeEnd : public QObject
{
	Q_OBJECT
public:
	enum Error
	{
		ErrorEOF,
		ErrorBroken
	};

	QPipeEnd(QObject *parent = 0);
	~QPipeEnd();

	void reset();

	QPipeDevice::Type type() const;
	bool isValid() const;
	Q_PIPE_ID id() const;
	QString idAsString() const;

	void take(Q_PIPE_ID id, QPipeDevice::Type t);
#ifdef QPIPE_SECURE
	void setSecurityEnabled(bool secure);
#endif
	void enable();
	void close();
	void release();
#ifdef Q_OS_WIN
	bool winDupHandle();
#endif

	void finalize(); // do an immediate read, and invalidate
	void finalizeAndRelease(); // same as above, but don't close pipe

	int bytesAvailable() const;
	int bytesToWrite() const;

	// normal i/o
	QByteArray read(int bytes = -1);
	void write(const QByteArray &a);

#ifdef QPIPE_SECURE
	// secure i/o
	QSecureArray readSecure(int bytes = -1);
	void writeSecure(const QSecureArray &a);
#endif

	QByteArray takeBytesToWrite();

#ifdef QPIPE_SECURE
	QSecureArray takeBytesToWriteSecure();
#endif

Q_SIGNALS:
	void readyRead();
	void bytesWritten(int bytes);
	void closed();
	void error(QCA::QPipeEnd::Error e);

private:
	class Private;
	friend class Private;
	Private *d;
};

// creates a full pipe (two pipe ends)
class QCA_EXPORT QPipe
{
public:
	QPipe(QObject *parent = 0);
	~QPipe();

	void reset();

#ifdef QPIPE_SECURE
	bool create(bool secure = false);
#else
	bool create();
#endif

	QPipeEnd & readEnd() { return i; }
	QPipeEnd & writeEnd() { return o; }

private:
	QPipeEnd i, o;
};

}

#endif
