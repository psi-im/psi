/*
 * Copyright (C) 2003-2006  Justin Karneges <justin@affinix.com>
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

#include "qpipe.h"

#ifdef Q_OS_UNIX
# include <stdlib.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <sys/ioctl.h>
# include <signal.h>
#endif

#define PIPEWRITER_BLOCK    8192
#define PIPEEND_BLOCK       8192
#define PIPEEND_READBUF     16384
#define PIPEEND_READBUF_SEC 1024

namespace QCA {

#ifdef Q_OS_UNIX
// adapted from qt
static void ignore_sigpipe()
{
	// Set to ignore SIGPIPE once only.
	static QBasicAtomic atom = Q_ATOMIC_INIT(0);
	if(atom.testAndSet(0, 1))
	{
		struct sigaction noaction;
		memset(&noaction, 0, sizeof(noaction));
		noaction.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &noaction, 0);
	}
}
#endif

#ifdef Q_OS_UNIX
static bool setBlocking(Q_PIPE_ID pipe, bool b)
{
#ifdef Q_OS_WIN
	DWORD flags = 0;
	if(!b)
		flags |= PIPE_NOWAIT;
	if(!SetNamedPipeHandleState(pipe, &flags, NULL, NULL))
		return false;
	return true;
#endif
#ifdef Q_OS_UNIX
	int flags = fcntl(pipe, F_GETFL);
	if(!b)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if(fcntl(pipe, F_SETFL, flags) == -1)
		return false;
	return true;
#endif
}
#endif

// returns number of bytes available
static int pipe_read_avail(Q_PIPE_ID pipe)
{
	int bytesAvail = 0;
#ifdef Q_OS_WIN
	DWORD i = 0;
	PeekNamedPipe(pipe, 0, 0, 0, &i, 0);
	bytesAvail = (int)i;
#endif
#ifdef Q_OS_UNIX
	size_t nbytes = 0;
	if(ioctl(pipe, FIONREAD, (char *)&nbytes) >= 0)
		bytesAvail = *((int *)&nbytes);
#endif
	return bytesAvail;
}

// returns number of bytes actually read, no more than 'max'.
// -1 on error.  0 means no data, NOT EOF.
// note: even though this function looks like it can return data and EOF
//       at the same time, it never actually does.
static int pipe_read(Q_PIPE_ID pipe, char *data, int max, bool *eof)
{
	int bytesRead = 0;
	if(eof)
		*eof = false;
	if(max < 1)
		return 0;
#ifdef Q_OS_WIN
	DWORD maxread = max;
	DWORD r = 0;
	if(!ReadFile(pipe, data, maxread, &r, 0))
	{
		DWORD err = GetLastError();
		if(err == ERROR_HANDLE_EOF)
		{
			if(eof)
				*eof = true;
		}
		else if(err == ERROR_NO_DATA)
		{
			r = 0;
		}
		else
			return -1;
	}
	bytesRead = (int)r;
#endif
#ifdef Q_OS_UNIX
	int r = 0;
	int ret = read(pipe, data, max);
	if(ret == -1)
	{
		if(errno != EAGAIN)
			return -1;
	}
	else if(ret == 0)
	{
		if(eof)
			*eof = true;
	}
	else
		r = ret;

	bytesRead = r;
#endif
	return bytesRead;
}

// this function only works with blocking pipes on windows.
// for unix, both blocking and non-blocking should work.
// returns number of bytes actually written.
// for blocking pipes, this should always be 'size'.
// -1 on error.
static int pipe_write(Q_PIPE_ID pipe, const char *data, int size)
{
#ifdef Q_OS_WIN
	DWORD written;
	if(!WriteFile(pipe, data, size, &written, 0))
		return -1;
	return written;
#endif
#ifdef Q_OS_UNIX
	ignore_sigpipe();
	int r = 0;
	int ret = write(pipe, data, size);
	if(ret == -1)
	{
		if(errno != EAGAIN)
			return -1;
	}
	else
		r = ret;
	return r;
#endif
}

#ifdef Q_OS_WIN
//----------------------------------------------------------------------------
// QPipeWriter
//----------------------------------------------------------------------------
class QPipeWriter : public QThread
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	QMutex m;
	QWaitCondition w;
	bool do_quit;
	const char *data;
	int size;

	QPipeWriter(Q_PIPE_ID id, QObject *parent = 0) : QThread(parent)
	{
		do_quit = false;
		data = 0;
		DuplicateHandle(GetCurrentProcess(), id, GetCurrentProcess(), &pipe, 0, FALSE, DUPLICATE_SAME_ACCESS);
	}

	~QPipeWriter()
	{
		if(isRunning())
		{
			m.lock();
			do_quit = true;
			w.wakeOne();
			m.unlock();
			if(!wait(100))
				terminate();
		}
		CloseHandle(pipe);
	}

	// data pointer needs to remain until canWrite is emitted
	int write(const char *_data, int _size)
	{
		if(!isRunning())
			return -1;

		QMutexLocker locker(&m);
		if(data)
			return 0;
		data = _data;
		size = _size;
		w.wakeOne();
		return _size;
	}

protected:
	virtual void run()
	{
		while(1)
		{
			m.lock();

			while(!data && !do_quit)
				w.wait(&m);

			if(do_quit)
			{
				m.unlock();
				break;
			}

			const char *p = data;
			int len = size;

			m.unlock();

			if(internalWrite(p, len) != 1)
				break;

			m.lock();
			data = 0;
			m.unlock();

			emit canWrite();
		}
	}

signals:
	void canWrite();

private:
	int internalWrite(const char *p, int len)
	{
		int total = 0;
		while(total < len)
		{
			m.lock();
			if(do_quit)
			{
				m.unlock();
				return 0;
			}
			m.unlock();

			int ret = pipe_write(pipe, p + total, qMin(PIPEWRITER_BLOCK, len - total));
			if(ret == -1)
			{
				// from qt
				if(GetLastError() == 0xE8 /*NT_STATUS_INVALID_USER_BUFFER*/)
				{
					// give the os a rest
					msleep(100);
					continue;
				}

				// on any other error, end thread
				return -1;
			}
			total += ret;
		}
		return 1;
	}
};

