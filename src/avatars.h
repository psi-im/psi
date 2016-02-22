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
	AvatarFactory(PsiAccount* pa);

	QPixmap getAvatar(const Jid& jid);
	PsiAccount* account() const;
	void setSelfAvatar(const QString& fileName);

	void importManualAvatar(const Jid& j, const QString& fileName);
	void removeManualAvatar(const Jid& j);
	bool hasManualAvatar(const Jid& j);

	void newMucItem(const Jid& fullJid, const Status& s);
	QPixmap getMucAvatar(const Jid& jid);

	static QString getManualDir();
	static QString getCacheDir();
	static int maxAvatarSize();
	static QPixmap roundedAvatar(const QPixmap& pix, int rad, int avatarSize);

signals:
	void avatarChanged(const Jid&);

public slots:
	void updateAvatar(const Jid&);
	void updateMucAvatar(const Jid&);

protected slots:
	void itemPublished(const Jid&, const QString&, const PubSubItem&);
	void publish_success(const QString&, const PubSubItem&);
	void resourceAvailable(const Jid&, const Resource&);

protected:
	Avatar* retrieveAvatar(const Jid& jid);

private:
	QByteArray selfAvatarData_;
	QString selfAvatarHash_;

	QMap<QString,Avatar*> active_avatars_;
	QMap<QString,PEPAvatar*> pep_avatars_;
	QMap<QString,FileAvatar*> file_avatars_;
	QMap<QString,VCardAvatar*> vcard_avatars_;
	QMap<QString,VCardMucAvatar*> muc_vcard_avatars_;
	QMap<QString,VCardStaticAvatar*> vcard_static_avatars_;
	PsiAccount* pa_;
	Iconset iconset_;
};

//------------------------------------------------------------------------------

class Avatar : public QObject
{
	Q_OBJECT
public:
	Avatar(AvatarFactory* factory);
	virtual ~Avatar();
	virtual QPixmap getPixmap()
		{ return pixmap(); }
	virtual bool isEmpty()
		{ return getPixmap().isNull(); }

protected:
	AvatarFactory* factory() const;
	virtual const QPixmap& pixmap() const
		{ return pixmap_; }

	virtual void setImage(const QImage&);
	virtual void setImage(const QByteArray&);
	virtual void setImage(const QPixmap&);
	virtual void resetImage();

private:
	QPixmap pixmap_;
	AvatarFactory* factory_;
};


#endif
