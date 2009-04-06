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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

// Note: if we ever enable the threaded backend, we need to protect:
//   QPipeDevice read and bytesAvailable
//   QPipeEnd finalize

// Note: we never use the return value for QPipeWriter::stop, but I don't
//   think this matters much

#include "qpipe.h"

#include <stdlib.h>
#include <limits.h>

// sorry, i've added this dependency for now, but it's easy enough to take
//   with you if you want qpipe independent of qca
#include "qca_safeobj.h"

#ifdef Q_OS_WIN
# include <QThread>
# include <QMutex>
# include <QWaitCondition>
# include <QTextCodec>
# include <QTextEncoder>
# include <QTextDecoder>
#else
# include <QMutex>
#endif

#ifdef Q_OS_UNIX
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <sys/ioctl.h>
# include <signal.h>
# ifdef HAVE_SYS_FILIO_H
#  include <sys/filio.h>
# endif
#endif

#define USE_POLL

#define CONSOLE_CHAREXPAND  5
#define PIPEWRITER_POLL     1000
#define PIPEREADER_POLL     100
#define PIPEWRITER_BLOCK    8192
#define PIPEEND_BLOCK       8192
#define PIPEEND_READBUF     16384
#define PIPEEND_READBUF_SEC 1024

namespace QCA {

#ifdef Q_OS_UNIX
// adapted from qt
Q_GLOBAL_STATIC(QMutex, ign_mutex)
static bool ign_sigpipe = false;

static void ignore_sigpipe()
{
	// Set to ignore SIGPIPE once only.
//#if QT_VERSION < 0x040400
	QMutexLocker locker(ign_mutex());
	if(!ign_sigpipe)
	{
		ign_sigpipe = true;
//#else
//	static QBasicAtomicInt atom = Q_BASIC_ATOMIC_INITIALIZER(0);
//	if(atom.testAndSetRelaxed(0, 1))
//	{
//#endif
		struct sigaction noaction;
		memset(&noaction, 0, sizeof(noaction));
		noaction.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &noaction, 0);
	}
}
#endif

#ifdef Q_OS_WIN
static int pipe_dword_cap_to_int(DWORD dw)
{
	if(sizeof(int) <= sizeof(DWORD))
		return (int)((dw > INT_MAX) ? INT_MAX : dw);
	else
		return (int)dw;
}

static bool pipe_dword_overflows_int(DWORD dw)
{
	if(sizeof(int) <= sizeof(DWORD))
		return (dw > INT_MAX) ? true : false;
	else
		return false;
}
#endif

#ifdef Q_OS_UNIX
static int pipe_size_t_cap_to_int(size_t size)
{
	if(sizeof(int) <= sizeof(size_t))
		return (int)((size > INT_MAX) ? INT_MAX : size);
	else // maybe silly..  can int ever be larger than size_t?
		return (int)size;
}
#endif

static bool pipe_set_blocking(Q_PIPE_ID pipe, bool b)
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

// on windows, the pipe is closed and the new pipe is returned in newPipe
static bool pipe_set_inheritable(Q_PIPE_ID pipe, bool b, Q_PIPE_ID *newPipe = 0)
{
#ifdef Q_OS_WIN
	// windows is required to accept a new pipe id
	if(!newPipe)
		return false;
	HANDLE h;
	if(!DuplicateHandle(GetCurrentProcess(), pipe, GetCurrentProcess(), &h, 0, b, DUPLICATE_SAME_ACCESS))
		return false;
	*newPipe = h;
	return true;
#endif
#ifdef Q_OS_UNIX
	if(newPipe)
		*newPipe = pipe;
	int flags = fcntl(pipe, F_GETFD);
	if(!b)
		flags |= FD_CLOEXEC;
	else
		flags &= ~FD_CLOEXEC;
	if(fcntl(pipe, F_SETFD, flags) == -1)
		return false;
	return true;
#endif
}