//----------------------------------------------------------------------------
// QPipeReader
//----------------------------------------------------------------------------
class QPipeReader : public QThread
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	QMutex m;
	QWaitCondition w;
	bool do_quit, active;

	QPipeReader(Q_PIPE_ID id, QObject *parent = 0) : QThread(parent)
	{
		do_quit = false;
		active = true;
		DuplicateHandle(GetCurrentProcess(), id, GetCurrentProcess(), &pipe, 0, FALSE, DUPLICATE_SAME_ACCESS);
	}

	~QPipeReader()
	{
		if(isRunning())
		{
			m.lock();
			do_quit = true;
			w.wakeOne();
			m.unlock();
			if(!wait(100))
				terminate();
		}
		CloseHandle(pipe);
	}

	void resume()
	{
		QMutexLocker locker(&m);
		active = true;
		w.wakeOne();
	}

protected:
	virtual void run()
	{
		while(1)
		{
			m.lock();

			while(!active && !do_quit)
				w.wait(&m);

			if(do_quit)
			{
				m.unlock();
				break;
			}

			m.unlock();

			while(1)
			{
				unsigned char c;
				bool done;
				int ret = pipe_read(pipe, (char *)&c, 1, &done);
				if(done || ret != 0) // eof, error, or data?
				{
					int result;

					if(done) // we got EOF?
						result = -1;
					else if(ret == -1) // we got an error?
						result = -2;
					else if(ret >= 1) // we got some data??  queue it
						result = c;
					else // no data?  not sure if this can happen
						continue;

					m.lock();
					active = false;
					m.unlock();

					emit canRead(result);
					break;
				}
			}
		}
	}

signals:
	// result values:
	//  >=  0 : readAhead
	//   = -1 : atEnd
	//   = -2 : atError
	void canRead(int result);
};
#endif

//----------------------------------------------------------------------------
// QPipeDevice
//----------------------------------------------------------------------------
class QPipeDevice::Private : public QObject
{
	Q_OBJECT
public:
	QPipeDevice *q;
	Q_PIPE_ID pipe;
	QPipeDevice::Type type;
	bool enabled;
	bool blockReadNotify;
	bool canWrite;

#ifdef Q_OS_WIN
	bool atEnd, atError, forceNotify;
	int readAhead;
	QTimer *readTimer;
	QPipeWriter *pipeWriter;
	QPipeReader *pipeReader;
#endif
#ifdef Q_OS_UNIX
	QSocketNotifier *sn_read, *sn_write;
#endif

