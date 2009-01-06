/*
 * bytestream.cpp - base class for bytestreams
 * Copyright (C) 2003  Justin Karneges
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

#include "bytestream.h"
#include <QByteArray>

// CS_NAMESPACE_BEGIN

//! \class ByteStream bytestream.h
//! \brief Base class for "bytestreams"
//! 
//! This class provides a basic framework for a "bytestream", here defined
//! as a bi-directional, asynchronous pipe of data.  It can be used to create
//! several different kinds of bytestream-applications, such as a console or
//! TCP connection, or something more abstract like a security layer or tunnel,
//! all with the same interface.  The provided functions make creating such
//! classes simpler.  ByteStream is a pure-virtual class, so you do not use it
//! on its own, but instead through a subclass such as \a BSocket.
//!
//! The signals connectionClosed(), delayedCloseFinished(), readyRead(),
//! bytesWritten(), and error() serve the exact same function as those from
//! <A HREF="http://doc.trolltech.com/3.1/qsocket.html">QSocket</A>.
//!
//! The simplest way to create a ByteStream is to reimplement isOpen(), close(),
//! and tryWrite().  Call appendRead() whenever you want to make data available for
//! reading.  ByteStream will take care of the buffers with regards to the caller,
//! and will call tryWrite() when the write buffer gains data.  It will be your
//! job to call tryWrite() whenever it is acceptable to write more data to
//! the underlying system.
//!
//! If you need more advanced control, reimplement read(), write(), bytesAvailable(),
//! and/or bytesToWrite() as necessary.
//!
//! Use appendRead(), appendWrite(), takeRead(), and takeWrite() to modify the
//! buffers.  If you have more advanced requirements, the buffers can be accessed
//! directly with readBuf() and writeBuf().
//!
//! Also available are the static convenience functions ByteStream::appendArray()
//! and ByteStream::takeArray(), which make dealing with byte queues very easy.

class ByteStream::Private
{
public:
	Private() {}

	QByteArray readBuf, writeBuf;
};

//!
//! Constructs a ByteStream object with parent \a parent.
ByteStream::ByteStream(QObject *parent)
:QObject(parent)
{
	d = new Private;
}

//!
//! Destroys the object and frees allocated resources.
ByteStream::~ByteStream()
{
	delete d;
}

//!
//! Returns TRUE if the stream is open, meaning that you can write to it.
bool ByteStream::isOpen() const
{
	return false;
}

//!
//! Closes the stream.  If there is data in the write buffer then it will be
//! written before actually closing the stream.  Once all data has been written,
//! the delayedCloseFinished() signal will be emitted.
//! \sa delayedCloseFinished()
void ByteStream::close()
{
}

//!
//! Writes array \a a to the stream.
void ByteStream::write(const QByteArray &a)
{
	if(!isOpen())
		return;

	bool doWrite = bytesToWrite() == 0 ? true: false;
	appendWrite(a);
	if(doWrite)
		tryWrite();
}

//!
//! Reads bytes \a bytes of data from the stream and returns them as an array.  If \a bytes is 0, then
//! \a read will return all available data.
QByteArray ByteStream::read(int bytes)
{
	return takeRead(bytes);
}

//!
//! Returns the number of bytes available for reading.
int ByteStream::bytesAvailable() const
{
	return d->readBuf.size();
}

//!
//! Returns the number of bytes that are waiting to be written.
int ByteStream::bytesToWrite() const
{
	return d->writeBuf.size();
}

//!
//! Clears the read buffer.
void ByteStream::clearReadBuffer()
{
	d->readBuf.resize(0);
}

//!
//! Clears the write buffer.
void ByteStream::clearWriteBuffer()
{
	d->writeBuf.resize(0);
}

//!
//! Appends \a block to the end of the read buffer.
void ByteStream::appendRead(const QByteArray &block)
{
	appendArray(&d->readBuf, block);
}

//!
//! Appends \a block to the end of the write buffer.
void ByteStream::appendWrite(const QByteArray &block)
{
	appendArray(&d->writeBuf, block);
}

//!
//! Returns \a size bytes from the start of the read buffer.
//! If \a size is 0, then all available data will be returned.
//! If \a del is TRUE, then the bytes are also removed.
QByteArray ByteStream::takeRead(int size, bool del)
{
	return takeArray(&d->readBuf, size, del);
}

//!
//! Returns \a size bytes from the start of the write buffer.
//! If \a size is 0, then all available data will be returned.
//! If \a del is TRUE, then the bytes are also removed.
QByteArray ByteStream::takeWrite(int size, bool del)
{
	return takeArray(&d->writeBuf, size, del);
}

//!
//! Returns a reference to the read buffer.
QByteArray & ByteStream::readBuf()
{
	return d->readBuf;
}

//!
//! Returns a reference to the write buffer.
QByteArray & ByteStream::writeBuf()
{
	return d->writeBuf;
}

//!
//! Attempts to try and write some bytes from the write buffer, and returns the number
//! successfully written or -1 on error.  The default implementation returns -1.
int ByteStream::tryWrite()
{
	return -1;
}

//!
//! Append array \a b to the end of the array pointed to by \a a.
void ByteStream::appendArray(QByteArray *a, const QByteArray &b)
{
	int oldsize = a->size();
	a->resize(oldsize + b.size());
	memcpy(a->data() + oldsize, b.data(), b.size());
}

//!
//! Returns \a size bytes from the start of the array pointed to by \a from.
//! If \a size is 0, then all available data will be returned.
//! If \a del is TRUE, then the bytes are also removed.
QByteArray ByteStream::takeArray(QByteArray *from, int size, bool del)
{
	QByteArray a;
	if(size == 0) {
		a = *from;
		if(del)
			from->resize(0);
	}
	else {
		if(size > (int)from->size())
			size = from->size();
		a.resize(size);
		char *r = from->data();
		memcpy(a.data(), r, size);
		if(del) {
			int newsize = from->size()-size;
			memmove(r, r+size, newsize);
			from->resize(newsize);
		}
	}
	return a;
}
	void connectionClosed();
	void delayedCloseFinished();
	void readyRead();
	void bytesWritten(int);
	void error(int);

//! \fn void ByteStream::connectionClosed()
//! This signal is emitted when the remote end of the stream closes.

//! \fn void ByteStream::delayedCloseFinished()
//! This signal is emitted when all pending data has been written to the stream
//! after an attempt to close.

//! \fn void ByteStream::readyRead()
//! This signal is emitted when data is available to be read.

//! \fn void ByteStream::bytesWritten(int x);
//! This signal is emitted when data has been successfully written to the stream.
//! \a x is the number of bytes written.

//! \fn void ByteStream::error(int code)
//! This signal is emitted when an error occurs in the stream.  The reason for
//! error is indicated by \a code.

// CS_NAMESPACE_END