// returns number of bytes available
static int pipe_read_avail(Q_PIPE_ID pipe)
{
	int bytesAvail = 0;
#ifdef Q_OS_WIN
	DWORD i = 0;
	if(PeekNamedPipe(pipe, 0, 0, 0, &i, 0))
		bytesAvail = pipe_dword_cap_to_int(i);
#endif
#ifdef Q_OS_UNIX
	size_t nbytes = 0;
	if(ioctl(pipe, FIONREAD, (char *)&nbytes) >= 0)
		bytesAvail = pipe_size_t_cap_to_int(nbytes);
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
	bytesRead = (int)r; // safe to cast, since 'max' is signed
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

// returns number of bytes actually written.
// for blocking pipes, this should always be 'size'.
// -1 on error.
static int pipe_write(Q_PIPE_ID pipe, const char *data, int size)
{
#ifdef Q_OS_WIN
	DWORD written;
	if(!WriteFile(pipe, data, size, &written, 0))
		return -1;
	return (int)written; // safe to cast, since 'size' is signed
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

// Windows Console functions

#ifdef Q_OS_WIN

static bool pipe_is_a_console(Q_PIPE_ID pipe)
{
	DWORD mode;
	if(GetConsoleMode(pipe, &mode))
		return true;
	return false;
}

// returns the number of keypress events in the console input queue,
//   or -1 if there is an error (don't forget this!!)
static int pipe_read_avail_console(Q_PIPE_ID pipe)
{
	DWORD count, i;
	INPUT_RECORD *rec;
	int n, icount, total;

	// how many events are there?
	if(!GetNumberOfConsoleInputEvents(pipe, &count))
		return -1;

	// peek them all
	rec = (INPUT_RECORD *)malloc(count * sizeof(INPUT_RECORD));
	BOOL ret;
	QT_WA(
		ret = PeekConsoleInputW(pipe, rec, count, &i);
	,
		ret = PeekConsoleInputA(pipe, rec, count, &i);
	)
	if(!ret)
	{
		free(rec);
		return -1;
	}

	icount = pipe_dword_cap_to_int(i); // process only the amount returned

	// see which ones are normal keypress events
	total = 0;
	for(n = 0; n < icount; ++n)
	{
		if(rec[n].EventType == KEY_EVENT)
		{
			KEY_EVENT_RECORD *ke = &rec[n].Event.KeyEvent;
			if(ke->bKeyDown && ke->uChar.AsciiChar != 0)
				total += ke->wRepeatCount;
		}
	}

	free(rec);
	return total;
}

// pass dec to keep a long-running decoder, else 0
static int pipe_read_console(Q_PIPE_ID pipe, ushort *data, int max, bool *eof, QTextDecoder *dec = 0)
{
	int n, size, count;
	bool own_decoder;

	if(eof)
		*eof = false;
	if(max < 1)
		return 0;

	count = pipe_read_avail_console(pipe);
	if(count == -1)
		return -1;
	if(count == 0)
		return 0;

	if(dec)
	{
		own_decoder = false;
	}
	else
	{
		QT_WA(
			dec = 0;
		,
			dec = QTextCodec::codecForLocale()->makeDecoder();
		)
		own_decoder = true;
	}

	size = 0;
	for(n = 0; n < count && size < max; ++n)
	{
		bool use_uni = true;
		quint16 uni = 0;
		quint8 ansi = 0;

		BOOL ret;
		DWORD i;
		QT_WA(
			ret = ReadConsoleW(pipe, &uni, 1, &i, NULL);
		,
			ret = ReadConsoleA(pipe, &ansi, 1, &i, NULL);
			use_uni = false;
		)
		if(!ret)
		{
			// if the first read is an error, then report error
			if(n == 0)
			{
				delete dec;
				return -1;
			}
			// if we have some data, don't count this as an error.
			//   we'll probably get it again next time around...
			else
				break;
		}

		QString substr;
		if(use_uni)
			substr = QChar(uni);
		else
			substr = dec->toUnicode((const char *)&ansi, 1);

		for(int k = 0; k < substr.length() && size < max; ++k)
		{
			QChar c = substr[k];
			if(c == QChar(0x1A)) // EOF?
			{
				if(eof)
					*eof = true;
				break;
			}
			data[size++] = substr[k].unicode();
		}
	}
	if(own_decoder)
		delete dec;

	return size;
}

static int pipe_write_console(Q_PIPE_ID pipe, const ushort *data, int size)
{
	DWORD i;
	BOOL ret;
	QT_WA(
		ret = WriteConsoleW(pipe, data, size, &i, NULL);
	,
		// Note: we lose security by converting to QString here, but
		//   who really cares if we're writing to a *display* ? :)
		QByteArray out = QString::fromUtf16(data, size).toLocal8Bit();
		ret = WriteConsoleA(pipe, out.data(), out.size(), &i, NULL);
		if(ret)
		{
			// convert number of bytes to number of unicode chars
			i = (DWORD)QString::fromLocal8Bit(out.mid(0, i)).length();
			if(pipe_dword_overflows_int(i))
				return -1;
		}
	)
	if(!ret)
		return -1;
	return (int)i; // safe to cast since 'size' is signed
}
#endif

#ifdef Q_OS_WIN

// Here is the multi-backend stuff for windows.  QPipeWriter and QPipeReader
//   define a common interface, and then subclasses (like QPipeWriterThread)
//   are used by QPipeDevice.  The base classes inherit from QThread, even
//   if threads aren't used, so that I can define signals without dealing
//   with multiple QObject inheritance in the thread subclasses (it is also
//   possible that I'm missing something obvious and don't need to do this).

// Note:
//   QPipeWriterThread and QPipeReaderThread require the pipes to be in
//     blocking mode.  QPipeWriterPoll and QPipeReaderPoll require the pipes
//     to be in non-blocking mode.

//----------------------------------------------------------------------------
// QPipeWriter
//----------------------------------------------------------------------------
class QPipeWriter : public QThread
{
	Q_OBJECT
public:
	QPipeWriter(QObject *parent = 0) : QThread(parent)
	{
	}

	virtual ~QPipeWriter()
	{
	}

	// start
	virtual void start() = 0;

	// stop, and return number of bytes written so far
	virtual int stop() = 0;

	// data pointer needs to remain until canWrite is emitted
	virtual int write(const char *data, int size) = 0;

signals:
	// result values:
	//   =   0 : success
	//   =  -1 : error
	void canWrite(int result, int bytesWritten);

protected:
	virtual void run()
	{
		// implement a default to satisfy the polling subclass
	}
};

//----------------------------------------------------------------------------
// QPipeReader
//----------------------------------------------------------------------------
class QPipeReader : public QThread
{
	Q_OBJECT
public:
	QPipeReader(QObject *parent = 0) : QThread(parent)
	{
	}

	virtual ~QPipeReader()
	{
	}

	// start
	virtual void start() = 0;

	// to be called after every read
	virtual void resume() = 0;

signals:
	// result values:
	//  >=  0 : readAhead
	//   = -1 : atEnd
	//   = -2 : atError
	//   = -3 : data available, but no readAhead
	void canRead(int result);

protected:
	virtual void run()
	{
		// implement a default to satisfy the polling subclass
	}
};

//----------------------------------------------------------------------------
// QPipeWriterThread
//----------------------------------------------------------------------------
class QPipeWriterThread : public QPipeWriter
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	QMutex m;
	QWaitCondition w;
	bool do_quit;
	const char *data;
	int size;

	QPipeWriterThread(Q_PIPE_ID id, QObject *parent = 0) : QPipeWriter(parent)
	{
		do_quit = false;
		data = 0;
		connect(this, SIGNAL(canWrite_p(int, int)), SIGNAL(canWrite(int, int)));
		DuplicateHandle(GetCurrentProcess(), id, GetCurrentProcess(), &pipe, 0, false, DUPLICATE_SAME_ACCESS);
	}

	virtual ~QPipeWriterThread()
	{
		stop();
		CloseHandle(pipe);
	}

	virtual void start()
	{
		pipe_set_blocking(pipe, true);
		QThread::start();
	}

	virtual int stop()
	{
		if(isRunning())
		{
			m.lock();
			do_quit = true;
			w.wakeOne();
			m.unlock();
			if(!wait(100))
				terminate();
			do_quit = false;
			data = 0;
		}
		return size;
	}

	virtual int write(const char *_data, int _size)
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

			int ret = internalWrite(p, len);

			m.lock();
			data = 0;
			size = ret;
			m.unlock();

			emit canWrite_p(ret < len ? -1 : 0, ret);
		}
	}

