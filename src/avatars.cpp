/*
 * avatars.cpp 
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * TODO:
 * - Be more efficient about storing avatars in memory
 * - Move ovotorChanged() to Avatar, and only listen to the active avatars
 *   being changed.
 */

#include <QDomElement>
#include <QtCrypto>
#include <QPixmap>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QBuffer>
#include <QPainter>

#include <qca_basic.h>

#include "xmpp_xmlcommon.h"
#include "xmpp_vcard.h"
#include "xmpp_client.h"
#include "xmpp_resource.h"
#include "xmpp_pubsubitem.h"
#include "avatars.h"
#include "applicationinfo.h"
#include "psiaccount.h"
#include "profiles.h"
#include "vcardfactory.h"
#include "pepmanager.h"
#include "pixmaputil.h"

#define MAX_AVATAR_SIZE 96
#define MAX_AVATAR_DISPLAY_SIZE 64

using namespace QCA;


//------------------------------------------------------------------------------

static QByteArray scaleAvatar(const QByteArray& b)
{
	//int maxSize = (LEGOPTS.avatarsSize > MAX_AVATAR_SIZE ? MAX_AVATAR_SIZE : LEGOPTS.avatarsSize);
	int maxSize = AvatarFactory::maxAvatarSize();
	QImage i = QImage::fromData(b);
	if (i.isNull()) {
		qWarning("AvatarFactory::scaleAvatar(): Null image (unrecognized format?)");
		return QByteArray();
	}
	else if (i.width() > maxSize || i.height() > maxSize) {
		QImage image = i.scaled(maxSize,maxSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
		QByteArray ba;
		QBuffer buffer(&ba);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		return ba;
	}
	else {
		return b;
	}
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Avatar: Base class for avatars.
//------------------------------------------------------------------------------

Avatar::Avatar(AvatarFactory* factory)
	: QObject(factory), factory_(factory)
{
}


Avatar::~Avatar()
{
}

void Avatar::setImage(const QImage& i)
{
	if (i.width() > MAX_AVATAR_DISPLAY_SIZE || i.height() > MAX_AVATAR_DISPLAY_SIZE)
		pixmap_ = QPixmap::fromImage(i.scaled(MAX_AVATAR_DISPLAY_SIZE,MAX_AVATAR_DISPLAY_SIZE,Qt::KeepAspectRatio,Qt::SmoothTransformation));
	else
		pixmap_ = QPixmap::fromImage(i);

}

void Avatar::setImage(const QByteArray& ba)
{
	setImage(QImage::fromData(ba));
}

void Avatar::setImage(const QPixmap& p)
{
	if (p.width() > MAX_AVATAR_DISPLAY_SIZE || p.height() > MAX_AVATAR_DISPLAY_SIZE)
		pixmap_ = p.scaled(MAX_AVATAR_DISPLAY_SIZE,MAX_AVATAR_DISPLAY_SIZE,Qt::KeepAspectRatio,Qt::SmoothTransformation);
	else
		pixmap_ = p;
}

void Avatar::resetImage()
{
	pixmap_ = QPixmap();
}

AvatarFactory* Avatar::factory() const
{
	return factory_;
}

//------------------------------------------------------------------------------
// CachedAvatar: Base class for avatars which are requested and are to be cached
//------------------------------------------------------------------------------

class CachedAvatar : public Avatar
{
public:
	CachedAvatar(AvatarFactory* factory)
		: Avatar(factory)
	{ };
	virtual void updateHash(const QString& h);

protected:
	virtual const QString& hash() const { return hash_; }
	virtual void requestAvatar() { }
	virtual void avatarUpdated() { }
	
	virtual bool isCached(const QString& hash);
	virtual void loadFromCache(const QString& hash);
	virtual void saveToCache(const QByteArray& data);

private:
	QString hash_;
};


void CachedAvatar::updateHash(const QString& h)
{
	if (hash_ != h) {
		hash_ = h;
		if (h.isEmpty()) {
			hash_ = "";
			resetImage();
			avatarUpdated();
		}
		else if (isCached(h)) {
			loadFromCache(h);
			avatarUpdated();
		}
		else {
			resetImage();
			avatarUpdated();
			requestAvatar();
		}
	}
}

bool CachedAvatar::isCached(const QString& h)
{
	return QDir(AvatarFactory::getCacheDir()).exists(h);
}

void CachedAvatar::loadFromCache(const QString& h)
{
	// printf("Loading avatar from cache\n");
	hash_ = h;
	setImage(QImage(QDir(AvatarFactory::getCacheDir()).filePath(h)));
	if (pixmap().isNull()) {
		qWarning("CachedAvatar::loadFromCache(): Null pixmap. Unsupported format ?");
	}
}

void CachedAvatar::saveToCache(const QByteArray& data)
{
	QString hash = QCA::Hash("sha1").hashToString(data);
	// printf("Saving %s to cache.\n",hash.latin1());
	QString fn = QDir(AvatarFactory::getCacheDir()).filePath(hash);
	QFile f(fn);
	if (f.open(QIODevice::WriteOnly)) {
		f.write(data);
		f.close();
	}
	else
		printf("Error opening %s for writing.\n", qPrintable(f.fileName()));
	
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  PEPAvatar: PEP Avatars
//------------------------------------------------------------------------------

class PEPAvatar : public CachedAvatar
{
	Q_OBJECT

public:
	PEPAvatar(AvatarFactory* factory, const Jid& jid)
		: CachedAvatar(factory), jid_(jid)
	{ };
	
	void setData(const QString& h, const QString& data) {
		if (h == hash()) {
			QByteArray ba = Base64().stringToArray(data).toByteArray();
			if (!ba.isEmpty()) {
				saveToCache(ba);
				setImage(ba);
				if (pixmap().isNull()) {
					qWarning("PEPAvatar::setData(): Null pixmap. Unsupported format ?");
				}
				emit avatarChanged(jid_);
			}
			else 
				qWarning("PEPAvatar::setData(): Received data is empty. Bad encoding ?");
		}
	}
	
signals:
	void avatarChanged(const Jid&);

protected:
	void requestAvatar() {
		factory()->account()->pepManager()->get(jid_,"urn:xmpp:avatar:data",hash());
	}

	void avatarUpdated() 
		{ emit avatarChanged(jid_); }

private:
	Jid jid_;
};

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// VCardAvatar: Avatars coming from VCards of contacts.
//------------------------------------------------------------------------------

class VCardAvatar : public CachedAvatar
{
	Q_OBJECT

public:
	VCardAvatar(AvatarFactory* factory, const Jid& jid);

signals:
	void avatarChanged(const Jid&);

public slots:
	void receivedVCard();

protected:
	void requestAvatar();
	void avatarUpdated() 
		{ emit avatarChanged(jid_); }

private:
	Jid jid_;
};


VCardAvatar::VCardAvatar(AvatarFactory* factory, const Jid& jid)
	: CachedAvatar(factory), jid_(jid)
{
}

void VCardAvatar::requestAvatar()
{
	VCardFactory::instance()->getVCard(jid_.bare(), factory()->account()->client()->rootTask(), this, SLOT(receivedVCard()));
}

void VCardAvatar::receivedVCard()
{
	const VCard* vcard = VCardFactory::instance()->vcard(jid_);
	if (vcard) {
		saveToCache(vcard->photo());
		setImage(vcard->photo());
		emit avatarChanged(jid_);
	}
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// VCardStaticAvatar: VCard static photo avatar (not published through presence)
//------------------------------------------------------------------------------

class VCardStaticAvatar : public Avatar
{
	Q_OBJECT

public:
	VCardStaticAvatar(AvatarFactory* factory, const Jid& j);

public slots:
	void vcardChanged(const Jid&);

signals:
	void avatarChanged(const Jid&);

private:
	Jid jid_;
};


VCardStaticAvatar::VCardStaticAvatar(AvatarFactory* factory, const Jid& j)
	: Avatar(factory), jid_(j.bare())
{ 
	const VCard* vcard = VCardFactory::instance()->vcard(jid_);
	if (vcard && !vcard->photo().isEmpty())
		setImage(vcard->photo());
	connect(VCardFactory::instance(),SIGNAL(vcardChanged(const Jid&)),SLOT(vcardChanged(const Jid&)));
}

void VCardStaticAvatar::vcardChanged(const Jid& j)
{
	if (j.compare(jid_,false)) {
		const VCard* vcard = VCardFactory::instance()->vcard(jid_);
		if (vcard && !vcard->photo().isEmpty())
			setImage(vcard->photo());
		else
			resetImage();
		emit avatarChanged(jid_);
	}
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// FileAvatar: Avatars coming from local files.
//------------------------------------------------------------------------------

class FileAvatar : public Avatar
{
public:
	FileAvatar(AvatarFactory* factory, const Jid& jid);
	void import(const QString& file);
	void removeFromDisk();
	bool exists();
	QPixmap getPixmap();
	const Jid& getJid() const
		{ return jid_; }

protected:
	bool isDirty() const;
	QString getFileName() const;
	void refresh();
	QDateTime lastModified() const
		{ return lastModified_; }

private:
	QDateTime lastModified_;
	Jid jid_;
};


FileAvatar::FileAvatar(AvatarFactory* factory, const Jid& jid)
	: Avatar(factory), jid_(jid)
{
}

void FileAvatar::import(const QString& file)
{
	if (QFileInfo(file).exists()) {
		QFile source_file(file);
		QFile target_file(getFileName());
		if (source_file.open(QIODevice::ReadOnly) && target_file.open(QIODevice::WriteOnly)) {
			QByteArray ba = source_file.readAll();
			QByteArray data = scaleAvatar(ba);
			target_file.write(data);
		}
	}
}

void FileAvatar::removeFromDisk()
{
	QFile f(getFileName());
	f.remove();
}

bool FileAvatar::exists()
{
	return QFileInfo(getFileName()).exists();
}

QPixmap FileAvatar::getPixmap()
{
	refresh();
	return pixmap();
}

void FileAvatar::refresh()
{
	if (isDirty()) {
		if (QFileInfo(getFileName()).exists()) {
			QImage img(getFileName());
			setImage(QImage(getFileName()));
		}
		else
			resetImage();
	}
}


QString FileAvatar::getFileName() const
{
	QString f = getJid().bare();
	f.replace('@',"_at_");
	return QDir(AvatarFactory::getManualDir()).filePath(f);
}


bool FileAvatar::isDirty() const
{
	return (pixmap().isNull()
			|| !QFileInfo(getFileName()).exists()
			|| QFileInfo(getFileName()).lastModified() > lastModified());
}


//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Avatar factory
//------------------------------------------------------------------------------

AvatarFactory::AvatarFactory(PsiAccount* pa) : pa_(pa)
{
	// Register iconset
	iconset_.addToFactory();

	// Connect signals
	connect(VCardFactory::instance(),SIGNAL(vcardChanged(const Jid&)),this,SLOT(updateAvatar(const Jid&)));
	connect(pa_->client(), SIGNAL(resourceAvailable(const Jid &, const Resource &)), SLOT(resourceAvailable(const Jid &, const Resource &)));

	// PEP
	connect(pa_->pepManager(),SIGNAL(itemPublished(const Jid&, const QString&, const PubSubItem&)),SLOT(itemPublished(const Jid&, const QString&, const PubSubItem&)));
	connect(pa_->pepManager(),SIGNAL(publish_success(const QString&, const PubSubItem&)),SLOT(publish_success(const QString&,const PubSubItem&)));
}

PsiAccount* AvatarFactory::account() const
{
	return pa_;
}

inline static QPixmap ensureSquareAvatar(const QPixmap& original)
{
	if (original.isNull() || original.width() == original.height())
		return original;

	int size = qMax(original.width(), original.height());
	QPixmap square = PixmapUtil::createTransparentPixmap(size, size);

	QPainter p(&square);
	p.drawPixmap((size - original.width()) / 2, (size - original.height()) / 2, original);

	return square;
}

QPixmap AvatarFactory::getAvatar(const Jid& _jid)
{
	// protect from race condition when caller gets
	// deleted as result of avatarChanged() signal
	Jid jid = _jid;

	// Compute the avatar of the user
	Avatar* av = retrieveAvatar(jid);

	// If the avatar changed since the previous request, notify everybody of this
	if (av != active_avatars_[jid.full()]) {
		active_avatars_[jid.full()] = av;
		active_avatars_[jid.bare()] = av;
		emit avatarChanged(jid);
	}

	QPixmap pm = (av ? av->getPixmap() : QPixmap());
	pm = ensureSquareAvatar(pm);

	// Update iconset
	PsiIcon icon;
	icon.setImpix(pm);
	iconset_.setIcon(QString("avatars/%1").arg(jid.bare()),icon);

	return pm;
}

Avatar* AvatarFactory::retrieveAvatar(const Jid& jid)
{
	//printf("Retrieving avatar of %s\n", jid.full().latin1());

	// Try finding a file avatar.
	//printf("File avatar\n");
	if (!file_avatars_.contains(jid.bare())) {
		//printf("File avatar not yet loaded\n");
		file_avatars_[jid.bare()] = new FileAvatar(this, jid);
	}
	//printf("Trying file avatar\n");
	if (!file_avatars_[jid.bare()]->isEmpty())
		return file_avatars_[jid.bare()];
	
	//printf("PEP avatar\n");
	if (pep_avatars_.contains(jid.bare()) && !pep_avatars_[jid.bare()]->isEmpty()) {
		return pep_avatars_[jid.bare()];
	}

	// Try finding a vcard avatar
	//printf("VCard avatar\n");
	if (vcard_avatars_.contains(jid.bare()) && !vcard_avatars_[jid.bare()]->isEmpty()) {
		return vcard_avatars_[jid.bare()];
	}
	
	// Try finding a static vcard avatar
	//printf("Static VCard avatar\n");
	if (!vcard_static_avatars_.contains(jid.bare())) {
		//printf("Static vcard avatar not yet loaded\n");
		vcard_static_avatars_[jid.bare()] = new VCardStaticAvatar(this, jid);
		connect(vcard_static_avatars_[jid.bare()],SIGNAL(avatarChanged(const Jid&)),this,SLOT(updateAvatar(const Jid&)));
	}
	if (!vcard_static_avatars_[jid.bare()]->isEmpty()) {
		return vcard_static_avatars_[jid.bare()];
	}

	return 0;
}

void AvatarFactory::setSelfAvatar(const QString& fileName)
{
	if (!fileName.isEmpty()) {
		QFile avatar_file(fileName);
		if (!avatar_file.open(QIODevice::ReadOnly))
			return;
		
		QByteArray avatar_data = scaleAvatar(avatar_file.readAll());
		QImage avatar_image = QImage::fromData(avatar_data);
		if(!avatar_image.isNull()) {
			// Publish data
			QDomDocument* doc = account()->client()->doc();
			QString hash = Hash("sha1").hashToString(avatar_data);
			QDomElement el = doc->createElement("data");
			el.setAttribute("xmlns","urn:xmpp:avatar:data");
			el.appendChild(doc->createTextNode(Base64().arrayToString(avatar_data)));
			selfAvatarData_ = avatar_data;
			selfAvatarHash_ = hash;
			account()->pepManager()->publish("urn:xmpp:avatar:data",PubSubItem(hash,el));
		}
	}
	else {
		QDomDocument* doc = account()->client()->doc();
		QDomElement meta_el =  doc->createElement("metadata");
		meta_el.setAttribute("xmlns","urn:xmpp:avatar:metadata");
		meta_el.appendChild(doc->createElement("stop"));
		account()->pepManager()->publish("urn:xmpp:avatar:metadata",PubSubItem("current",meta_el));
	}
}

void AvatarFactory::updateAvatar(const Jid& j)
{
	getAvatar(j);
	// FIXME: This signal might be emitted twice (first time from getAvatar()).
	emit avatarChanged(j);
}

void AvatarFactory::importManualAvatar(const Jid& j, const QString& fileName)
{
	FileAvatar(this, j).import(fileName);
	emit avatarChanged(j);
}

void AvatarFactory::removeManualAvatar(const Jid& j)
{
	FileAvatar(this, j).removeFromDisk();
	// TODO: Remove from caches. Maybe create a clearManualAvatar() which
	// removes the file but doesn't remove the avatar from caches (since it'll
	// be created again whenever the FileAvatar is requested)
	emit avatarChanged(j);
}

bool AvatarFactory::hasManualAvatar(const Jid& j)
{
	return FileAvatar(this, j).exists();
}

void AvatarFactory::resourceAvailable(const Jid& jid, const Resource& r)
{
	if (r.status().hasPhotoHash()) {
		QString hash = r.status().photoHash();
		if (!vcard_avatars_.contains(jid.bare())) {
			vcard_avatars_[jid.bare()] = new VCardAvatar(this, jid);
			connect(vcard_avatars_[jid.bare()],SIGNAL(avatarChanged(const Jid&)),this,SLOT(updateAvatar(const Jid&)));
		}
		vcard_avatars_[jid.bare()]->updateHash(hash);
	}
}

QString AvatarFactory::getManualDir()
{
	QDir avatars(pathToProfile(activeProfile) + "/pictures");
	if (!avatars.exists()) {
		QDir profile(pathToProfile(activeProfile));
		profile.mkdir("pictures");
	}
	return avatars.path();
}

QString AvatarFactory::getCacheDir()
{
	QDir avatars(ApplicationInfo::homeDir() + "/avatars");
	if (!avatars.exists()) {
		QDir home(ApplicationInfo::homeDir());
		home.mkdir("avatars");
	}
	return avatars.path();
}

int AvatarFactory::maxAvatarSize()
{
	return MAX_AVATAR_SIZE;
}

void AvatarFactory::itemPublished(const Jid& jid, const QString& n, const PubSubItem& item)
{
	if (n == "urn:xmpp:avatar:data") {
		if (item.payload().tagName() == "data") {
			if (pep_avatars_.contains(jid.bare())) {
				pep_avatars_[jid.bare()]->setData(item.id(),item.payload().text());
			}
		}
		else {
			qWarning("avatars.cpp: Unexpected item payload");
		}
	}
	else if (n == "urn:xmpp:avatar:metadata") {
		if (!pep_avatars_.contains(jid.bare())) {
			pep_avatars_[jid.bare()] = new PEPAvatar(this, jid.bare());
			connect(pep_avatars_[jid.bare()],SIGNAL(avatarChanged(const Jid&)),this, SLOT(updateAvatar(const Jid&)));
		}
		QDomElement e;
		bool found;
		e = findSubTag(item.payload(), "stop", &found);
		if (found) {
			pep_avatars_[jid.bare()]->updateHash("");
		}
		else {
			pep_avatars_[jid.bare()]->updateHash(item.id());
		}
	}	
}

void AvatarFactory::publish_success(const QString& n, const PubSubItem& item)
{
	if (n == "urn:xmpp:avatar:data" && item.id() == selfAvatarHash_) {
		// Publish metadata
		QDomDocument* doc = account()->client()->doc();
		QImage avatar_image = QImage::fromData(selfAvatarData_);
		QDomElement meta_el = doc->createElement("metadata");
		meta_el.setAttribute("xmlns","urn:xmpp:avatar:metadata");
		QDomElement info_el = doc->createElement("info");
		info_el.setAttribute("id",selfAvatarHash_);
		info_el.setAttribute("bytes",avatar_image.numBytes());
		info_el.setAttribute("height",avatar_image.height());
		info_el.setAttribute("width",avatar_image.width());
		info_el.setAttribute("type",image2type(selfAvatarData_));
		meta_el.appendChild(info_el);
		account()->pepManager()->publish("urn:xmpp:avatar:metadata",PubSubItem(selfAvatarHash_,meta_el));
	}
}

//------------------------------------------------------------------------------

#include "avatars.moc"