	Private(QPipeDevice *_q) : QObject(_q), q(_q), pipe(INVALID_Q_PIPE_ID)
	{
#ifdef Q_OS_WIN
		readTimer = 0;
		pipeWriter = 0;
		pipeReader = 0;
#endif
#ifdef Q_OS_UNIX
		sn_read = 0;
		sn_write = 0;
#endif
	}

	~Private()
	{
		reset();
	}

	void reset()
	{
#ifdef Q_OS_WIN
		atEnd = false;
		atError = false;
		forceNotify = false;
		readAhead = -1;
		delete readTimer;
		readTimer = 0;
		delete pipeWriter;
		pipeWriter = 0;
		delete pipeReader;
		pipeReader = 0;
#endif
#ifdef Q_OS_UNIX
		delete sn_read;
		sn_read = 0;
		delete sn_write;
		sn_write = 0;
#endif
		if(pipe != INVALID_Q_PIPE_ID)
		{
#ifdef Q_OS_WIN
			CloseHandle(pipe);
#endif
#ifdef Q_OS_UNIX
			::close(pipe);
#endif
			pipe = INVALID_Q_PIPE_ID;
		}

		enabled = false;
		blockReadNotify = false;
		canWrite = true;
	}

	void setup(Q_PIPE_ID id, QPipeDevice::Type _type)
	{
		pipe = id;
		type = _type;
	}

	void enable()
	{
		if(enabled)
			return;

		enabled = true;

		if(type == QPipeDevice::Read)
		{
#ifdef Q_OS_WIN
			// pipe reader
			pipeReader = new QPipeReader(pipe, this);
			connect(pipeReader, SIGNAL(canRead(int)), this, SLOT(pr_canRead(int)));
			pipeReader->start();

			// polling timer
			readTimer = new QTimer(this);
			connect(readTimer, SIGNAL(timeout()), SLOT(t_timeout()));

			// NOTE: now that we have pipeReader, this no longer
			//   polls for data.  it only does delayed
			//   singleshot notifications.
			readTimer->setSingleShot(true);
			//readTimer->start(100);
#endif
#ifdef Q_OS_UNIX
			setBlocking(pipe, false);

			// socket notifier
			sn_read = new QSocketNotifier(pipe, QSocketNotifier::Read, this);
			connect(sn_read, SIGNAL(activated(int)), SLOT(sn_read_activated(int)));
#endif
		}
		else
		{
#ifdef Q_OS_UNIX
			setBlocking(pipe, false);

			// socket notifier
			sn_write = new QSocketNotifier(pipe, QSocketNotifier::Write, this);
			connect(sn_write, SIGNAL(activated(int)), SLOT(sn_write_activated(int)));
			sn_write->setEnabled(false);
#endif
		}
	}

public slots:
	void t_timeout()
	{
#ifdef Q_OS_WIN
		if(blockReadNotify)
			return;

		// were we forced to notify?  this can happen if we want to
		//   spread out results across two reads.  whatever caused
		//   the forceNotify already knows what to do, so all we do
		//   is signal.
		if(forceNotify)
		{
			forceNotify = false;
			blockReadNotify = true;
			emit q->notify();
			return;
		}

		// NOTE: disabling this since we now have pipeReader
		// is there data available for reading?  if so, signal.
		/*int bytes = pipe_read_avail(pipe);
		if(bytes > 0)
		{
			blockReadNotify = true;
			emit q->notify();
			return;
		}

		// no data available?  probe for EOF/error
		unsigned char c;
		bool done;
		int ret = pipe_read(pipe, (char *)&c, 1, &done);
		if(done || ret != 0) // eof, error, or data?
		{
			if(done) // we got EOF?
				atEnd = true;
			else if(ret == -1) // we got an error?
				atError = true;
			else if(ret >= 1) // we got some data??  queue it
				readAhead = c;

			blockReadNotify = true;
			emit q->notify();
			return;
		}*/
#endif
	}

	void pw_canWrite()
	{
#ifdef Q_OS_WIN
		canWrite = true;
		emit q->notify();
#endif
	}

	void pr_canRead(int result)
	{
#ifdef Q_OS_WIN
		blockReadNotify = true;
		if(result == -1)
			atEnd = true;
		else if(result == -2)
			atError = true;
		else
			readAhead = result;
		emit q->notify();
#else
		Q_UNUSED(result);
#endif
	}

