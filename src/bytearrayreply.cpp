/*
 * bytearrayreply.cpp - Base class for QNetworReply'es returning QByteArray
 * Copyright (C) 2010 senu, Rion
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "bytearrayreply.h"

#include <QTimer>

ByteArrayReply::ByteArrayReply(const QNetworkRequest &request,
							   const QByteArray &ba, const QString& mimeType,
							   QObject *parent) :
	QNetworkReply(parent),
	origLen(ba.size()),
    data(ba),
	buffer(&data)
{
	setRequest(request);
	setOpenMode(QIODevice::ReadOnly);
	buffer.open(QIODevice::ReadOnly);

	if (ba.isNull()) {
		setError(QNetworkReply::ContentNotFoundError, "Not found");
		QTimer::singleShot(0, this, SIGNAL(metaDataChanged()));
		QTimer::singleShot(0, this, SLOT(signalError()));
		QTimer::singleShot(0, this, SIGNAL(finished()));
	} else {
		if (mimeType.isEmpty()) {
			setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
		} else {
			setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
		}
		setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(origLen));
		QTimer::singleShot(0, this, SIGNAL(metaDataChanged()));
		QTimer::singleShot(0, this, SIGNAL(readyRead()));
	}
}

ByteArrayReply::~ByteArrayReply() {

}

void ByteArrayReply::abort() {
	// its ok for abort here. webkit calls it in any case on finish
}

qint64 ByteArrayReply::bytesAvailable() const
{
	return data.length() - buffer.pos() + QNetworkReply::bytesAvailable();
}


qint64 ByteArrayReply::readData(char *buf, qint64 maxlen)
{
	auto len = buffer.read(buf, maxlen);
	if (buffer.atEnd())
		QTimer::singleShot(0, this, SIGNAL(finished()));
	return len;
}


bool ByteArrayReply::open(OpenMode mode) {
	Q_ASSERT(0); // we don't come here
	return buffer.open(mode);
}

void ByteArrayReply::signalError()
{
	emit error(error());
}
