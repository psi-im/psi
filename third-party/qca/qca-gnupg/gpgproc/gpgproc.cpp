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

#include "gpgproc.h"

#include "sprocess.h"

#ifdef Q_OS_MAC
#define QT_PIPE_HACK
#endif

#define QPROC_SIGNAL_RELAY

using namespace QCA;

namespace gpgQCAPlugin {

void releaseAndDeleteLater(QObject *owner, QObject *obj)
{
	obj->disconnect(owner);
	obj->setParent(0);
	obj->deleteLater();
}

//----------------------------------------------------------------------------
// SafeTimer
//----------------------------------------------------------------------------
SafeTimer::SafeTimer(QObject *parent) :
	QObject(parent)
{
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SIGNAL(timeout()));
}

SafeTimer::~SafeTimer()
{
	releaseAndDeleteLater(this, timer);
}

int SafeTimer::interval() const
{
	return timer->interval();
}

bool SafeTimer::isActive() const
{
	return timer->isActive();
}

bool SafeTimer::isSingleShot() const
{
	return timer->isSingleShot();
}

void SafeTimer::setInterval(int msec)
{
	timer->setInterval(msec);
}

void SafeTimer::setSingleShot(bool singleShot)
{
	timer->setSingleShot(singleShot);
}

int SafeTimer::timerId() const
{
	return timer->timerId();
}

void SafeTimer::start(int msec)
{
	timer->start(msec);
}

void SafeTimer::start()
{
	timer->start();
}

void SafeTimer::stop()
{
	timer->stop();
}

//----------------------------------------------------------------------------
// QProcessSignalRelay
//----------------------------------------------------------------------------
class QProcessSignalRelay : public QObject
{
	Q_OBJECT
public:
	QProcessSignalRelay(QProcess *proc, QObject *parent = 0)
	:QObject(parent)
	{
		qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
		connect(proc, SIGNAL(started()), SLOT(proc_started()), Qt::QueuedConnection);
		connect(proc, SIGNAL(readyReadStandardOutput()), SLOT(proc_readyReadStandardOutput()), Qt::QueuedConnection);
		connect(proc, SIGNAL(readyReadStandardError()), SLOT(proc_readyReadStandardError()), Qt::QueuedConnection);
		connect(proc, SIGNAL(bytesWritten(qint64)), SLOT(proc_bytesWritten(qint64)), Qt::QueuedConnection);
		connect(proc, SIGNAL(finished(int)), SLOT(proc_finished(int)), Qt::QueuedConnection);
		connect(proc, SIGNAL(error(QProcess::ProcessError)), SLOT(proc_error(QProcess::ProcessError)), Qt::QueuedConnection);
	}

signals:
	void started();
	void readyReadStandardOutput();
	void readyReadStandardError();
	void bytesWritten(qint64);
	void finished(int);
	void error(QProcess::ProcessError);

public slots:
	void proc_started()
	{
		emit started();
	}

	void proc_readyReadStandardOutput()
	{
		emit readyReadStandardOutput();
	}

	void proc_readyReadStandardError()
	{
		emit readyReadStandardError();
	}

	void proc_bytesWritten(qint64 x)
	{
		emit bytesWritten(x);
	}

	void proc_finished(int x)
	{
		emit finished(x);
	}

	void proc_error(QProcess::ProcessError x)
	{
		emit error(x);
	}
};

//----------------------------------------------------------------------------
// GPGProc
//----------------------------------------------------------------------------
enum ResetMode
{
	ResetSession        = 0,
	ResetSessionAndData = 1,
	ResetAll            = 2
};

class GPGProc::Private : public QObject
{
	Q_OBJECT
public:
	GPGProc *q;
	QString bin;
	QStringList args;
	GPGProc::Mode mode;
	SProcess *proc;
#ifdef QPROC_SIGNAL_RELAY
	QProcessSignalRelay *proc_relay;
#endif
	QPipe pipeAux, pipeCommand, pipeStatus;
	QByteArray statusBuf;
	QStringList statusLines;
	GPGProc::Error error;
	int exitCode;
	SafeTimer startTrigger, doneTrigger;

	QByteArray pre_stdin, pre_aux;
#ifdef QPIPE_SECURE
	SecureArray pre_command;
#else
	QByteArray pre_command;
#endif
	bool pre_stdin_close, pre_aux_close, pre_command_close;

