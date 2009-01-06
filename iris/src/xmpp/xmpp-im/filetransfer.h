/*
 * filetransfer.h - File Transfer
 * Copyright (C) 2004  Justin Karneges
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

#ifndef XMPP_FILETRANSFER_H
#define XMPP_FILETRANSFER_H

#include "im.h"

namespace XMPP
{
	class S5BConnection;
	struct FTRequest;

	/*class AbstractFileTransfer 
	{
		public:
			// Receive
			virtual Jid peer() const = 0;
			virtual QString fileName() const = 0;
			virtual qlonglong fileSize() const = 0;
			virtual QString description() const { return ""; }
			virtual bool rangeSupported() const { return false; }
			virtual void accept(qlonglong offset=0, qlonglong length=0) = 0;
	};*/

	class FileTransfer : public QObject /*, public AbstractFileTransfer */
	{
		Q_OBJECT
	public:
		enum { ErrReject, ErrNeg, ErrConnect, ErrProxy, ErrStream, Err400 };
		enum { Idle, Requesting, Connecting, WaitingForAccept, Active };
		~FileTransfer();

		FileTransfer *copy() const;

		void setProxy(const Jid &proxy);

		// send
		void sendFile(const Jid &to, const QString &fname, qlonglong size, const QString &desc);
		qlonglong offset() const;
		qlonglong length() const;
		int dataSizeNeeded() const;
		void writeFileData(const QByteArray &a);

		// receive
		Jid peer() const;
		QString fileName() const;
		qlonglong fileSize() const;
		QString description() const;
		bool rangeSupported() const;
		void accept(qlonglong offset=0, qlonglong length=0);

		// both
		void close(); // reject, or stop sending/receiving
		S5BConnection *s5bConnection() const; // active link

	signals:
		void accepted(); // indicates S5BConnection has started
		void connected();
		void readyRead(const QByteArray &a);
		void bytesWritten(int);
		void error(int);

	private slots:
		void ft_finished();
		void s5b_connected();
		void s5b_connectionClosed();
		void s5b_readyRead();
		void s5b_bytesWritten(int);
		void s5b_error(int);
		void doAccept();

	private:
		class Private;
		Private *d;

		void reset();

		friend class FileTransferManager;
		FileTransfer(FileTransferManager *, QObject *parent=0);
		FileTransfer(const FileTransfer& other);
		void man_waitForAccept(const FTRequest &req);
		void takeConnection(S5BConnection *c);
	};

	class FileTransferManager : public QObject
	{
		Q_OBJECT
	public:
		FileTransferManager(Client *);
		~FileTransferManager();

		bool isActive(const FileTransfer *ft) const;

		Client *client() const;
		FileTransfer *createTransfer();
		FileTransfer *takeIncoming();

	signals:
		void incomingReady();

	private slots:
		void pft_incoming(const FTRequest &req);

	private:
		class Private;
		Private *d;

		friend class Client;
		void s5b_incomingReady(S5BConnection *);

		friend class FileTransfer;
		QString link(FileTransfer *);
		void con_accept(FileTransfer *);
		void con_reject(FileTransfer *);
		void unlink(FileTransfer *);
	};

	class JT_FT : public Task
	{
		Q_OBJECT
	public:
		JT_FT(Task *parent);
		~JT_FT();

		void request(const Jid &to, const QString &id, const QString &fname, qlonglong size, const QString &desc, const QStringList &streamTypes);
		qlonglong rangeOffset() const;
		qlonglong rangeLength() const;
		QString streamType() const;

		void onGo();
		bool take(const QDomElement &);

	private:
		class Private;
		Private *d;
	};

	struct FTRequest
	{
		Jid from;
		QString iq_id, id;
		QString fname;
		qlonglong size;
		QString desc;
		bool rangeSupported;
		QStringList streamTypes;
	};
	class JT_PushFT : public Task
	{
		Q_OBJECT
	public:
		JT_PushFT(Task *parent);
		~JT_PushFT();

		void respondSuccess(const Jid &to, const QString &id, qlonglong rangeOffset, qlonglong rangeLength, const QString &streamType);
		void respondError(const Jid &to, const QString &id, int code, const QString &str);

		bool take(const QDomElement &);

	signals:
		void incoming(const FTRequest &req);
	};
}

#endif