private:
	// attempts to write len bytes.  value returned is number of bytes written.
	//   any return value less than len means a write error was encountered
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
				// from qt, don't know why
				if(GetLastError() == 0xE8) // NT_STATUS_INVALID_USER_BUFFER
				{
					// give the os a rest
					msleep(100);
					continue;
				}

				// on any other error, end thread
				return total;
			}
			total += ret;
		}
		return total;
	}

signals:
	void canWrite_p(int result, int bytesWritten);
};

//----------------------------------------------------------------------------
// QPipeWriterPoll
//----------------------------------------------------------------------------
class QPipeWriterPoll : public QPipeWriter
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	const char *data;
	int size;
	SafeTimer timer;
	int total;

	QPipeWriterPoll(Q_PIPE_ID id, QObject *parent = 0) : QPipeWriter(parent), timer(this)
	{
		pipe = id;
		data = 0;
		connect(&timer, SIGNAL(timeout()), SLOT(tryNextWrite()));
	}

	virtual ~QPipeWriterPoll()
	{
	}

	virtual void start()
	{
		pipe_set_blocking(pipe, false);
	}

	// return number of bytes written
	virtual int stop()
	{
		timer.stop();
		data = 0;
		return total;
	}

	// data pointer needs to remain until canWrite is emitted
	virtual int write(const char *_data, int _size)
	{
		total = 0;
		data = _data;
		size = _size;
		timer.start(0); // write at next event loop
		return _size;
	}