	bool need_status, fin_process, fin_process_success, fin_status;
	QByteArray leftover_stdout;
	QByteArray leftover_stderr;

	Private(GPGProc *_q) : QObject(_q), q(_q), pipeAux(this), pipeCommand(this), pipeStatus(this), startTrigger(this), doneTrigger(this)
	{
		qRegisterMetaType<gpgQCAPlugin::GPGProc::Error>("gpgQCAPlugin::GPGProc::Error");

		proc = 0;
#ifdef QPROC_SIGNAL_RELAY
		proc_relay = 0;
#endif
		startTrigger.setSingleShot(true);
		doneTrigger.setSingleShot(true);

		connect(&pipeAux.writeEnd(), SIGNAL(bytesWritten(int)), SLOT(aux_written(int)));
		connect(&pipeAux.writeEnd(), SIGNAL(error(QCA::QPipeEnd::Error)), SLOT(aux_error(QCA::QPipeEnd::Error)));
		connect(&pipeCommand.writeEnd(), SIGNAL(bytesWritten(int)), SLOT(command_written(int)));
		connect(&pipeCommand.writeEnd(), SIGNAL(error(QCA::QPipeEnd::Error)), SLOT(command_error(QCA::QPipeEnd::Error)));
		connect(&pipeStatus.readEnd(), SIGNAL(readyRead()), SLOT(status_read()));
		connect(&pipeStatus.readEnd(), SIGNAL(error(QCA::QPipeEnd::Error)), SLOT(status_error(QCA::QPipeEnd::Error)));
		connect(&startTrigger, SIGNAL(timeout()), SLOT(doStart()));
		connect(&doneTrigger, SIGNAL(timeout()), SLOT(doTryDone()));

		reset(ResetSessionAndData);
	}

	~Private()
	{
		reset(ResetSession);
	}

	void closePipes()
	{
#ifdef QT_PIPE_HACK
		pipeAux.readEnd().reset();
		pipeCommand.readEnd().reset();
		pipeStatus.writeEnd().reset();
#endif

		pipeAux.reset();
		pipeCommand.reset();
		pipeStatus.reset();
	}

	void reset(ResetMode mode)
	{
#ifndef QT_PIPE_HACK
		closePipes();
#endif

		if(proc)
		{
			proc->disconnect(this);
			if(proc->state() != QProcess::NotRunning)
				proc->terminate();
			proc->setParent(0);
#ifdef QPROC_SIGNAL_RELAY
			releaseAndDeleteLater(this, proc_relay);
			proc_relay = 0;
			delete proc; // should be safe to do thanks to relay
#else
			proc->deleteLater();
#endif
			proc = 0;
		}

#ifdef QT_PIPE_HACK
		closePipes();
#endif

		startTrigger.stop();
		doneTrigger.stop();

		pre_stdin.clear();
		pre_aux.clear();
		pre_command.clear();
		pre_stdin_close = false;
		pre_aux_close = false;
		pre_command_close = false;

		need_status = false;
		fin_process = false;
		fin_status = false;

		if(mode >= ResetSessionAndData)
		{
			statusBuf.clear();
			statusLines.clear();
			leftover_stdout.clear();
			leftover_stderr.clear();
			error = GPGProc::FailedToStart;
			exitCode = -1;
		}
	}

	bool setupPipes(bool makeAux)
	{
		if(makeAux && !pipeAux.create())
		{
			closePipes();
			emit q->debug("Error creating pipeAux");
			return false;
		}

#ifdef QPIPE_SECURE
		if(!pipeCommand.create(true)) // secure
#else
		if(!pipeCommand.create())
#endif
		{
			closePipes();
			emit q->debug("Error creating pipeCommand");
			return false;
		}

		if(!pipeStatus.create())
		{
			closePipes();
			emit q->debug("Error creating pipeStatus");
			return false;
		}

		return true;
	}

