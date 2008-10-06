/*
 * Copyright (C) 2006,2007  Justin Karneges <justin@affinix.com>
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

#include "qca_support.h"

#include "qpipe.h"
#include "qca_safeobj.h"

#include <QPointer>
#include <QTextCodec>
#include <QMutex>

#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <sys/termios.h>
# include <unistd.h>
# include <fcntl.h>
# include <stdlib.h>
#endif

#include <stdio.h>

#define CONSOLEPROMPT_INPUT_MAX 56

Q_DECLARE_METATYPE(QCA::SecureArray)

namespace QCA {

//----------------------------------------------------------------------------
// ConsoleWorker
//----------------------------------------------------------------------------
class ConsoleWorker : public QObject
{
	Q_OBJECT
private:
	QPipeEnd in, out;
	bool started;
	QByteArray in_left, out_left;

public:
	ConsoleWorker(QObject *parent = 0) : QObject(parent), in(this), out(this)
	{
		started = false;
	}

	~ConsoleWorker()
	{
		stop();
	}

	void start(Q_PIPE_ID in_id, Q_PIPE_ID out_id)
	{
		Q_ASSERT(!started);

		if(in_id != INVALID_Q_PIPE_ID)
		{
			in.take(in_id, QPipeDevice::Read);
			connect(&in, SIGNAL(readyRead()), SLOT(in_readyRead()));
			connect(&in, SIGNAL(closed()), SLOT(in_closed()));
			connect(&in, SIGNAL(error(QCA::QPipeEnd::Error)), SLOT(in_error(QCA::QPipeEnd::Error)));
			in.enable();
		}

		if(out_id != INVALID_Q_PIPE_ID)
		{
			out.take(out_id, QPipeDevice::Write);
			connect(&out, SIGNAL(bytesWritten(int)), SLOT(out_bytesWritten(int)));
			connect(&out, SIGNAL(closed()), SLOT(out_closed()));
			out.enable();
		}

		started = true;
	}

	void stop()
	{
		if(!started)
			return;

		if(in.isValid())
			in.finalizeAndRelease();
		if(out.isValid())
			out.release();

		in_left = in.read();
		out_left = out.takeBytesToWrite();

		started = false;
	}

public slots:
	bool isValid() const
	{
		return in.isValid();
	}

	void setSecurityEnabled(bool enabled)
	{
		if(in.isValid())
			in.setSecurityEnabled(enabled);
		if(out.isValid())
			out.setSecurityEnabled(enabled);
	}

	QByteArray read(int bytes = -1)
	{
		return in.read(bytes);
	}

	void write(const QByteArray &a)
	{
		out.write(a);
	}

	QCA::SecureArray readSecure(int bytes = -1)
	{
		return in.readSecure(bytes);
	}

	void writeSecure(const QCA::SecureArray &a)
	{
		out.writeSecure(a);
	}

	void closeOutput()
	{
		out.close();
	}

	int bytesAvailable() const
	{
		return in.bytesAvailable();
	}

	int bytesToWrite() const
	{
		return in.bytesToWrite();
	}

public:
	QByteArray takeBytesToRead()
	{
		QByteArray a = in_left;
		in_left.clear();
		return a;
	}

	QByteArray takeBytesToWrite()
	{
		QByteArray a = out_left;
		out_left.clear();
		return a;
	}

signals:
	void readyRead();
	void bytesWritten(int bytes);
	void inputClosed();
	void outputClosed();

private slots:
	void in_readyRead()
	{
		emit readyRead();
	}

	void out_bytesWritten(int bytes)
	{
		emit bytesWritten(bytes);
	}

	void in_closed()
	{
		emit inputClosed();
	}

	void in_error(QCA::QPipeEnd::Error)
	{
		emit inputClosed();
	}

	void out_closed()
	{
		emit outputClosed();
	}
};

//----------------------------------------------------------------------------
// ConsoleThread
//----------------------------------------------------------------------------
class ConsoleThread : public SyncThread
{
	Q_OBJECT
public:
	ConsoleWorker *worker;
	Q_PIPE_ID _in_id, _out_id;
	QByteArray in_left, out_left;
	QMutex call_mutex;

	ConsoleThread(QObject *parent = 0) : SyncThread(parent)
	{
		qRegisterMetaType<SecureArray>("QCA::SecureArray");
	}

	~ConsoleThread()
	{
		stop();
	}

	void start(Q_PIPE_ID in_id, Q_PIPE_ID out_id)
	{
		_in_id = in_id;
		_out_id = out_id;
		SyncThread::start();
	}

	void stop()
	{
		SyncThread::stop();
	}

	QVariant mycall(QObject *obj, const char *method, const QVariantList &args = QVariantList())
	{
		QVariant ret;
		bool ok;

		call_mutex.lock();
		ret = call(obj, method, args, &ok);
		call_mutex.unlock();

		Q_ASSERT(ok);
		if(!ok)
		{
			fprintf(stderr, "QCA: ConsoleWorker call [%s] failed.\n", method);
			abort();
			return QVariant();
		}
		return ret;
	}

	bool isValid()
	{
		return mycall(worker, "isValid").toBool();
	}

	void setSecurityEnabled(bool enabled)
	{
		mycall(worker, "setSecurityEnabled", QVariantList() << enabled);
	}

	QByteArray read(int bytes = -1)
	{
		return mycall(worker, "read", QVariantList() << bytes).toByteArray();
	}

	void write(const QByteArray &a)
	{
		mycall(worker, "write", QVariantList() << a);
	}

	SecureArray readSecure(int bytes = -1)
	{
		return qVariantValue<SecureArray>(mycall(worker, "readSecure", QVariantList() << bytes));
	}

	void writeSecure(const SecureArray &a)
	{
		mycall(worker, "writeSecure", QVariantList() << qVariantFromValue<SecureArray>(a));
	}

	void closeOutput()
	{
		mycall(worker, "closeOutput");
	}

	int bytesAvailable()
	{
		return mycall(worker, "bytesAvailable").toInt();
	}

	int bytesToWrite()
	{
		return mycall(worker, "bytesToWrite").toInt();
	}

	QByteArray takeBytesToRead()
	{
		QByteArray a = in_left;
		in_left.clear();
		return a;
	}

	QByteArray takeBytesToWrite()
	{
		QByteArray a = out_left;
		out_left.clear();
		return a;
	}

signals:
	void readyRead();
	void bytesWritten(int);
	void inputClosed();
	void outputClosed();

protected:
	virtual void atStart()
	{
		worker = new ConsoleWorker;

		// use direct connections here, so that the emits come from
		//   the other thread.  we can also connect to our own
		//   signals to avoid having to make slots just to emit.
		connect(worker, SIGNAL(readyRead()), SIGNAL(readyRead()), Qt::DirectConnection);
		connect(worker, SIGNAL(bytesWritten(int)), SIGNAL(bytesWritten(int)), Qt::DirectConnection);
		connect(worker, SIGNAL(inputClosed()), SIGNAL(inputClosed()), Qt::DirectConnection);
		connect(worker, SIGNAL(outputClosed()), SIGNAL(outputClosed()), Qt::DirectConnection);

		worker->start(_in_id, _out_id);
	}

	virtual void atEnd()
	{
		in_left = worker->takeBytesToRead();
		out_left = worker->takeBytesToWrite();
		delete worker;
	}
};

//----------------------------------------------------------------------------
// Console
//----------------------------------------------------------------------------
class ConsolePrivate : public QObject
{
	Q_OBJECT
public:
	Console *q;

	bool started;
	Console::Type type;
	Console::ChannelMode cmode;
	Console::TerminalMode mode;
	ConsoleThread *thread;
	ConsoleReference *ref;
	Q_PIPE_ID in_id;

#ifdef Q_OS_WIN
	DWORD old_mode;
#else
	struct termios old_term_attr;
#endif

	ConsolePrivate(Console *_q) : QObject(_q), q(_q)
	{
		started = false;
		mode = Console::Default;
		thread = new ConsoleThread(this);
		ref = 0;
	}

	~ConsolePrivate()
	{
		delete thread;
		setInteractive(Console::Default);
	}

	void setInteractive(Console::TerminalMode m)
	{
		// no change
		if(m == mode)
			return;

		if(m == Console::Interactive)
		{
#ifdef Q_OS_WIN
			GetConsoleMode(in_id, &old_mode);
			SetConsoleMode(in_id, old_mode & (~ENABLE_LINE_INPUT & ~ENABLE_ECHO_INPUT));
#else
			int fd = in_id;
			struct termios attr;
			tcgetattr(fd, &attr);
			old_term_attr = attr;

			attr.c_lflag &= ~(ECHO);    // turn off the echo flag
			attr.c_lflag &= ~(ICANON);  // no wait for a newline
			attr.c_cc[VMIN] = 1;        // read at least 1 char
			attr.c_cc[VTIME] = 0;       // set wait time to zero

			// set the new attributes
			tcsetattr(fd, TCSAFLUSH, &attr);
#endif
		}
		else
		{
#ifdef Q_OS_WIN
			SetConsoleMode(in_id, old_mode);
#else
			int fd = in_id;
			tcsetattr(fd, TCSANOW, &old_term_attr);
#endif
		}

		mode = m;
	}
};

static Console *g_tty_console = 0, *g_stdio_console = 0;

Console::Console(Type type, ChannelMode cmode, TerminalMode tmode, QObject *parent)
:QObject(parent)
{
	if(type == Tty)
	{
		Q_ASSERT(g_tty_console == 0);
		g_tty_console = this;
	}
	else
	{
		Q_ASSERT(g_stdio_console == 0);
		g_stdio_console = this;
	}

	d = new ConsolePrivate(this);
	d->type = type;
	d->cmode = cmode;

	Q_PIPE_ID in = INVALID_Q_PIPE_ID;
	Q_PIPE_ID out = INVALID_Q_PIPE_ID;

#ifdef Q_OS_WIN
	if(type == Tty)
	{
		in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, false,
			OPEN_EXISTING, 0, NULL);
	}
	else
	{
		in = GetStdHandle(STD_INPUT_HANDLE);
	}
#else
	if(type == Tty)
	{
		in = open("/dev/tty", O_RDONLY);
	}
	else
	{
		in = 0; // stdin
	}
#endif
	if(cmode == ReadWrite)
	{
#ifdef Q_OS_WIN
		if(type == Tty)
		{
			out = CreateFileA("CONOUT$",
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, false,
				OPEN_EXISTING, 0, NULL);
		}
		else
		{
			out = GetStdHandle(STD_OUTPUT_HANDLE);
		}
#else
		if(type == Tty)
		{
			out = open("/dev/tty", O_WRONLY);
		}
		else
		{
			out = 1; // stdout
		}
#endif
	}

	d->in_id = in;
	d->setInteractive(tmode);
	d->thread->start(in, out);
}

Console::~Console()
{
	release();
	Console::Type type = d->type;
	delete d;
	if(type == Tty)
		g_tty_console = 0;
	else
		g_stdio_console = 0;
}

Console::Type Console::type() const
{
	return d->type;
}

Console::ChannelMode Console::channelMode() const
{
	return d->cmode;
}

Console::TerminalMode Console::terminalMode() const
{
	return d->mode;
}

bool Console::isStdinRedirected()
{
#ifdef Q_OS_WIN
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	if(GetConsoleMode(h, &mode))
		return false;
	return true;
#else
	return (isatty(0) ? false : true); // 0 == stdin
#endif
}

bool Console::isStdoutRedirected()
{
#ifdef Q_OS_WIN
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode;
	if(GetConsoleMode(h, &mode))
		return false;
	return true;
#else
	return (isatty(1) ? false : true); // 1 == stdout
#endif
}

Console *Console::ttyInstance()
{
	return g_tty_console;
}

Console *Console::stdioInstance()
{
	return g_stdio_console;
}

void Console::release()
{
	d->thread->stop();
}

QByteArray Console::bytesLeftToRead()
{
	return d->thread->takeBytesToRead();
}

QByteArray Console::bytesLeftToWrite()
{
	return d->thread->takeBytesToWrite();
}

//----------------------------------------------------------------------------
// ConsoleReference
//----------------------------------------------------------------------------
class ConsoleReferencePrivate : public QObject
{
	Q_OBJECT
public:
	ConsoleReference *q;

	Console *console;
	ConsoleThread *thread;
	ConsoleReference::SecurityMode smode;
	SafeTimer lateTrigger;
	bool late_read, late_close;

	ConsoleReferencePrivate(ConsoleReference *_q) : QObject(_q), q(_q), lateTrigger(this)
	{
		console = 0;
		thread = 0;
		connect(&lateTrigger, SIGNAL(timeout()), SLOT(doLate()));
		lateTrigger.setSingleShot(true);
	}

private slots:
	void doLate()
	{
		QPointer<QObject> self = this;
		if(late_read)
			emit q->readyRead();
		if(!self)
			return;
		if(late_close)
			emit q->inputClosed();
	}
};

ConsoleReference::ConsoleReference(QObject *parent)
:QObject(parent)
{
	d = new ConsoleReferencePrivate(this);
}

ConsoleReference::~ConsoleReference()
{
	stop();
	delete d;
}

bool ConsoleReference::start(Console *console, SecurityMode mode)
{
	// make sure this reference isn't using a console already
	Q_ASSERT(!d->console);

	// one console reference at a time
	Q_ASSERT(console->d->ref == 0);

	// let's take it
	d->console = console;
	d->thread = d->console->d->thread;
	d->console->d->ref = this;

	bool valid = d->thread->isValid();
	int avail = d->thread->bytesAvailable();

	// pipe already closed and no data?  consider this an error
	if(!valid && avail == 0)
	{
		d->console->d->ref = 0;
		d->thread = 0;
		d->console = 0;
		return false;
	}

	// enable security?  it will last for this active session only
	d->smode = mode;
	if(mode == SecurityEnabled)
		d->thread->setSecurityEnabled(true);

	connect(d->thread, SIGNAL(readyRead()), SIGNAL(readyRead()));
	connect(d->thread, SIGNAL(bytesWritten(int)), SIGNAL(bytesWritten(int)));
	connect(d->thread, SIGNAL(inputClosed()), SIGNAL(inputClosed()));
	connect(d->thread, SIGNAL(outputClosed()), SIGNAL(outputClosed()));

	d->late_read = false;
	d->late_close = false;

	if(avail > 0)
		d->late_read = true;

	if(!valid)
		d->late_close = true;

	if(d->late_read || d->late_close)
		d->lateTrigger.start();

	return true;
}

void ConsoleReference::stop()
{
	if(!d->console)
		return;

	d->lateTrigger.stop();

	disconnect(d->thread, 0, this, 0);

	// automatically disable security when we go inactive
	d->thread->setSecurityEnabled(false);

	d->console->d->ref = 0;
	d->thread = 0;
	d->console = 0;
}

Console *ConsoleReference::console() const
{
	return d->console;
}

ConsoleReference::SecurityMode ConsoleReference::securityMode() const
{
	return d->smode;
}

QByteArray ConsoleReference::read(int bytes)
{
	return d->thread->read(bytes);
}

void ConsoleReference::write(const QByteArray &a)
{
	d->thread->write(a);
}

SecureArray ConsoleReference::readSecure(int bytes)
{
	return d->thread->readSecure(bytes);
}

void ConsoleReference::writeSecure(const SecureArray &a)
{
	d->thread->writeSecure(a);
}

void ConsoleReference::closeOutput()
{
	d->thread->closeOutput();
}

int ConsoleReference::bytesAvailable() const
{
	return d->thread->bytesAvailable();
}

int ConsoleReference::bytesToWrite() const
{
	return d->thread->bytesToWrite();
}

//----------------------------------------------------------------------------
// ConsolePrompt
//----------------------------------------------------------------------------
class ConsolePrompt::Private : public QObject
{
	Q_OBJECT
public:
	ConsolePrompt *q;

	Synchronizer sync;
	Console *con;
	bool own_con;
	ConsoleReference console;
	QString promptStr;
	SecureArray result;
	bool waiting;
	int at;
	bool done;
	bool charMode;
	QTextCodec *codec;
	QTextCodec::ConverterState *encstate, *decstate;

	Private(ConsolePrompt *_q) : QObject(_q), q(_q), sync(_q), console(this)
	{
		connect(&console, SIGNAL(readyRead()), SLOT(con_readyRead()));
		connect(&console, SIGNAL(inputClosed()), SLOT(con_inputClosed()));

		con = 0;
		own_con = false;
		waiting = false;

#ifdef Q_OS_WIN
		codec = QTextCodec::codecForMib(106); // UTF-8
#else
		codec = QTextCodec::codecForLocale();
#endif
		encstate = 0;
		decstate = 0;
	}

	~Private()
	{
		reset();
	}

	void reset()
	{
		delete encstate;
		encstate = 0;
		delete decstate;
		decstate = 0;

		console.stop();
		if(own_con)
		{
			delete con;
			con = 0;
			own_con = false;
		}
	}

	bool start(bool _charMode)
	{
		own_con = false;
		con = Console::ttyInstance();
		if(!con)
		{
			con = new Console(Console::Tty, Console::ReadWrite, Console::Interactive);
			own_con = true;
		}

		result.clear();
		at = 0;
		done = false;
		charMode = _charMode;

		encstate = new QTextCodec::ConverterState(QTextCodec::IgnoreHeader);
		decstate = new QTextCodec::ConverterState(QTextCodec::IgnoreHeader);

		if(!console.start(con, ConsoleReference::SecurityEnabled))
		{
			reset();
			fprintf(stderr, "Console input not available or closed\n");
			return false;
		}

		if(!charMode)
			writeString(promptStr + ": ");

		return true;
	}

	void writeString(const QString &str)
	{
		console.writeSecure(codec->fromUnicode(str.unicode(), str.length(), encstate));
	}

	// process each char.  internally store the result as utf16, which
	//   is easier to edit (e.g. backspace)
	bool processChar(QChar c)
	{
		if(charMode)
		{
			appendChar(c);
			done = true;
			return false;
		}

		if(c == '\r' || c == '\n')
		{
			writeString("\n");
			done = true;
			return false;
		}

		if(c == '\b' || c == 0x7f)
		{
			if(at > 0)
			{
				--at;
				writeString("\b \b");
				result.resize(at * sizeof(ushort));
			}
			return true;
		}
		else if(c < 0x20)
			return true;

		if(at >= CONSOLEPROMPT_INPUT_MAX)
			return true;

		appendChar(c);

		writeString("*");
		return true;
	}

	void appendChar(QChar c)
	{
		if((at + 1) * (int)sizeof(ushort) > result.size())
			result.resize((at + 1) * sizeof(ushort));
		ushort *p = (ushort *)result.data();
		p[at++] = c.unicode();
	}

	void convertToUtf8()
	{
		// convert result from utf16 to utf8, securely
		QTextCodec *codec = QTextCodec::codecForMib(106);
		QTextCodec::ConverterState cstate(QTextCodec::IgnoreHeader);
		SecureArray out;
		ushort *ustr = (ushort *)result.data();
		int len = result.size() / sizeof(ushort);
		for(int n = 0; n < len; ++n)
		{
			QChar c(ustr[n]);
			out += codec->fromUnicode(&c, 1, &cstate);
		}
		result = out;
	}

private slots:
	void con_readyRead()
	{
		while(console.bytesAvailable() > 0)
		{
			SecureArray buf = console.readSecure(1);
			if(buf.isEmpty())
				break;

			// convert to unicode and process
			QString str = codec->toUnicode(buf.data(), 1, decstate);
			bool quit = false;
			for(int n = 0; n < str.length(); ++n)
			{
				if(!processChar(str[n]))
				{
					quit = true;
					break;
				}
			}
			if(quit)
				break;
		}

		if(done)
		{
			convertToUtf8();

			reset();
			if(waiting)
				sync.conditionMet();
			else
				emit q->finished();
		}
	}

	void con_inputClosed()
	{
		fprintf(stderr, "Console input closed\n");
		if(!done)
		{
			done = true;
			result.clear();

			reset();
			if(waiting)
				sync.conditionMet();
			else
				emit q->finished();
		}
	}
};

ConsolePrompt::ConsolePrompt(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

ConsolePrompt::~ConsolePrompt()
{
	delete d;
}

void ConsolePrompt::getHidden(const QString &promptStr)
{
	d->reset();

	d->promptStr = promptStr;
	if(!d->start(false))
	{
		QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
		return;
	}
}

void ConsolePrompt::getChar()
{
	d->reset();

	if(!d->start(true))
	{
		QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
		return;
	}
}

void ConsolePrompt::waitForFinished()
{
	// reparent the Console under us (for Synchronizer)
	QObject *orig_parent = d->con->parent();
	d->con->setParent(this);

	// block while prompting
	d->waiting = true;
	d->sync.waitForCondition();
	d->waiting = false;

	// restore parent (if con still exists)
	if(d->con)
		d->con->setParent(orig_parent);
}

SecureArray ConsolePrompt::result() const
{
	return d->result;
}

QChar ConsolePrompt::resultChar() const
{
	QString str = QString::fromUtf8(d->result.toByteArray());

	// this will never happen if getChar completes
	if(str.isEmpty())
		return QChar();

	return str[0];
}

}

#include "console.moc"
