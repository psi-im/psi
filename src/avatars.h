/*
 * avatars.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef AVATARS_H
#define AVATARS_H

#include <QPixmap>
#include <QMap>
#include <QByteArray>
#include <QString>

#include "iconset.h"

#define PEP_AVATAR_DATA_TN     "data"
#define PEP_AVATAR_DATA_NS     "urn:xmpp:avatar:data"

#define PEP_AVATAR_METADATA_TN "metadata"
#define PEP_AVATAR_METADATA_NS "urn:xmpp:avatar:metadata"

class PsiAccount;
class Avatar;
class VCardAvatar;
class VCardMucAvatar;
class VCardStaticAvatar;
class FileAvatar;
class PEPAvatar;

namespace XMPP {
	class Jid;
	class Resource;
	class PubSubItem;
	class Status;
}

using namespace XMPP;

//------------------------------------------------------------------------------

class AvatarFactory : public QObject
{
	Q_OBJECT

public:
	struct UserHashes {
		QString avatar; // current active avatar
		QString vcard;  // avatar hash just in case
	};

	struct AvatarData {
		QByteArray data;
		QString metaType;
	};

	AvatarFactory(PsiAccount* pa);

	QPixmap getAvatar(const Jid& jid);
	QPixmap getAvatarByHash(const QString& hash);
	static AvatarData avatarDataByHash(const QString& hash);
	UserHashes userHashes(const Jid& jid) const;
	PsiAccount* account() const;
	void setSelfAvatar(const QString& fileName);

	void importManualAvatar(const Jid& j, const QString& fileName);
	void removeManualAvatar(const Jid& j);
	bool hasManualAvatar(const Jid& j);

	void newMucItem(const Jid& fullJid, const Status& s);
	QPixmap getMucAvatar(const Jid& jid);

	static QString getCacheDir();
	static int maxAvatarSize();
	static QPixmap roundedAvatar(const QPixmap& pix, int rad, int avatarSize);

	void statusUpdate(const Jid &jid, const XMPP::Status &status);
signals:
	void avatarChanged(const Jid&);

protected slots:
	void itemPublished(const Jid&, const QString&, const PubSubItem&);
	void publish_success(const QString&, const PubSubItem&);
	void resourceAvailable(const Jid&, const Resource&);

private slots:
	void onVcardTaskFinsihed();
	void vcardUpdated(const Jid&, bool isMuc);
private:

	QByteArray selfAvatarData_;
	QString selfAvatarHash_;

	PsiAccount* pa_;
	Iconset iconset_;
};



#endif