	void setupArguments()
	{
		QStringList fullargs;
		fullargs += "--no-tty";

		if(mode == ExtendedMode)
		{
			fullargs += "--enable-special-filenames";

			fullargs += "--status-fd";
			fullargs += QString::number(pipeStatus.writeEnd().idAsInt());

			fullargs += "--command-fd";
			fullargs += QString::number(pipeCommand.readEnd().idAsInt());
		}

		for(int n = 0; n < args.count(); ++n)
		{
			QString a = args[n];
			if(mode == ExtendedMode && a == "-&?")
				fullargs += QString("-&") + QString::number(pipeAux.readEnd().idAsInt());
			else
				fullargs += a;
		}

		QString fullcmd = fullargs.join(" ");
		emit q->debug(QString("Running: [") + bin + ' ' + fullcmd + ']');

		args = fullargs;
	}

public slots:
	void doStart()
	{
#ifdef Q_OS_WIN
		// Note: for unix, inheritability is set in SProcess
		if(pipeAux.readEnd().isValid())
			pipeAux.readEnd().setInheritable(true);
		if(pipeCommand.readEnd().isValid())
			pipeCommand.readEnd().setInheritable(true);
		if(pipeStatus.writeEnd().isValid())
			pipeStatus.writeEnd().setInheritable(true);
#endif

		setupArguments();

		proc->start(bin, args);

		// FIXME: From reading the source to Qt on both windows
		//   and unix platforms, we know that fork/CreateProcess
		//   are called in start.  However this is not guaranteed
		//   from an API perspective.  We should probably call
		//   QProcess::waitForStarted() to synchronously ensure
		//   fork/CreateProcess are called before closing these
		//   pipes.
		pipeAux.readEnd().close();
		pipeCommand.readEnd().close();
		pipeStatus.writeEnd().close();
	}

	void aux_written(int x)
	{
		emit q->bytesWrittenAux(x);
	}

	void aux_error(QCA::QPipeEnd::Error)
	{
		emit q->debug("Aux: Pipe error");
		reset(ResetSession);
		emit q->error(GPGProc::ErrorWrite);
	}

	void command_written(int x)
	{
		emit q->bytesWrittenCommand(x);
	}

	void command_error(QCA::QPipeEnd::Error)
	{
		emit q->debug("Command: Pipe error");
		reset(ResetSession);
		emit q->error(GPGProc::ErrorWrite);
	}

	void status_read()
	{
		if(readAndProcessStatusData())
			emit q->readyReadStatusLines();
	}

	void status_error(QCA::QPipeEnd::Error e)
	{
		if(e == QPipeEnd::ErrorEOF)
			emit q->debug("Status: Closed (EOF)");
		else
			emit q->debug("Status: Closed (gone)");

		fin_status = true;
		doTryDone();
	}

	void proc_started()
	{
		emit q->debug("Process started");

		// Note: we don't close these here anymore.  instead we
		//   do it just after calling proc->start().
		// close these, we don't need them
		/*pipeAux.readEnd().close();
		pipeCommand.readEnd().close();
		pipeStatus.writeEnd().close();*/

		// do the pre* stuff
		if(!pre_stdin.isEmpty())
		{
			proc->write(pre_stdin);
			pre_stdin.clear();
		}
		if(!pre_aux.isEmpty())
		{
			pipeAux.writeEnd().write(pre_aux);
			pre_aux.clear();
		}
		if(!pre_command.isEmpty())
		{
#ifdef QPIPE_SECURE
			pipeCommand.writeEnd().writeSecure(pre_command);
#else
			pipeCommand.writeEnd().write(pre_command);
#endif
			pre_command.clear();
		}

		if(pre_stdin_close)
			proc->closeWriteChannel();
		if(pre_aux_close)
			pipeAux.writeEnd().close();
		if(pre_command_close)
			pipeCommand.writeEnd().close();
	}

	void proc_readyReadStandardOutput()
	{
		emit q->readyReadStdout();
	}

	void proc_readyReadStandardError()
	{
		emit q->readyReadStderr();
	}

	void proc_bytesWritten(qint64 lx)
	{
		int x = (int)lx;
		emit q->bytesWrittenStdin(x);
	}

	void proc_finished(int x)
	{
		emit q->debug(QString("Process finished: %1").arg(x));
		exitCode = x;

		fin_process = true;
		fin_process_success = true;

		if(need_status && !fin_status)
		{
			pipeStatus.readEnd().finalize();
			fin_status = true;
			if(readAndProcessStatusData())
			{
				doneTrigger.start();
				emit q->readyReadStatusLines();
				return;
			}
		}

		doTryDone();
	}

