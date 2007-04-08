/*
 * xmpp_status.h
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

#ifndef XMPP_STATUS_H
#define XMPP_STATUS_H

#include <QString>
#include <QDateTime>

#include "xmpp_muc.h"

namespace XMPP 
{
	class Status
	{
	public:
		enum Type { Offline, Online, Away, XA, DND, Invisible, FFC };

		Status(const QString &show="", const QString &status="", int priority=0, bool available=true);
		Status(Type type, const QString& status="", int priority=0);
		~Status();

		int priority() const;
		Type type() const;
		QString typeString() const;
		const QString & show() const;
		const QString & status() const;
		QDateTime timeStamp() const;
		const QString & keyID() const;
		bool isAvailable() const;
		bool isAway() const;
		bool isInvisible() const;
		bool hasError() const;
		int errorCode() const;
		const QString & errorString() const;

		const QString & xsigned() const;
		const QString & songTitle() const;
		const QString & capsNode() const;
		const QString & capsVersion() const;
		const QString & capsExt() const;
		
		bool isMUC() const;
		bool hasMUCItem() const;
		const MUCItem & mucItem() const;
		bool hasMUCDestroy() const;
		const MUCDestroy & mucDestroy() const;
		bool hasMUCStatus() const;
		int mucStatus() const;
		const QString& mucPassword() const;
		bool hasMUCHistory() const;
		int mucHistoryMaxChars() const;
		int mucHistoryMaxStanzas() const;
		int mucHistorySeconds() const;

		void setPriority(int);
		void setType(Type);
		void setType(QString);
		void setShow(const QString &);
		void setStatus(const QString &);
		void setTimeStamp(const QDateTime &);
		void setKeyID(const QString &);
		void setIsAvailable(bool);
		void setIsInvisible(bool);
		void setError(int, const QString &);
		void setCapsNode(const QString&);
		void setCapsVersion(const QString&);
		void setCapsExt(const QString&);
		
		void setMUC();
		void setMUCItem(const MUCItem&);
		void setMUCDestroy(const MUCDestroy&);
		void setMUCStatus(int);
		void setMUCPassword(const QString&);
		void setMUCHistory(int maxchars, int maxstanzas, int seconds);

		void setXSigned(const QString &);
		void setSongTitle(const QString &);

		// JEP-153: VCard-based Avatars
		const QString& photoHash() const;
		void setPhotoHash(const QString&);
		bool hasPhotoHash() const;

	private:
		int v_priority;
		QString v_show, v_status, v_key;
		QDateTime v_timeStamp;
		bool v_isAvailable;
		bool v_isInvisible;
		QString v_photoHash;
		bool v_hasPhotoHash;

		QString v_xsigned;
		// gabber song extension
		QString v_songTitle;
		QString v_capsNode, v_capsVersion, v_capsExt;

		// MUC
		bool v_isMUC, v_hasMUCItem, v_hasMUCDestroy;
		MUCItem v_mucItem;
		MUCDestroy v_mucDestroy;
		int v_mucStatus;
		QString v_mucPassword;
		int v_mucHistoryMaxChars, v_mucHistoryMaxStanzas, v_mucHistorySeconds;

		int ecode;
		QString estr;
	};

}

#endif