private slots:
	void tryNextWrite()
	{
		int written = pipe_write(pipe, data + total, size - total);
		bool error = false;
		if(written == -1)
		{
			error = true;
			written = 0; // no bytes written on error

			// from qt, they don't count it as fatal
			if(GetLastError() == 0xE8) // NT_STATUS_INVALID_USER_BUFFER
				error = false;
		}

		total += written;
		if(error || total == size)
		{
			timer.stop();
			data = 0;
			emit canWrite(error ? -1 : 0, total);
			return;
		}

		timer.setInterval(PIPEWRITER_POLL);
	}
};

//----------------------------------------------------------------------------
// QPipeReaderThread
//----------------------------------------------------------------------------
class QPipeReaderThread : public QPipeReader
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	QMutex m;
	QWaitCondition w;
	bool do_quit, active;

	QPipeReaderThread(Q_PIPE_ID id, QObject *parent = 0) : QPipeReader(parent)
	{
		do_quit = false;
		active = true;
		connect(this, SIGNAL(canRead_p(int)), SIGNAL(canRead(int)));
		DuplicateHandle(GetCurrentProcess(), id, GetCurrentProcess(), &pipe, 0, false, DUPLICATE_SAME_ACCESS);
	}

	virtual ~QPipeReaderThread()
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

	virtual void start()
	{
		pipe_set_blocking(pipe, true);
		QThread::start();
	}

	virtual void resume()
	{
		QMutexLocker locker(&m);
		pipe_set_blocking(pipe, true);
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
					else // will never happen
						result = -2;

					m.lock();
					active = false;
					pipe_set_blocking(pipe, false);
					m.unlock();

					emit canRead_p(result);
					break;
				}
			}
		}
	}

signals:
	void canRead_p(int result);
};

//----------------------------------------------------------------------------
// QPipeReaderPoll
//----------------------------------------------------------------------------
class QPipeReaderPoll : public QPipeReader
{
	Q_OBJECT
public:
	Q_PIPE_ID pipe;
	SafeTimer timer;
	bool consoleMode;

	QPipeReaderPoll(Q_PIPE_ID id, QObject *parent = 0) : QPipeReader(parent), timer(this)
	{
		pipe = id;
		connect(&timer, SIGNAL(timeout()), SLOT(tryRead()));
	}

	virtual ~QPipeReaderPoll()
	{
	}

	virtual void start()
	{
		pipe_set_blocking(pipe, false);
		consoleMode = pipe_is_a_console(pipe);
		resume();
	}