	void proc_error(QProcess::ProcessError x)
	{
		QMap<int, QString> errmap;
		errmap[QProcess::FailedToStart] = "FailedToStart";
		errmap[QProcess::Crashed]       = "Crashed";
		errmap[QProcess::Timedout]      = "Timedout";
		errmap[QProcess::WriteError]    = "WriteError";
		errmap[QProcess::ReadError]     = "ReadError";
		errmap[QProcess::UnknownError]  = "UnknownError";

		emit q->debug(QString("Process error: %1").arg(errmap[x]));

		if(x == QProcess::FailedToStart)
			error = GPGProc::FailedToStart;
		else if(x == QProcess::WriteError)
			error = GPGProc::ErrorWrite;
		else
			error = GPGProc::UnexpectedExit;

		fin_process = true;
		fin_process_success = false;

#ifdef QT_PIPE_HACK
		// If the process fails to start, then the ends of the pipes
		// intended for the child process are still open.  Some Mac
		// users experience a lockup if we close our ends of the pipes
		// when the child's ends are still open.  If we ensure the
		// child's ends are closed, we prevent this lockup.  I have no
		// idea why the problem even happens or why this fix should
		// work.
		pipeAux.readEnd().reset();
		pipeCommand.readEnd().reset();
		pipeStatus.writeEnd().reset();
#endif

		if(need_status && !fin_status)
		{
			pipeStatus.readEnd().finalize();
			fin_status = true;
			if(readAndProcessStatusData())
			{
				doneTrigger.start();
				emit q->readyReadStatusLines();
				return;
			}
		}

		doTryDone();
	}

	void doTryDone()
	{
		if(!fin_process)
			return;

		if(need_status && !fin_status)
			return;

		emit q->debug("Done");

		// get leftover data
		proc->setReadChannel(QProcess::StandardOutput);
		leftover_stdout = proc->readAll();

		proc->setReadChannel(QProcess::StandardError);
		leftover_stderr = proc->readAll();

		reset(ResetSession);
		if(fin_process_success)
			emit q->finished(exitCode);
		else
			emit q->error(error);
	}

private:
	bool readAndProcessStatusData()
	{
		QByteArray buf = pipeStatus.readEnd().read();
		if(buf.isEmpty())
			return false;

		return processStatusData(buf);
	}

	// return true if there are newly parsed lines available
	bool processStatusData(const QByteArray &buf)
	{
		statusBuf.append(buf);

		// extract all lines
		QStringList list;
		while(1)
		{
			int n = statusBuf.indexOf('\n');
			if(n == -1)
				break;

			// extract the string from statusbuf
			++n;
			char *p = (char *)statusBuf.data();
			QByteArray cs(p, n);
			int newsize = statusBuf.size() - n;
			memmove(p, p + n, newsize);
			statusBuf.resize(newsize);

			// convert to string without newline
			QString str = QString::fromUtf8(cs);
			str.truncate(str.length() - 1);

			// ensure it has a proper header
			if(str.left(9) != "[GNUPG:] ")
				continue;

			// take it off
			str = str.mid(9);

			// add to the list
			list += str;
		}

		if(list.isEmpty())
			return false;

		statusLines += list;
		return true;
	}
};

