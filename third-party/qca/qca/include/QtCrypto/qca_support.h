/*
 * qca_support.h - Qt Cryptographic Architecture
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005, 2007  Brad Hards <bradh@frogmouth.net>
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
   \file qca_support.h

   Header file for "support" classes used in %QCA

   The classes in this header do not have any cryptographic
   content - they are used in %QCA, and are included for convenience.

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_SUPPORT_H
#define QCA_SUPPORT_H

#include <QByteArray>
#include <QString>
#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <QList>
#include <QMetaObject>
#include <QThread>
#include "qca_export.h"
#include "qca_tools.h"

namespace QCA
{
	QCA_EXPORT QByteArray methodReturnType(const QMetaObject *obj, const QByteArray &method, const QList<QByteArray> argTypes);
	QCA_EXPORT bool invokeMethodWithVariants(QObject *obj, const QByteArray &method, const QVariantList &args, QVariant *ret, Qt::ConnectionType type = Qt::AutoConnection);

	class QCA_EXPORT SyncThread : public QThread
	{
		Q_OBJECT
	public:
		SyncThread(QObject *parent = 0);
		~SyncThread();

		void start();
		void stop();
		QVariant call(QObject *obj, const QByteArray &method, const QVariantList &args = QVariantList(), bool *ok = 0);

	protected:
		virtual void atStart() = 0;
		virtual void atEnd() = 0;

		// reimplemented
		virtual void run();

	private:
		class Private;
		friend class Private;
		Private *d;
	};

	class QCA_EXPORT Synchronizer : public QObject
	{
		Q_OBJECT
	public:
		Synchronizer(QObject *parent);
		~Synchronizer();

		bool waitForCondition(int msecs = -1);
		void conditionMet();

	private:
		class Private;
		Private *d;
	};

	class QCA_EXPORT DirWatch : public QObject
	{
		Q_OBJECT
	public:
		explicit DirWatch(const QString &dir = QString(), QObject *parent = 0);
		~DirWatch();

		QString dirName() const;
		void setDirName(const QString &dir);

		// DirWatch still works even if this returns false,
		// but it will be inefficient
		static bool platformSupported();

	Q_SIGNALS:
		void changed();

	private:
		class Private;
		friend class Private;
		Private *d;
	};

	/**
	   Support class to monitor a file for activity

	   %FileWatch monitors a specified file for any changes. When
	   the file changes, the changed() signal is emitted.
	*/
	class QCA_EXPORT FileWatch : public QObject
	{
		Q_OBJECT
	public:
		/**
		   Standard constructor

		   \param file the name of the file to watch. If not
		   in this object, you can set it using setFileName()
		   \param parent the parent object for this object
		*/
		explicit FileWatch(const QString &file = QString(), QObject *parent = 0);
		~FileWatch();

		/**
		   The name of the file that is being monitored
		*/
		QString fileName() const;

		/**
		   Change the file being monitored

		   \param file the name of the file to monitor
		*/
		void setFileName(const QString &file);

	Q_SIGNALS:
		/**
		   The changed signal is emitted when the file is
		   changed (e.g. modified, deleted)
		*/
		void changed();

	private:
		class Private;
		friend class Private;
		Private *d;
	};

	class ConsolePrivate;
	class ConsoleReferencePrivate;
	class ConsoleReference;

	// QCA Console system
	//
	// QCA provides an API for asynchronous, event-based access to
	//   the console and stdin/stdout, as these facilities are
	//   otherwise not portable.  The primary use of this system within
	//   QCA is for passphrase prompting in command-line applications,
	//   using the tty console type.
	//
	// How it works: Create a Console object for the type of console
	//   desired, and then use ConsoleReference to act on the console.
	//   Only one ConsoleReference may operate on a Console at a time.
	//
	// A Console object overtakes either the physical console (tty
	//   type) or stdin/stdout (stdio type).  Only one of each type
	//   may be created at a time.
	//
	// Whenever code is written that needs a tty or stdio object, the
	//   code should first call one of the static methods (ttyInstance
	//   or stdioInstance) to see if a console object for the desired
	//   type exists already.  If the object exists, use it.  If it does
	//   not exist, the rule is that the relevant code should create the
	//   object, use the object, and then destroy the object when the
	//   operation is completed.
	//
	// By following the above rule, you can write code that utilizes
	//   a console without the application having to create some master
	//   console object for you.  Of course, if the application has
	//   created a console then it will be used.
	//
	// Why make a master console object?  The primary reason is that it
	//   is not guaranteed that all I/O will survive creation and
	//   destruction of a console object.  If you are using the stdio
	//   type, then you probably want a long-lived console object.  It
	//   is possible to capture unprocessed I/O by calling
	//   bytesLeftToRead or bytesLeftToWrite.  However, it is not
	//   expected that general console-needing code will call these
	//   functions when utilizing a temporary console.  Thus, an
	//   application developer would need to create his own console
	//   object, invoke the console-needing code, and then do his own
	//   extraction of the unprocessed I/O if necessary.  Another reason
	//   to extract unprocessed I/O is if you need to switch from
	//   QCA::Console back to standard functions (e.g. fgets).
	//
	class QCA_EXPORT Console : public QObject
	{
		Q_OBJECT
	public:
		enum Type
		{
			Tty,         // physical console
			Stdio        // stdin/stdout
		};

		enum ChannelMode
		{
			Read,        // stdin
			ReadWrite    // stdin + stdout
		};

		enum TerminalMode
		{
			Default,     // use default terminal settings
			Interactive  // char-by-char input, no echo
		};

		Console(Type type, ChannelMode cmode, TerminalMode tmode, QObject *parent = 0);
		~Console();

		static bool isStdinRedirected();
		static bool isStdoutRedirected();

		static Console *ttyInstance();
		static Console *stdioInstance();

		// call release() to get access to unempty buffers
		void release();
		QByteArray bytesLeftToRead();
		QByteArray bytesLeftToWrite();

	private:
		friend class ConsolePrivate;
		ConsolePrivate *d;

		friend class ConsoleReference;
	};

	// note: only one ConsoleReference object can be active at a time
	class QCA_EXPORT ConsoleReference : public QObject
	{
		Q_OBJECT
	public:
		enum SecurityMode
		{
			SecurityDisabled,
			SecurityEnabled
		};

		ConsoleReference(QObject *parent = 0);
		~ConsoleReference();

		bool start(Console *console, SecurityMode mode = SecurityDisabled);
		void stop();

		// normal i/o
		QByteArray read(int bytes = -1);
		void write(const QByteArray &a);

		// secure i/o
		SecureArray readSecure(int bytes = -1);
		void writeSecure(const SecureArray &a);

		// close write channel (only if writing enabled)
		void closeOutput();

		int bytesAvailable() const;
		int bytesToWrite() const;

	Q_SIGNALS:
		void readyRead();
		void bytesWritten(int bytes);
		void inputClosed();
		void outputClosed();

	private:
		friend class ConsoleReferencePrivate;
		ConsoleReferencePrivate *d;

		friend class Console;
	};

	class QCA_EXPORT ConsolePrompt : public QObject
	{
		Q_OBJECT
	public:
		ConsolePrompt(QObject *parent = 0);
		~ConsolePrompt();

		void getHidden(const QString &promptStr);
		void getEnter();
		void waitForFinished();

		SecureArray result() const;

	signals:
		void finished();

	private:
		class Private;
		Private *d;
	};

        class AbstractLogDevice;

        /**
           A simple logging system

           This class provides a simple but flexible approach to logging information that
           may be used for debugging or system operation diagnostics.

           There is a single %Logger for each application that uses %QCA. You do not need
           to create this %Logger yourself - %QCA automatically creates it on startup. You
           can get access to the %Logger using the global QCA::logger() method.

           By default the Logger just accepts all messages (binary and text). If you want to
           get access to those messages, you need to subclass AbstractLogDevice, and register
           your subclass (using registerLogDevice()). You can then take whatever action is
           appropriate (e.g. show to the user using the GUI, log to a file or send to standard
           error).
        */
	class QCA_EXPORT Logger : public QObject
	{
	    Q_OBJECT
	public:
            /**
               The severity of the message

               This information may be used by the log device to determine
               what the appropriate action is.
            */
            enum Severity
            {
                Quiet = 0,       ///< Quiet: turn of logging
                Emergency = 1,   ///< Emergency: system is unusable
                Alert = 2,       ///< Alert: action must be taken immediately
                Critical = 3,    ///< Critical: critical conditions
                Error = 4,       ///< Error: error conditions
                Warning = 5,     ///< Warning: warning conditions
                Notice = 6,      ///< Notice: normal but significant condition
                Information = 7, ///< Informational: informational messages
                Debug = 8        ///< Debug: debug-level messages
            };

	    /**
		Get the current logging level

		\return Current level
	    */
	    inline Logger::Severity level () const {
	    	return m_logLevel;
	    }

	    /**
		Set the current logging level

		\param level new logging level

		Only severities less or equal than the log level one will be logged
	    */
	    void setLevel (Logger::Severity level);

	    /**
		Log a message to all available log devices

		\param message the text to log
	    */
	    void logTextMessage(const QString &message, Severity = Information);

	    /**
	       Log a binary blob to all available log devices

	       \param blob the information to log

	       \note how this is handled is quite logger specific. For
	       example, it might be logged as a binary, or it might be
	       encoded in some way
	    */
	    void logBinaryMessage(const QByteArray &blob, Severity = Information);

	    /**
	       Add an AbstractLogDevice subclass to the existing list of loggers

	       \param logger the LogDevice to add
	    */
	    void registerLogDevice(AbstractLogDevice *logger);

	    /**
	       Remove an AbstractLogDevice subclass from the existing list of loggers

	       \param loggerName the name of the LogDevice to remove

	       \note If there are several log devices with the same name, all will be removed.
	    */
	    void unregisterLogDevice(const QString &loggerName);

	    /**
	       Get a list of the names of all registered log devices
	    */
	    QStringList currentLogDevices() const;

	private:
	    friend class Global;

	    /**
	       Create a new message logger
	    */
	    Logger();

	    ~Logger();

	    QStringList m_loggerNames;
	    QList<AbstractLogDevice*> m_loggers;
	    Severity m_logLevel;
	};

        /**
           An abstract log device
        */
	class QCA_EXPORT AbstractLogDevice : public QObject
	{
	    Q_OBJECT
	public:
	    /**
		The name of this log device
	    */
	    QString name() const;

            /**
               Log a message

               The default implementation does nothing - you should
               override this method in your subclass to do whatever
               logging is required
            */
            virtual void logTextMessage( const QString &message, enum Logger::Severity severity );

            /**
               Log a binary blob

               The default implementation does nothing - you should
               override this method in your subclass to do whatever
               logging is required
            */
            virtual void logBinaryMessage( const QByteArray &blob, Logger::Severity severity );

	protected:
	    /**
	       Create a new message logger

	       \param name the name of this log device
	       \param parent the parent for this logger
	    */
	    explicit AbstractLogDevice(const QString &name, QObject *parent = 0);

	    virtual ~AbstractLogDevice() = 0;

	private:
	    class Private;
	    Private *d;

            QString m_name;
	};


}

#endif