	virtual void resume()
	{
		timer.start(0);
	}

private slots:
	void tryRead()
	{
		if(consoleMode)
			tryReadConsole();
		else
			tryReadPipe();
	}

private:
	void tryReadPipe()
	{
		// is there data available for reading?  if so, signal.
		int bytes = pipe_read_avail(pipe);
		if(bytes > 0)
		{
			timer.stop();
			emit canRead(-3); // no readAhead
			return;
		}

		// no data available?  probe for EOF/error
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
			else // will never happen
				result = -2;

			timer.stop();
			emit canRead(result);
			return;
		}

		timer.setInterval(PIPEREADER_POLL);
	}

	void tryReadConsole()
	{
		// is there data available for reading?  if so, signal.
		int count = pipe_read_avail_console(pipe);
		if(count > 0)
		{
			timer.stop();
			emit canRead(-3); // no readAhead
			return;
		}

		timer.setInterval(PIPEREADER_POLL);
	}
};

// end of windows pipe writer/reader implementations

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
	int writeResult;
	int lastTaken, lastWritten;

#ifdef Q_OS_WIN
	bool atEnd, atError, forceNotify;
	int readAhead;
	SafeTimer *readTimer;
	QTextDecoder *dec;
	bool consoleMode;
	QPipeWriter *pipeWriter;
	QPipeReader *pipeReader;
#endif
#ifdef Q_OS_UNIX
	SafeSocketNotifier *sn_read, *sn_write;