GPGProc::GPGProc(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

GPGProc::~GPGProc()
{
	delete d;
}

void GPGProc::reset()
{
	d->reset(ResetAll);
}

bool GPGProc::isActive() const
{
	return (d->proc ? true : false);
}

void GPGProc::start(const QString &bin, const QStringList &args, Mode mode)
{
	if(isActive())
		d->reset(ResetSessionAndData);

	if(mode == ExtendedMode)
	{
		if(!d->setupPipes(args.contains("-&?")))
		{
			d->error = FailedToStart;

			// emit later
			QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection, Q_ARG(gpgQCAPlugin::GPGProc::Error, d->error));
			return;
		}

		d->need_status = true;

		emit debug("Pipe setup complete");
	}

	d->proc = new SProcess(d);

#ifdef Q_OS_UNIX
	QList<int> plist;
	if(d->pipeAux.readEnd().isValid())
		plist += d->pipeAux.readEnd().id();
	if(d->pipeCommand.readEnd().isValid())
		plist += d->pipeCommand.readEnd().id();
	if(d->pipeStatus.writeEnd().isValid())
		plist += d->pipeStatus.writeEnd().id();
	d->proc->setInheritPipeList(plist);
#endif

	// enable the pipes we want
	if(d->pipeAux.writeEnd().isValid())
		d->pipeAux.writeEnd().enable();
	if(d->pipeCommand.writeEnd().isValid())
		d->pipeCommand.writeEnd().enable();
	if(d->pipeStatus.readEnd().isValid())
		d->pipeStatus.readEnd().enable();

#ifdef QPROC_SIGNAL_RELAY
	d->proc_relay = new QProcessSignalRelay(d->proc, d);
	connect(d->proc_relay, SIGNAL(started()), d, SLOT(proc_started()));
	connect(d->proc_relay, SIGNAL(readyReadStandardOutput()), d, SLOT(proc_readyReadStandardOutput()));
	connect(d->proc_relay, SIGNAL(readyReadStandardError()), d, SLOT(proc_readyReadStandardError()));
	connect(d->proc_relay, SIGNAL(bytesWritten(qint64)), d, SLOT(proc_bytesWritten(qint64)));
	connect(d->proc_relay, SIGNAL(finished(int)), d, SLOT(proc_finished(int)));
	connect(d->proc_relay, SIGNAL(error(QProcess::ProcessError)), d, SLOT(proc_error(QProcess::ProcessError)));
#else
	connect(d->proc, SIGNAL(started()), d, SLOT(proc_started()));
	connect(d->proc, SIGNAL(readyReadStandardOutput()), d, SLOT(proc_readyReadStandardOutput()));
	connect(d->proc, SIGNAL(readyReadStandardError()), d, SLOT(proc_readyReadStandardError()));
	connect(d->proc, SIGNAL(bytesWritten(qint64)), d, SLOT(proc_bytesWritten(qint64)));
	connect(d->proc, SIGNAL(finished(int)), d, SLOT(proc_finished(int)));
	connect(d->proc, SIGNAL(error(QProcess::ProcessError)), d, SLOT(proc_error(QProcess::ProcessError)));
#endif

	d->bin = bin;
	d->args = args;
	d->mode = mode;
	d->startTrigger.start();
}

QByteArray GPGProc::readStdout()
{
	if(d->proc)
	{
		d->proc->setReadChannel(QProcess::StandardOutput);
		return d->proc->readAll();
	}
	else
	{
		QByteArray a = d->leftover_stdout;
		d->leftover_stdout.clear();
		return a;
	}
}

QByteArray GPGProc::readStderr()
{
	if(d->proc)
	{
		d->proc->setReadChannel(QProcess::StandardError);
		return d->proc->readAll();
	}
	else
	{
		QByteArray a = d->leftover_stderr;
		d->leftover_stderr.clear();
		return a;
	}
}

QStringList GPGProc::readStatusLines()
{
	QStringList out = d->statusLines;
	d->statusLines.clear();
	return out;
}

void GPGProc::writeStdin(const QByteArray &a)
{
	if(!d->proc || a.isEmpty())
		return;

	if(d->proc->state() == QProcess::Running)
		d->proc->write(a);
	else
		d->pre_stdin += a;
}

void GPGProc::writeAux(const QByteArray &a)
{
	if(!d->proc || a.isEmpty())
		return;

	if(d->proc->state() == QProcess::Running)
		d->pipeAux.writeEnd().write(a);
	else
		d->pre_aux += a;
}

#ifdef QPIPE_SECURE
void GPGProc::writeCommand(const SecureArray &a)
#else
void GPGProc::writeCommand(const QByteArray &a)
#endif
{
	if(!d->proc || a.isEmpty())
		return;

	if(d->proc->state() == QProcess::Running)
#ifdef QPIPE_SECURE
		d->pipeCommand.writeEnd().writeSecure(a);
#else
		d->pipeCommand.writeEnd().write(a);
#endif
	else
		d->pre_command += a;
}

void GPGProc::closeStdin()
{
	if(!d->proc)
		return;

	if(d->proc->state() == QProcess::Running)
		d->proc->closeWriteChannel();
	else
		d->pre_stdin_close = true;
}

void GPGProc::closeAux()
{
	if(!d->proc)
		return;

	if(d->proc->state() == QProcess::Running)
		d->pipeAux.writeEnd().close();
	else
		d->pre_aux_close = true;
}

void GPGProc::closeCommand()
{
	if(!d->proc)
		return;

	if(d->proc->state() == QProcess::Running)
		d->pipeCommand.writeEnd().close();
	else
		d->pre_command_close = true;
}

}

#include "gpgproc.moc"