	void sn_read_activated(int)
	{
#ifdef Q_OS_UNIX
		if(blockReadNotify)
			return;

		blockReadNotify = true;
		emit q->notify();
#endif
	}

	void sn_write_activated(int)
	{
#ifdef Q_OS_UNIX
		canWrite = true;
		sn_write->setEnabled(false);
		emit q->notify();
#endif
	}
};

QPipeDevice::QPipeDevice(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

QPipeDevice::~QPipeDevice()
{
	delete d;
}

QPipeDevice::Type QPipeDevice::type() const
{
	return d->type;
}

bool QPipeDevice::isValid() const
{
	return (d->pipe != INVALID_Q_PIPE_ID);
}

Q_PIPE_ID QPipeDevice::id() const
{
	return d->pipe;
}

int QPipeDevice::idAsInt() const
{
#ifdef Q_OS_WIN
	DWORD dw;
	memcpy(&dw, &d->pipe, sizeof(DWORD));
	return (int)dw;
#endif
#ifdef Q_OS_UNIX
	return d->pipe;
#endif
}

void QPipeDevice::take(Q_PIPE_ID id, Type t)
{
	close();
	d->setup(id, t);
}

void QPipeDevice::enable()
{
	d->enable();
}

void QPipeDevice::close()
{
	d->reset();
}

void QPipeDevice::release()
{
	d->pipe = INVALID_Q_PIPE_ID;
	d->reset();
}

#ifdef Q_OS_WIN
bool QPipeDevice::winDupHandle()
{
	HANDLE h;
	if(!DuplicateHandle(GetCurrentProcess(), d->pipe, GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return false;

	Type t = d->type;
	d->reset();
	d->setup(h, t);
	return true;
}
#endif

int QPipeDevice::bytesAvailable() const
{
	int n = pipe_read_avail(d->pipe);
#ifdef Q_OS_WIN
	if(d->readAhead != -1)
		++n;
#endif
	return n;
}

int QPipeDevice::read(char *data, int maxsize)
{
	if(d->type != QPipeDevice::Read)
		return -1;

	// must read at least 1 byte
	if(maxsize < 1)
		return -1;

#ifdef Q_OS_WIN
	// for resuming the pipeReader thread
	bool wasBlocked = d->blockReadNotify;
#endif

	d->blockReadNotify = false;

#ifdef Q_OS_WIN
	// predetermined results
	if(d->atEnd)
	{
		close();
		return 0;
	}
	if(d->atError)
	{
		close();
		return -1;
	}

	int offset = 0;
	int size = maxsize;

	// prepend readAhead if we have it
	if(d->readAhead != -1)
	{
		unsigned char c = (unsigned char)d->readAhead;
		d->readAhead = -1;
		memcpy(&data[0], &c, 1);
		++offset;
		--size;

		// readAhead was enough data for the caller?
		if(size == 0)
		{
			if(wasBlocked)
				d->pipeReader->resume();
			return offset;
		}
	}

	// read from the pipe now
	bool done;
	int ret = pipe_read(d->pipe, data + offset, size, &done);
	if(done || ret == -1) // eof or error
	{
		// did we already have some data?  if so, defer the eof/error
		if(offset)
		{
			d->forceNotify = true;
			if(done)
				d->atEnd = true;
			else
				d->atError = true;

			// NOTE: now that readTimer is a singleshot, we have
			//   to start it for forceNotify to work
			d->readTimer->start();
		}
		// otherwise, bail
		else
		{
			close();
			if(done)
				return 0;
			else
				return -1;
		}
	}
	else
		offset += ret;

	// pipe still active?  resume the pipeReader
	if(wasBlocked && !d->atEnd && !d->atError)
		d->pipeReader->resume();

	// no data means error
	if(offset == 0)
		return -1;

	return offset;
#endif
#ifdef Q_OS_UNIX
	bool done;
	int r = pipe_read(d->pipe, data, maxsize, &done);
	if(done)
	{
		close();
		return 0;
	}
	if(r == -1)
	{
		close();
		return -1;
	}

	// no data means error
	if(r == 0)
		return -1;

	return r;
#endif
}

int QPipeDevice::write(const char *data, int size)
{
	if(d->type != QPipeDevice::Write)
		return -1;

	// allowed to write?
	if(!d->canWrite)
		return -1;

	// if size is zero, don't bother
	if(size == 0)
		return 0;

	int r;
#ifdef Q_OS_WIN
	if(!d->pipeWriter)
	{
		d->pipeWriter = new QPipeWriter(d->pipe, d);
		connect(d->pipeWriter, SIGNAL(canWrite()), d, SLOT(pw_canWrite()));
		d->pipeWriter->start();
	}
	d->canWrite = false;
	r = d->pipeWriter->write(data, size);
	if(r == -1)
	{
		close();
		return -1;
	}
#endif
#ifdef Q_OS_UNIX
	r = pipe_write(d->pipe, data, size);
	if(r == -1)
	{
		close();
		return -1;
	}

	d->canWrite = false;
	d->sn_write->setEnabled(true);
#endif
	return r;
}

//----------------------------------------------------------------------------
// QPipeEnd
//----------------------------------------------------------------------------
enum ResetMode
{
	ResetSession        = 0,
	ResetSessionAndData = 1,
	ResetAll            = 2
};

class QPipeEnd::Private : public QObject
{
	Q_OBJECT
public:
	QPipeEnd *q;
	QPipeDevice pipe;
	QPipeDevice::Type type;
	QByteArray buf;
	QByteArray curWrite;
#ifdef QPIPE_SECURE
	bool secure;
	QSecureArray sec_buf;
	QSecureArray sec_curWrite;
#endif
	QTimer readTrigger, writeTrigger, closeTrigger;
	bool canRead, activeWrite;
	int lastWrite;
	bool closeLater;
	bool closing;

	Private(QPipeEnd *_q) : QObject(_q), q(_q), pipe(this), readTrigger(this), writeTrigger(this), closeTrigger(this)
	{
		readTrigger.setSingleShot(true);
		writeTrigger.setSingleShot(true);
		closeTrigger.setSingleShot(true);
		connect(&pipe, SIGNAL(notify()), SLOT(pipe_notify()));
		connect(&readTrigger, SIGNAL(timeout()), SLOT(doRead()));
		connect(&writeTrigger, SIGNAL(timeout()), SLOT(doWrite()));
		connect(&closeTrigger, SIGNAL(timeout()), SLOT(doClose()));
		reset(ResetSessionAndData);
	}

	void reset(ResetMode mode)
	{
		pipe.close();
		readTrigger.stop();
		writeTrigger.stop();
		closeTrigger.stop();
		canRead = false;
		activeWrite = false;
		lastWrite = 0;
		closeLater = false;
		closing = false;
		curWrite.clear();
#ifdef QPIPE_SECURE
		secure = false;
		sec_curWrite.clear();
#endif

		if(mode >= ResetSessionAndData)
		{
			buf.clear();
#ifdef QPIPE_SECURE
			sec_buf.clear();
#endif
		}
	}

	void setup(Q_PIPE_ID id, QPipeDevice::Type _type)
	{
		type = _type;
		pipe.take(id, type);
	}

	int pendingSize() const
	{
#ifdef QPIPE_SECURE
		if(secure)
			return sec_buf.size();
		else
#endif
			return buf.size();
	}

	int pendingFreeSize() const
	{
#ifdef QPIPE_SECURE
		if(secure)
			return qMax(PIPEEND_READBUF_SEC - sec_buf.size(), 0);
		else
#endif
			return qMax(PIPEEND_READBUF - buf.size(), 0);
	}

	void appendArray(QByteArray *a, const QByteArray &b)
	{
		(*a) += b;
	}

#ifdef QPIPE_SECURE
	void appendArray(QSecureArray *a, const QSecureArray &b)
	{
		a->append(b);
	}
#endif

	void takeArray(QByteArray *a, int len)
	{
		char *p = a->data();
		int newsize = a->size() - len;
		memmove(p, p + len, newsize);
		a->resize(newsize);
	}

#ifdef QPIPE_SECURE
	void takeArray(QSecureArray *a, int len)
	{
		char *p = a->data();
		int newsize = a->size() - len;
		memmove(p, p + len, newsize);
		a->resize(newsize);
	}
#endif

	void setupNextRead()
	{
		if(pipe.isValid() && canRead)
		{
			canRead = false;
			readTrigger.start(0);
		}
	}

	void setupNextWrite()
	{
		if(!activeWrite)
		{
			activeWrite = true;
			writeTrigger.start(0);
		}
	}

	QByteArray read(QByteArray *buf, int bytes)
	{
		QByteArray a;
		if(bytes == -1 || bytes > buf->size())
		{
			a = *buf;
		}
		else
		{
			a.resize(bytes);
			memcpy(a.data(), buf->data(), a.size());
		}

		takeArray(buf, a.size());
		setupNextRead();
		return a;
	}

	void write(QByteArray *buf, const QByteArray &a)
	{
		appendArray(buf, a);
		setupNextWrite();
	}

#ifdef QPIPE_SECURE
	QSecureArray readSecure(QSecureArray *buf, int bytes)
	{
		QSecureArray a;
		if(bytes == -1 || bytes > buf->size())
		{
			a = *buf;
		}
		else
		{
			a.resize(bytes);
			memcpy(a.data(), buf->data(), a.size());
		}

		takeArray(buf, a.size());
		setupNextRead();
		return a;
	}

	void writeSecure(QSecureArray *buf, const QSecureArray &a)
	{
		appendArray(buf, a);
		setupNextWrite();
	}
#endif

public slots:
	void pipe_notify()
	{
		if(pipe.type() == QPipeDevice::Read)
		{
			doRead();
		}
		else
		{
			// remove what we just wrote
			bool moreData = false;
#ifdef QPIPE_SECURE
			if(secure)
			{
				takeArray(&sec_buf, lastWrite);
				moreData = !sec_buf.isEmpty();
			}
			else
#endif
			{
				takeArray(&buf, lastWrite);
				moreData = !buf.isEmpty();
			}

#ifdef QPIPE_SECURE
			sec_curWrite.clear();
#endif
			curWrite.clear();

			int x = lastWrite;
			lastWrite = 0;

			// more to write?  do it
			if(moreData)
			{
				writeTrigger.start(0);
			}
			// done with all writing
			else
			{
				activeWrite = false;
				if(closeLater)
				{
					closeLater = false;
					closeTrigger.start(0);
				}
			}

			emit q->bytesWritten(x);
		}
	}

	void doRead()
	{
		doReadActual(true);
	}

	void doReadActual(bool sigs)
	{
		int left = pendingFreeSize();
		if(left == 0)
		{
			canRead = true;
			return;
		}

		int ret;
#ifdef QPIPE_SECURE
		if(secure)
		{
			QSecureArray a(qMin(left, pipe.bytesAvailable()));
			ret = pipe.read(a.data(), a.size());
			if(ret >= 1)
			{
				a.resize(ret);
				sec_buf.append(a);
			}
		}
		else
#endif
		{
			QByteArray a(qMin(left, pipe.bytesAvailable()), 0);
			ret = pipe.read(a.data(), a.size());
			if(ret >= 1)
			{
				a.resize(ret);
				buf += a;
			}
		}

		if(ret < 1)
		{
			reset(ResetSession);
			if(sigs)
			{
				if(ret == 0)
					emit q->error(QPipeEnd::ErrorEOF);
				else
					emit q->error(QPipeEnd::ErrorBroken);
			}
			return;
		}

		if(sigs)
			emit q->readyRead();
	}

	void doWrite()
	{
		int ret;
#ifdef QPIPE_SECURE
		if(secure)
		{
			sec_curWrite.resize(qMin(PIPEEND_BLOCK, sec_buf.size()));
			memcpy(sec_curWrite.data(), sec_buf.data(), sec_curWrite.size());

			ret = pipe.write(sec_curWrite.data(), sec_curWrite.size());
		}
		else
#endif
		{
			curWrite.resize(qMin(PIPEEND_BLOCK, buf.size()));
			memcpy(curWrite.data(), buf.data(), curWrite.size());

			ret = pipe.write(curWrite.data(), curWrite.size());
		}

		if(ret == -1)
		{
			reset(ResetSession);
			emit q->error(QPipeEnd::ErrorBroken);
			return;
		}

		lastWrite = ret;
	}

	void doClose()
	{
		reset(ResetSession);
		emit q->closed();
	}
};

QPipeEnd::QPipeEnd(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

QPipeEnd::~QPipeEnd()
{
	delete d;
}

void QPipeEnd::reset()
{
	d->reset(ResetAll);
}

QPipeDevice::Type QPipeEnd::type() const
{
	return d->pipe.type();
}

bool QPipeEnd::isValid() const
{
	return d->pipe.isValid();
}

Q_PIPE_ID QPipeEnd::id() const
{
	return d->pipe.id();
}

QString QPipeEnd::idAsString() const
{
	return QString::number(d->pipe.idAsInt());
}

void QPipeEnd::take(Q_PIPE_ID id, QPipeDevice::Type t)
{
	reset();
	d->setup(id, t);
}

#ifdef QPIPE_SECURE
void QPipeEnd::setSecurityEnabled(bool secure)
{
	// no change
	if(d->secure == secure)
		return;

	if(secure)
	{
		d->sec_buf = d->buf;
		d->buf.clear();
	}
	else
	{
		d->buf = d->sec_buf.toByteArray();
		d->sec_buf.clear();
	}

	d->secure = secure;
}
#endif

void QPipeEnd::enable()
{
	d->pipe.enable();
}

void QPipeEnd::close()
{
	if(!isValid() || d->closing)
		return;

	d->closing = true;

	if(d->activeWrite)
		d->closeLater = true;
	else
		d->closeTrigger.start(0);
}

void QPipeEnd::release()
{
	if(!isValid())
		return;

	d->pipe.release();
	d->reset(ResetSession);
}

#ifdef Q_OS_WIN
bool QPipeEnd::winDupHandle()
{
	return d->pipe.winDupHandle();
}
#endif

void QPipeEnd::finalize()
{
	if(!isValid())
		return;

	if(d->pipe.bytesAvailable())
		d->doReadActual(false);
	d->reset(ResetSession);
}

void QPipeEnd::finalizeAndRelease()
{
	if(!isValid())
		return;

	if(d->pipe.bytesAvailable())
		d->doReadActual(false);
	d->pipe.release();
	d->reset(ResetSession);
}

int QPipeEnd::bytesAvailable() const
{
	return d->pendingSize();
}

int QPipeEnd::bytesToWrite() const
{
	return d->pendingSize();
}

QByteArray QPipeEnd::read(int bytes)
{
	return d->read(&d->buf, bytes);
}

void QPipeEnd::write(const QByteArray &buf)
{
	if(!isValid() || d->closing)
		return;

	if(buf.isEmpty())
		return;

#ifdef QPIPE_SECURE
	if(d->secure) // call writeSecure() instead
		return;
#endif

	d->write(&d->buf, buf);
}

#ifdef QPIPE_SECURE
QSecureArray QPipeEnd::readSecure(int bytes)
{
	return d->readSecure(&d->sec_buf, bytes);
}

void QPipeEnd::writeSecure(const QSecureArray &buf)
{
	if(!isValid() || d->closing)
		return;

	if(buf.isEmpty())
		return;

	if(!d->secure) // call write() instead
		return;

	d->writeSecure(&d->sec_buf, buf);
}
#endif

QByteArray QPipeEnd::takeBytesToWrite()
{
	// only call this on inactive sessions
	if(isValid())
		return QByteArray();

	QByteArray a = d->buf;
	d->buf.clear();
	return a;
}

#ifdef QPIPE_SECURE
QSecureArray QPipeEnd::takeBytesToWriteSecure()
{
	// only call this on inactive sessions
	if(isValid())
		return QSecureArray();

	QSecureArray a = d->sec_buf;
	d->sec_buf.clear();
	return a;
}
#endif

//----------------------------------------------------------------------------
// QPipe
//----------------------------------------------------------------------------
QPipe::QPipe(QObject *parent)
:i(parent), o(parent)
{
}

QPipe::~QPipe()
{
}

void QPipe::reset()
{
	i.reset();
	o.reset();
}

#ifdef QPIPE_SECURE
bool QPipe::create(bool secure)
#else
bool QPipe::create()
#endif
{
	reset();

#ifdef Q_OS_WIN
	SECURITY_ATTRIBUTES secAttr;
	memset(&secAttr, 0, sizeof secAttr);
	secAttr.nLength = sizeof secAttr;
	secAttr.bInheritHandle = TRUE;

	HANDLE r, w;
	if(!CreatePipe(&r, &w, &secAttr, 0))
		return false;
	i.take(r, QPipeDevice::Read);
	o.take(w, QPipeDevice::Write);
#endif

#ifdef Q_OS_UNIX
	int p[2];
	if(pipe(p) == -1)
		return false;
	i.take(p[0], QPipeDevice::Read);
	o.take(p[1], QPipeDevice::Write);
#endif

#ifdef QPIPE_SECURE
	i.setSecurityEnabled(secure);
	o.setSecurityEnabled(secure);
#endif

	return true;
}

}

#include "qpipe.moc"