#endif

	Private(QPipeDevice *_q) : QObject(_q), q(_q), pipe(INVALID_Q_PIPE_ID)
	{
#ifdef Q_OS_WIN
		readTimer = 0;
		pipeWriter = 0;
		pipeReader = 0;
		dec = 0;
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
		delete dec;
		dec = 0;
		consoleMode = false;
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
		writeResult = -1;
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
			// for windows, the blocking mode is chosen by the QPipeReader

			// console might need a decoder
			if(consoleMode)
			{
				QT_WA(
					dec = 0;
				,
					dec = QTextCodec::codecForLocale()->makeDecoder();
				)
			}

			// pipe reader
#ifdef USE_POLL
			pipeReader = new QPipeReaderPoll(pipe, this);
#else
			// console always polls, no matter what
			if(consoleMode)
				pipeReader = new QPipeReaderPoll(pipe, this);
			else
				pipeReader = new QPipeReaderThread(pipe, this);
#endif
			connect(pipeReader, SIGNAL(canRead(int)), this, SLOT(pr_canRead(int)));
			pipeReader->start();

			// polling timer
			readTimer = new SafeTimer(this);
			connect(readTimer, SIGNAL(timeout()), SLOT(t_timeout()));

			// updated: now that we have pipeReader, this no longer
			//   polls for data.  it only does delayed singleshot
			//   notifications.
			readTimer->setSingleShot(true);
#endif
#ifdef Q_OS_UNIX
			pipe_set_blocking(pipe, false);

			// socket notifier
			sn_read = new SafeSocketNotifier(pipe, QSocketNotifier::Read, this);
			connect(sn_read, SIGNAL(activated(int)), SLOT(sn_read_activated(int)));
#endif
		}
		else
		{
			// for windows, the blocking mode is chosen by the QPipeWriter
#ifdef Q_OS_UNIX
			pipe_set_blocking(pipe, false);

			// socket notifier
			sn_write = new SafeSocketNotifier(pipe, QSocketNotifier::Write, this);
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
#endif
	}

	void pw_canWrite(int result, int bytesWritten)
	{
#ifdef Q_OS_WIN
		if(result == 0)
		{
			writeResult = 0;
			lastWritten = lastTaken; // success means all bytes
		}
		else
		{
			writeResult = -1;
			lastWritten = bytesWritten;
		}

		canWrite = true;
		emit q->notify();
#else
		Q_UNUSED(result);
		Q_UNUSED(bytesWritten);
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
		else if(result != -3)
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
		writeResult = 0;
		lastWritten = lastTaken;

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
	return (int)dw; // FIXME? assumes handle value fits in signed int
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
#ifdef Q_OS_WIN
	d->consoleMode = pipe_is_a_console(d->pipe);
#endif
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

bool QPipeDevice::setInheritable(bool enabled)
{
#ifdef Q_OS_WIN
	Q_PIPE_ID newPipe;
	if(!pipe_set_inheritable(d->pipe, enabled, &newPipe))
		return false;
	d->pipe = newPipe;
#ifdef USE_POLL
	if(d->pipeReader)
		static_cast<QPipeReaderPoll*>(d->pipeReader)->pipe = d->pipe;
	if(d->pipeWriter)
		static_cast<QPipeWriterPoll*>(d->pipeWriter)->pipe = d->pipe;
#endif
	return true;
#endif
#ifdef Q_OS_UNIX
	return pipe_set_inheritable(d->pipe, enabled, 0);
#endif
}

int QPipeDevice::bytesAvailable() const
{
	int n;
#ifdef Q_OS_WIN
	if(d->consoleMode)
		n = pipe_read_avail_console(d->pipe);
	else
		n = pipe_read_avail(d->pipe);
	if(d->readAhead != -1)
		++n;
#else
	n = pipe_read_avail(d->pipe);
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
	// for windows console:
	// the number of bytes in utf8 can exceed the number of actual
	//   characters it represents.  to be safe, we'll assume that
	//   utf8 could outnumber characters X:1.  this does mean that
	//   the maxsize parameter needs to be at least X to do
	//   anything.  (X = CONSOLE_CHAREXPAND)
	if(d->consoleMode && maxsize < CONSOLE_CHAREXPAND)
		return -1;

	// for resuming the pipeReader
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
	int ret;
	if(d->consoleMode)
	{
		// read a fraction of the number of characters as requested,
		//   to guarantee the result fits
		int num = size / CONSOLE_CHAREXPAND;

#ifdef QPIPE_SECURE
		SecureArray destbuf(num * sizeof(ushort), 0);
#else
		QByteArray destbuf(num * sizeof(ushort), 0);
#endif
		ushort *dest = (ushort *)destbuf.data();

		ret = pipe_read_console(d->pipe, dest, num, &done, d->dec);
		if(ret != -1)
		{
			// for security, encode one character at a time without
			//   performing a QString conversion of the whole thing
			QTextCodec *codec = QTextCodec::codecForMib(106);
			QTextCodec::ConverterState cstate(QTextCodec::IgnoreHeader);
			int at = 0;
			for(int n = 0; n < ret; ++n)
			{
				QChar c(dest[n]);
				QByteArray out = codec->fromUnicode(&c, 1, &cstate);
				memcpy(data + offset + at, out.data(), out.size());
				at += out.size();
			}
			ret = at; // change ret to actual bytes
		}
	}
	else
		ret = pipe_read(d->pipe, data + offset, size, &done);
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

			// readTimer is a singleshot, so we have to start it
			//   for forceNotify to work
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
#ifdef USE_POLL
		d->pipeWriter = new QPipeWriterPoll(d->pipe, d);
#else
		// console always polls, no matter what
		if(d->consoleMode)
			d->pipeWriter = new QPipeReaderPoll(d->pipe, d);
		else
			d->pipeWriter = new QPipeWriterThread(d->pipe, d);
#endif
		connect(d->pipeWriter, SIGNAL(canWrite(int, int)), d, SLOT(pw_canWrite(int, int)));
		d->pipeWriter->start();
	}

	if(d->consoleMode)
	{
		// Note: we convert to QString here, but it should not be a
		//   security issue (see pipe_write_console comment above)

		// for console, just write direct.  we won't use pipewriter
		QString out = QString::fromUtf8(QByteArray(data, size));
		r = pipe_write_console(d->pipe, out.utf16(), out.length());
		if(r == -1)
			return -1;

		// convert characters to bytes
		r = out.mid(0, r).toUtf8().size();

		// simulate.  we invoke the signal of pipewriter rather than our
		//   own slot, so that the invoke can be cancelled.
		d->canWrite = false;
		QMetaObject::invokeMethod(d->pipeWriter, "canWrite", Qt::QueuedConnection, Q_ARG(int, 0), Q_ARG(int, r));
	}
	else
	{
		d->canWrite = false;
		r = d->pipeWriter->write(data, size);
	}

	d->lastTaken = r;
	if(r == -1)
	{
		close();
		return -1;
	}
#endif
#ifdef Q_OS_UNIX
	r = pipe_write(d->pipe, data, size);
	d->lastTaken = r;
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

int QPipeDevice::writeResult(int *written) const
{
	if(written)
		*written = d->lastWritten;
	return d->writeResult;
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

#ifdef Q_OS_WIN
	bool consoleMode;
#endif

#ifdef QPIPE_SECURE
	bool secure;
	SecureArray sec_buf;
	SecureArray sec_curWrite;
#endif
	SafeTimer readTrigger, writeTrigger, closeTrigger, writeErrorTrigger;
	bool canRead, activeWrite;
	int lastWrite;
	bool closeLater;
	bool closing;

	Private(QPipeEnd *_q) : QObject(_q), q(_q), pipe(this), readTrigger(this), writeTrigger(this), closeTrigger(this), writeErrorTrigger(this)
	{
		readTrigger.setSingleShot(true);
		writeTrigger.setSingleShot(true);
		closeTrigger.setSingleShot(true);
		writeErrorTrigger.setSingleShot(true);
		connect(&pipe, SIGNAL(notify()), SLOT(pipe_notify()));
		connect(&readTrigger, SIGNAL(timeout()), SLOT(doRead()));
		connect(&writeTrigger, SIGNAL(timeout()), SLOT(doWrite()));
		connect(&closeTrigger, SIGNAL(timeout()), SLOT(doClose()));
		connect(&writeErrorTrigger, SIGNAL(timeout()), SLOT(doWriteError()));
		reset(ResetSessionAndData);
	}

	void reset(ResetMode mode)
	{
		pipe.close();
		readTrigger.stop();
		writeTrigger.stop();
		closeTrigger.stop();
		writeErrorTrigger.stop();
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
#ifdef Q_OS_WIN
		consoleMode = pipe_is_a_console(id);
#endif
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
	void appendArray(SecureArray *a, const SecureArray &b)
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
	void takeArray(SecureArray *a, int len)
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
	SecureArray readSecure(SecureArray *buf, int bytes)
	{
		SecureArray a;
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

	void writeSecure(SecureArray *buf, const SecureArray &a)
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
			int x;
			int writeResult = pipe.writeResult(&x);
			if(writeResult == -1)
				lastWrite = x; // if error, we may have written less bytes

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

			x = lastWrite;
			lastWrite = 0;

			if(writeResult == 0)
			{
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
			}
			else
				writeErrorTrigger.start();

			if(x > 0)
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

		int max;
#ifdef Q_OS_WIN
		if(consoleMode)
		{
			// need a minimum amount for console
			if(left < CONSOLE_CHAREXPAND)
			{
				canRead = true;
				return;
			}

			// don't use pipe.bytesAvailable() for console mode,
			//   as it is somewhat bogus.  fortunately, there is
			//   no problem with overreading from the console.
			max = qMin(left, 32);
		}
		else
#endif
		{
			max = qMin(left, pipe.bytesAvailable());
		}

		int ret;
#ifdef QPIPE_SECURE
		if(secure)
		{
			SecureArray a(max);
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
			QByteArray a(max, 0);
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

	void doWriteError()
	{
		reset(ResetSession);
		emit q->error(QPipeEnd::ErrorBroken);
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

int QPipeEnd::idAsInt() const
{
	return d->pipe.idAsInt();
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

bool QPipeEnd::setInheritable(bool enabled)
{
	return d->pipe.setInheritable(enabled);
}

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
SecureArray QPipeEnd::readSecure(int bytes)
{
	return d->readSecure(&d->sec_buf, bytes);
}

void QPipeEnd::writeSecure(const SecureArray &buf)
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
SecureArray QPipeEnd::takeBytesToWriteSecure()
{
	// only call this on inactive sessions
	if(isValid())
		return SecureArray();

	SecureArray a = d->sec_buf;
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
	secAttr.bInheritHandle = false;

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
	if(!pipe_set_inheritable(p[0], false, 0) ||
		!pipe_set_inheritable(p[1], false, 0))
	{
		close(p[0]);
		close(p[1]);
		return false;
	}
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
