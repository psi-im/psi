/*
 * qca_support.h - Qt Cryptographic Architecture
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005  Brad Hards <bradh@frogmouth.net>
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

#include <QString>
#include <QObject>
#include "qca_export.h"
#include "qca_tools.h"

namespace QCA
{
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
		DirWatch(const QString &dir = QString(), QObject *parent = 0);
		~DirWatch();

		QString dirName() const;
		void setDirName(const QString &dir);

		// DirWatch still works even if this returns false,
		// but it will be inefficient
		static bool platformSupported();

	signals:
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
		FileWatch(const QString &file = QString(), QObject *parent = 0);
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

	signals:
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

	// note: only one Console object can be created at a time
	class QCA_EXPORT Console : public QObject
	{
		Q_OBJECT
	public:
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

		Console(ChannelMode cmode = Read, TerminalMode tmode = Default, QObject *parent = 0);
		~Console();

		static Console *instance();

		// call shutdown() to get access to unempty buffers
		void shutdown();
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

		bool start(SecurityMode mode = SecurityDisabled);
		void stop();

		// normal i/o
		QByteArray read(int bytes = -1);
		void write(const QByteArray &a);

		// secure i/o
		QSecureArray readSecure(int bytes = -1);
		void writeSecure(const QSecureArray &a);

		int bytesAvailable() const;
		int bytesToWrite() const;

	signals:
		void readyRead();
		void bytesWritten(int);
		void closed();

	private:
		friend class ConsoleReferencePrivate;
		ConsoleReferencePrivate *d;

		friend class Console;
	};
}

#endif
