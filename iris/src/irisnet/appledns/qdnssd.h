/*
 * Copyright (C) 2007  Justin Karneges
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

#ifndef QDNSSD_H
#define QDNSSD_H

#include <QObject>
#include <QByteArray>
#include <QList>

// DOR-compliant
class QDnsSd : public QObject
{
	Q_OBJECT
public:
	class LowLevelError
	{
	public:
		QString func;
		int code;

		LowLevelError() :
			code(0)
		{
		}

		LowLevelError(const QString &_func, int _code) :
			func(_func),
			code(_code)
		{
		}
	};

	class Record
	{
	public:
		bool added; // only used by QueryResult

		QByteArray name;
		int rrtype;
		QByteArray rdata;
		quint32 ttl;
	};

	class BrowseEntry
	{
	public:
		bool added;
		QByteArray serviceName;

		// these may be different from request, see dns_sd docs
		QByteArray serviceType;
		QByteArray replyDomain;
	};

	class QueryResult
	{
	public:
		bool success;
		LowLevelError lowLevelError;

		QList<Record> records;
	};

	class BrowseResult
	{
	public:
		bool success;
		LowLevelError lowLevelError;

		QList<BrowseEntry> entries;
	};

	class ResolveResult
	{
	public:
		bool success;
		LowLevelError lowLevelError;

		QByteArray fullName;
		QByteArray hostTarget;
		int port; // host byte-order
		QByteArray txtRecord;
	};

	class RegResult
	{
	public:
		enum Error
		{
			ErrorGeneric,
			ErrorConflict
		};

		bool success;
		Error errorCode;
		LowLevelError lowLevelError;

		QByteArray domain;
	};

	QDnsSd(QObject *parent = 0);
	~QDnsSd();

	int query(const QByteArray &name, int qType);

	// domain may be empty
	int browse(const QByteArray &serviceType, const QByteArray &domain);

	int resolve(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain);

	// domain may be empty
	int reg(const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain, int port, const QByteArray &txtRecord);

	// return -1 on error, else a record id
	int recordAdd(int reg_id, const Record &rec, LowLevelError *lowLevelError = 0);

	bool recordUpdate(int rec_id, const Record &rec, LowLevelError *lowLevelError = 0);
	bool recordUpdateTxt(int reg_id, const QByteArray &txtRecord, quint32 ttl, LowLevelError *lowLevelError = 0);
	void recordRemove(int rec_id);

	void stop(int id);

	// return empty array on error
	static QByteArray createTxtRecord(const QList<QByteArray> &strings);

	// return empty list on error (note that it is possible to have a
	//   txt record with no entries, but in that case txtRecord will be
	//   empty and so you shouldn't call this function)
	static QList<QByteArray> parseTxtRecord(const QByteArray &txtRecord);

signals:
	void queryResult(int id, const QDnsSd::QueryResult &result);
	void browseResult(int id, const QDnsSd::BrowseResult &result);
	void resolveResult(int id, const QDnsSd::ResolveResult &result);
	void regResult(int id, const QDnsSd::RegResult &result);

private:
	class Private;
	friend class Private;
	Private *d;
};

#endif
