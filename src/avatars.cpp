/*
 * avatars.cpp
 * Copyright (C) 2006-2017  Remko Troncon, Evgeny Khryukin, Sergey Ilinykh
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
#include <QPainterPath>

#include "xmpp_xmlcommon.h"
#include "xmpp_vcard.h"
#include "xmpp_client.h"
#include "xmpp_resource.h"
#include "xmpp_pubsubitem.h"
#include "xmpp_tasks.h"
#include "avatars.h"
#include "applicationinfo.h"
#include "psiaccount.h"
#include "profiles.h"
#include "vcardfactory.h"
#include "pepmanager.h"
#include "pixmaputil.h"
#include "filecache.h"

// we have retine nowdays and various other huge resolutions.96px is not that big already.
// it would be better to scale images according to monitor properties
#define MAX_AVATAR_SIZE 96
//#define MAX_AVATAR_DISPLAY_SIZE 64

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
//  AvatarCache
//
// Features:
//  - keeps avatars in disk cache. but up to 1Mb most necessary avatars are in ram.
//  - keeps together with each icon a list of its users.
//  - disables icon deletion if in use
//
// Known problems:
//  - If we start changing nicknames in muc, items metadata will grow and never
//    released, while this picture is still in use by someone in your mucs
//  - MUC, if someone else enters the room with your nick w/o hash in presence,
//    he will take your picture as well.
//------------------------------------------------------------------------------
class AvatarCache : public FileCache
{
	Q_OBJECT
public:
	enum IconType {
		NoneType,
		AvatarType,
		VCardType,
		AvatarFromVCardType,
		CustomType
	};

	enum OpResult {
		NoData,
		NotChanged,
		Changed,
		UserUpdateRequired
	};

	// design of the structure is matter of priority, not possible ways of avatars receiving.
	struct JidIcons {
		FileCacheItem *vcard = nullptr;  // sha1 of avatar's photo (posibly much bigger than 64x64 regardless of recommended max 96px)
		FileCacheItem *avatar = nullptr; // sha1 of pubsub or vcard avatar (64x64 or less. TODO consider retina)
		FileCacheItem *customAvatar = nullptr; // set by you
		bool avatarFromVCard = false;
	};

	static AvatarCache *instance()
	{
		if (!_instance) {
			_instance = new AvatarCache();
		}
		return _instance;
	}

	JidIcons icons(const QString &jid)
	{
		auto it = jidToIcons.constFind(jid);
		if (it == jidToIcons.constEnd()) {
			return JidIcons();
		}

		if (it->avatar) {
			it->avatar->reborn();
			it->avatar->setSessionUndeletable();
		}
		if (it->vcard) {
			it->vcard->reborn();
			it->vcard->setSessionUndeletable();
		}
		ensureHasAvatar(*it, jid);
		return *it;
	}

	FileCacheItem *activeAvatarIcon(const JidIcons &icons) const
	{
		if (icons.customAvatar) {
			return icons.customAvatar;
		}
		if (icons.avatar) {
			return icons.avatar;
		}
		return nullptr;
	}

	// caches avatars and returns true if it's really something new and valid
	OpResult setIcon(IconType iconType, const QString &jid, const QByteArray &data, const QString &_hash = QString())
	{
		QString metaType = image2type(data);
		if (metaType == QLatin1String("image/unknown")) { // a little bit stupid. It's better to use some enum
			return NoData;
		}

		QByteArray newData;
		if (iconType == VCardType) {
			// do not scale. keep as is. we make avatar from it later
			newData = data;
		} else {
			newData = scaleAvatar(data);
			if (!newData.isSharedWith(data)) { // some new data. so resized
				metaType = QLatin1String("image/png");
			}
		}

		if (newData.isNull())
			return NoData;

		auto &icons = jidToIcons[jid]; // it's fine to make new icons-item here. it's anyway required after all the checks
		if (!canAdd(icons, iconType)) { // for new icons-item we definitely can
			return NotChanged;
		}

		QString hash(_hash);
		if (newData != data || hash.isEmpty()) {
			hash = QString::fromLatin1(QCryptographicHash::hash(newData, QCryptographicHash::Sha1).toHex());
		}

		OpResult res = appendUser(hash, iconType, jid);
		if (res != NoData) { // found and updated dup. no reason to continue
			return res;
		}

		// so we have a new image.
		FileCacheItem *oldActiveItem = activeAvatarIcon(icons);

		QVariantMap md;
		md.insert(QLatin1String("type"), metaType);
		md.insert(QLatin1String("jids"), QStringList() << typedJid(iconType, jid));
		FileCacheItem *item = append(hash, newData, md, 7 * 24 * 3600); // a week

		if (iconType == CustomType) {
			item->setUndeletable();
		}

		FileCacheItem *prevItem = setIconItem(icons, item, iconType);
		if (prevItem) {
			removeUser(prevItem, iconType, jid);
		}
		if (icons.avatarFromVCard && iconType == VCardType) {
			icons.avatar = nullptr; // we have to regenerate it from new vcard
		}

		FileCacheItem *newActiveItem = ensureHasAvatar(icons, jid);
		return oldActiveItem == newActiveItem? Changed : UserUpdateRequired;
	}

	// returns true if something was really deleted
	// keepEmptyIcons - don't remove from jidToIcons even if all icons associated with jid are empty.
	OpResult removeIcon(IconType iconType, const QString &jid, bool keepEmptyIcons = false)
	{
		auto it = jidToIcons.find(jid);
		if (it == jidToIcons.end()) {
			return NoData; // wtf?
		}

		if (!canDel(*it, iconType)) {
			return NoData; // or NotChanged.. no suitable icon to delete
		}

		FileCacheItem *oldActiveIcon = activeAvatarIcon(*it);

		FileCacheItem *prevIcon = setIconItem(it.value(), nullptr, iconType);
		if (!prevIcon) {
			return NotChanged; // already removed?
		}

		if (iconType == VCardType && it.value().avatarFromVCard) {
			removeIcon(AvatarFromVCardType, jid, true);
		}

		FileCacheItem *newActiveIcon;
		if (iconType == AvatarFromVCardType) {
			newActiveIcon = activeAvatarIcon(*it); // we can't restore avatar from vcard. so just get new hash.
		} else {
			newActiveIcon = ensureHasAvatar(*it, jid);
		}

		if (!keepEmptyIcons && areIconsEmpty(*it)) {
			jidToIcons.erase(it);
		}

		if (prevIcon) {
			removeUser(prevIcon, iconType, jid);
		}

		return oldActiveIcon == newActiveIcon? Changed : UserUpdateRequired;
	}

	OpResult appendUser(const QString &hash, IconType iconType, const QString &jid,
	                          JidIcons **iconsOut = 0, FileCacheItem **itemOut = 0)
	{
		auto item = get(hash);
		if (!item) {
			return NoData;
		}
		if (itemOut) {
			*itemOut = item;
		}
		auto &icons = jidToIcons[jid];
		if (iconsOut) {
			*iconsOut = &icons;
		}
		if (!canAdd(icons, iconType)) { // for new icons-item we definitely can
			return NotChanged;
		}
		FileCacheItem *oldActiveIcon = activeAvatarIcon(icons);
		FileCacheItem *prevIcon = setIconItem(icons, item, iconType);
		if (prevIcon == item) {
			return NotChanged; // not changed
		}
		appendUser(item, iconType, jid);
		if (prevIcon) {
			removeUser(prevIcon, iconType, jid);
		}
		if (icons.avatarFromVCard && iconType == VCardType) {
			icons.avatar = nullptr; // we have to regenerate it from new vcard
		}

		FileCacheItem *newActiveIcon = ensureHasAvatar(icons, jid); // for example we have added avatar which was shared with smb.
		return oldActiveIcon == newActiveIcon? Changed : UserUpdateRequired;
	}

protected:

	// all the active items are protected (undeletabe) during session. so it's fine to not update user of changes.
	// from other side we call removeItem of parent class and even if called this function, by design it's only if no users.
	void removeItem(FileCacheItem *item, bool needSync)
	{
		QStringList jids = item->metadata().value(QLatin1String("jids")).toStringList();
		// weh have user of this icon. And this users can use other icons as well. so we have to properly update them
		for (const QString &j : jids) {
			IconType itype = extractIconType(j);
			QString jid = extractIconJid(j);
			auto it = jidToIcons.find(jid);
			if (it == jidToIcons.end()) { // never happens if we maintain cache properly
				continue;
			}
			setIconItem(it.value(), nullptr, itype);

			// if hashes for icon are empty, then it's useless.
			if (areIconsEmpty(*it)) {
				jidToIcons.erase(it);
			}
		}
		FileCache::removeItem(item, needSync);
	}

private:
	AvatarCache()
		: FileCache(AvatarFactory::getCacheDir(), QApplication::instance())
	{
		updateJids();
	}

	bool areIconsEmpty(const JidIcons &icons) const
	{
		return !icons.avatar && !icons.vcard && !icons.customAvatar;
	}

	/**
	 * @brief whether we are allowed to add icon to icons set.
	 * @param icons current set of icons
	 * @param iconType new icon type
	 * @return true if allowed
	 */
	bool canAdd(const JidIcons &icons, IconType iconType) const
	{
		switch (iconType) {
		case VCardType:
		case AvatarType:
		case CustomType:
			return true;
		case AvatarFromVCardType:
			return icons.avatarFromVCard || !icons.avatar; // don't replace avatar with generated stuff
		case NoneType:
		default:
			return false;
		}
	}

	/**
	 * @brief whether we are allowed to remove icon from icons set.
	 * @param icons current set of icons
	 * @param iconType icon type to remove
	 * @return true if allowed
	 */
	bool canDel(const JidIcons &icons, IconType iconType) const
	{
		switch (iconType) {
		case VCardType:
		case CustomType:
			return true;
		case AvatarType:
			return !icons.avatarFromVCard; // don't remove if occupied by generated avatar
		case AvatarFromVCardType:
			return icons.avatarFromVCard; // remove only if occupied by generated avatar
		case NoneType:
		default:
			return false;
		}
	}

	FileCacheItem *ensureHasAvatar(const JidIcons &icons, const QString &jid)
	{
		FileCacheItem *item = activeAvatarIcon(icons);
		if (!item && icons.vcard) {
			setIcon(AvatarFromVCardType, jid, icons.vcard->data()); // it should change our "icons"
			item = icons.avatar;
		}
		return item;
	}

	/**
	 * @brief updates hash in icons corresponding to iconType and returns previous hash
	 *     We come this this function only after checks canAdd/Del. So it's fine to not check again.
	 *
	 * @param icons    - user icons hashes
	 * @param hash     - a new hash corresponding to iconType
	 * @param iconType - type of icon
	 * @return previous hash
	 */
	FileCacheItem* setIconItem(JidIcons &icons, FileCacheItem *item, IconType iconType)
	{
		FileCacheItem *ret = item;

		switch (iconType) {
		case AvatarFromVCardType:
			if (icons.avatar != item) {
				ret = icons.avatar;
				icons.avatar = item;
				icons.avatarFromVCard = (icons.avatar != nullptr);
			}
			break;
		case AvatarType:
			if (icons.avatar != item) {
				ret = icons.avatar;
				icons.avatar = item;
				icons.avatarFromVCard = false;
			}
			break;
		case VCardType:
			ret = icons.vcard;
			icons.vcard = item;
			break;
		case CustomType:
			ret = icons.customAvatar;
			icons.customAvatar = item;
			break;
		case NoneType:
		default:
			break;
		}
		return ret;
	}

	void appendUser(FileCacheItem *item, IconType iconType, const QString &jid)
	{
		QVariantMap md = item->metadata();
		QStringList jids = md.value(QLatin1String("jids")).toStringList();
		jids.append(typedJid(iconType, jid));
		md.insert(QLatin1String("jids"), jids);
		item->setMetadata(md);
	}

	void removeUser(FileCacheItem *item, IconType iconType, const QString &jid)
	{
		auto md = item->metadata();
		QStringList jids = md.value(QLatin1String("jids")).toStringList();
		jids.removeOne(typedJid(iconType, jid));
		if (iconType == AvatarType) {
			jids.removeOne(typedJid(AvatarFromVCardType, jid));
		}

		if (jids.empty()) {
			item->setUndeletable(false); // it's not necessary actually
			FileCache::removeItem(item, true);
		} else {
			md.insert(QLatin1String("jids"), jids);
			item->setMetadata(md);
		}
	}

	IconType extractIconType(const QString &jid)
	{
		if (jid.startsWith(QLatin1String("a:"))) { // pep or custom
			return AvatarType;
		}
		else if (jid.startsWith(QLatin1String("v:"))) {
			return VCardType;
		}
		else if (jid.startsWith(QLatin1String("av:"))) {
			return AvatarFromVCardType;
		}
		else if (jid.startsWith(QLatin1String("ca:"))) {
			return CustomType;
		}
		return NoneType;
	}

	QString extractIconJid(const QString &jid)
	{
		return jid.section(QLatin1Char(':'),1);
	}

	QString typedJid(IconType it, const QString &jid)
	{
		switch (it) {
		case NoneType:
			return QString(); // make compiler happy
		case AvatarType:
			return QLatin1String("a:") + jid;
		case VCardType:
			return QLatin1String("v:") + jid;
		case AvatarFromVCardType:
			return QLatin1String("av:") + jid;
		case CustomType:
			return QLatin1String("ca:") + jid;
		}
		return QString();
	}

	void updateJids()
	{
		QHashIterator<QString, FileCacheItem*> it(_items);
		while (it.hasNext()) {
			it.next();
			QVariantMap md(it.value()->metadata());
			QStringList jids = md.value(QLatin1String("jids")).toStringList();
			if (jids.count() == 0) {
				// no users of the icon
				FileCache::removeItem(it.value(), true);
				continue;
			}
			QMutableStringListIterator jIt(jids);
			bool jidsChanged = false;
			while (jIt.hasNext()) {
				QString jid = jIt.next();
				IconType itype = extractIconType(jid);
				QString realJid = extractIconJid(jid);
				switch (itype) {
				case NoneType:
					jidsChanged = true;
					jIt.remove();
					break;
				case AvatarType: // pep
					jidToIcons[realJid].avatar = it.value();
					break;
				case VCardType:
					jidToIcons[realJid].vcard = it.value();
					break;
				case AvatarFromVCardType:
				{
					auto &ref = jidToIcons[realJid];
					ref.avatar = it.value();
					ref.avatarFromVCard = true;
					break;
				}
				case CustomType:
					jidToIcons[realJid].customAvatar = it.value();
					break;
				}
			}

			if (jidsChanged) {
				md.insert(QLatin1String("jids"), jids);
				it.value()->setMetadata(md);
			}
		}
	}

	QHash<QString,JidIcons> jidToIcons;

	static AvatarCache *_instance;
};

AvatarCache* AvatarCache::_instance = 0;


//------------------------------------------------------------------------------
// Avatar factory
//------------------------------------------------------------------------------

AvatarFactory::AvatarFactory(PsiAccount* pa) : pa_(pa)
{
	// Register iconset
	iconset_.addToFactory();

	// Connect signals
	connect(VCardFactory::instance(),SIGNAL(vcardPhotoAvailable(Jid,bool)),this,SLOT(vcardUpdated(Jid,bool)));
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
	QString bareJid = _jid.bare();

	auto icons = AvatarCache::instance()->icons(bareJid);

	QImage img;
	if (icons.customAvatar) {
		img = QImage::fromData(icons.customAvatar->data());
	}
	if (img.isNull() && icons.avatar) {
		img = QImage::fromData(icons.avatar->data());
	}

	if (img.isNull()) {
		auto vcard = VCardFactory::instance()->vcard(_jid);
		if (vcard.isNull() || vcard.photo().isNull()) {
			return QPixmap();
		}
		QByteArray data = vcard.photo();
		if (AvatarCache::instance()->setIcon(AvatarCache::VCardType, bareJid, data) != AvatarCache::NoData) {
			img = QImage::fromData(data);
		}
	}

	if (img.isNull()) {
		return QPixmap();
	}

	QPixmap pm = QPixmap::fromImage(std::move(img));
	pm = ensureSquareAvatar(pm);

	// Update iconset
	PsiIcon icon;
	icon.setImpix(pm);
	iconset_.setIcon(QString("avatars/%1").arg(bareJid),icon);

	return pm;
}

QPixmap AvatarFactory::getAvatarByHash(const QString &hash)
{
	FileCacheItem *item = AvatarCache::instance()->get(hash, true);
	if (item) {
		// ensureSquareAvatar may be unexpected in some cases. actually almost always and not only here
		return ensureSquareAvatar(QPixmap::fromImage(std::move(QImage::fromData(item->data()))));
	}
	return QPixmap();
}

AvatarFactory::AvatarData AvatarFactory::avatarDataByHash(const QString &hash)
{
	FileCacheItem *item = AvatarCache::instance()->get(hash, true);
	if (item) {
		return AvatarData{item->data(), item->metadata()["type"].toString()};
	}
	return AvatarData();
}

/*!
 * \brief return current active avatar and vcard images hashes
 *    It's expected only for MUC the passed jid will have not empty resource
 *
 * \param jid
 * \return UserHashes
 */
AvatarFactory::UserHashes AvatarFactory::userHashes(const Jid &jid) const
{
	auto icons = AvatarCache::instance()->icons(jid.full());
	if (!icons.vcard) { // hm try to get from vcard factory then
		// we don't call this method often. so it's fine to query vcard factory every time.
		bool isMuc = !jid.resource().isEmpty();
		VCard vcard;
		if (isMuc) {
			vcard = VCardFactory::instance()->mucVcard(jid);
		} else {
			vcard = VCardFactory::instance()->vcard(jid);
		}
		if (!vcard.isNull() && !vcard.photo().isNull()) {
			if (AvatarCache::instance()->setIcon(AvatarCache::VCardType, jid.full(), vcard.photo()) != AvatarCache::NoData) {
				icons = AvatarCache::instance()->icons(jid.full());
			}
		}
	}

	FileCacheItem *active = AvatarCache::instance()->activeAvatarIcon(icons);
	UserHashes ret;
	ret.avatar = active? active->id() : QString();
	ret.vcard  = icons.vcard? icons.vcard->id() : QString();

	return ret;
}

QPixmap AvatarFactory::getMucAvatar(const Jid& _jid)
{
	QString fullJid = _jid.full();

	auto icons = AvatarCache::instance()->icons(fullJid);
	QByteArray data;
	if (icons.avatar) {
		data = icons.avatar->data();
	} else {
		auto vcard = VCardFactory::instance()->mucVcard(_jid);
		if (vcard.isNull() || vcard.photo().isNull()) {
			return QPixmap();
		}
		data = vcard.photo();
		AvatarCache::instance()->setIcon(AvatarCache::VCardType, _jid.full(), data);
	}

	// for mucs icons.avatar is always made of vcard and anything else is not supported. at least for now.
	QImage img(std::move(QImage::fromData(data)));

	if (img.isNull()) {
		return QPixmap();
	}

	QPixmap pm = QPixmap::fromImage(std::move(img));
	pm = ensureSquareAvatar(pm);

	// Update iconset
	PsiIcon icon;
	icon.setImpix(pm);
	iconset_.setIcon(QString("avatars/%1").arg(fullJid),icon); // FIXME do we ever release it?

	return pm;
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
			QString hash = QString::fromLatin1(QCryptographicHash::hash(avatar_data, QCryptographicHash::Sha1).toHex());
			QDomElement el = doc->createElement(PEP_AVATAR_DATA_TN);
			el.setAttribute("xmlns",PEP_AVATAR_DATA_NS);
			el.appendChild(doc->createTextNode(QString::fromLatin1(avatar_data.toBase64())));
			selfAvatarData_ = avatar_data;
			selfAvatarHash_ = hash;
			account()->pepManager()->publish(PEP_AVATAR_DATA_NS,PubSubItem(hash,el));
		}
	}
	else {
		account()->pepManager()->disable(PEP_AVATAR_METADATA_TN, PEP_AVATAR_METADATA_NS, "current");
	}
}

void AvatarFactory::importManualAvatar(const Jid& j, const QString& fileName)
{
	QFile f(fileName);
	if (!(f.open(QIODevice::ReadOnly) && AvatarCache::instance()->setIcon(AvatarCache::CustomType, j.bare(), f.readAll()))) {
		qWarning("Failed to set manual avatar");
	}
	emit avatarChanged(j);
}

void AvatarFactory::removeManualAvatar(const Jid& j)
{
	AvatarCache::instance()->removeIcon(AvatarCache::CustomType, j.bare());
	emit avatarChanged(j);
}

bool AvatarFactory::hasManualAvatar(const Jid& j)
{
	return AvatarCache::instance()->icons(j.bare()).customAvatar;
}

void AvatarFactory::resourceAvailable(const Jid& jid, const Resource& r)
{
	statusUpdate(jid.withResource(QString()), r.status());
}


void AvatarFactory::newMucItem(const Jid &fullJid, const Status &s)
{
	statusUpdate(fullJid, s);
}

void AvatarFactory::statusUpdate(const Jid &jid, const XMPP::Status &status)
{
	// MAJOR ASSUMPTION: jid has resource - it's muc. Do we ever need resources for anything else?
	bool isMuc = !jid.resource().isEmpty();

	if (status.hasPhotoHash()) { // even if it's empty. user adretises something.
		QString hash = status.photoHash();
		QString fullJid = jid.full(); // it's not muc. so just bare jids. probably something like XEP-0316 may break this rule

		if (hash.isEmpty()) { // photo removal
			if (AvatarCache::instance()->removeIcon(AvatarCache::VCardType, fullJid) == AvatarCache::UserUpdateRequired) {
				emit avatarChanged(jid);
			}
		} else {
			auto result = AvatarCache::instance()->appendUser(hash, AvatarCache::VCardType, fullJid);
			if (result == AvatarCache::UserUpdateRequired) {
				emit avatarChanged(jid);
			} else if (result == AvatarCache::NoData) {
				auto task = VCardFactory::instance()->getVCard(
							jid, pa_->client()->rootTask(), this, SLOT(onVcardTaskFinsihed()),
							!isMuc, isMuc, false);
				task->setProperty("hash", hash);
			}
		}
	}
}

QString AvatarFactory::getCacheDir()
{
	QDir avatars(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/avatars");
	if (!avatars.exists()) {
		QDir home(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
		home.mkdir("avatars");
	}
	return avatars.path();
}

int AvatarFactory::maxAvatarSize()
{
	return MAX_AVATAR_SIZE;
}

QPixmap AvatarFactory::roundedAvatar(const QPixmap &pix, int rad, int avSize)
{
	QPixmap avatar_icon;
	QPixmap av = pix;
	if(!pix.isNull()) {
		if (avSize != 0) {
			if (rad != 0) {
				avSize = qMax(avSize, rad*2);
				av = av.scaled(avSize, avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
				int w = av.width(), h = av.height();
				QPainterPath pp;
				pp.addRoundedRect(0, 0, w, h, rad, rad);
				avatar_icon = QPixmap(w, h);
				avatar_icon.fill(QColor(0,0,0,0));
				QPainter mp(&avatar_icon);
				mp.setBackgroundMode(Qt::TransparentMode);
				mp.setRenderHints(QPainter::Antialiasing, true);
				mp.fillPath(pp, QBrush(av));
			}
			else {
				avatar_icon = av.scaled(avSize, avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			}
		}
		else {
			avatar_icon = QPixmap();
		}
	}

	return avatar_icon;
}


void AvatarFactory::onVcardTaskFinsihed()
{
	auto task = dynamic_cast<JT_VCard*>(sender());
	QString hash = task->property("hash").toString();
	if (task->success() && !task->vcard().isNull()) {
		QByteArray ba = task->vcard().photo();
		if (!ba.isNull()) {
			QString fullJid = task->jid().full();
			if (AvatarCache::instance()->setIcon(AvatarCache::VCardType, fullJid, ba, hash) == AvatarCache::UserUpdateRequired) {
				emit avatarChanged(task->jid());
			}
		}
	}
}

void AvatarFactory::vcardUpdated(const Jid &j, bool isMuc)
{
	QByteArray ba;
	QString fullJid;
    XMPP::VCard vcard;
	if (isMuc) {
        vcard = VCardFactory::instance()->mucVcard(j);
		fullJid = j.full();
	} else {
        vcard = VCardFactory::instance()->vcard(j);
		fullJid = j.bare();
    }
    if (!vcard) {
        return; // wtf??
    }
    ba = vcard.photo();
	if (!ba.isEmpty()) {
		if (AvatarCache::instance()->setIcon(AvatarCache::VCardType, fullJid, ba) == AvatarCache::UserUpdateRequired) {
			emit avatarChanged(j);
		}
	}
}

void AvatarFactory::itemPublished(const Jid& jid, const QString& n, const PubSubItem& item)
{
	AvatarCache *cache = AvatarCache::instance();
	QString jidFull = jid.full();
	AvatarCache::OpResult result = AvatarCache::Changed;

	if (n == PEP_AVATAR_DATA_NS) {
		if (item.payload().tagName() == PEP_AVATAR_DATA_TN) {
			// try append user first. since data may be unexpected and we want to save some cpu cycles.
			result = cache->appendUser(item.id(), AvatarCache::AvatarType,
			                                jidFull);
			if (result == AvatarCache::NoData) {
				QByteArray ba = QByteArray::fromBase64(item.payload().text().toLatin1());
				if (!ba.isEmpty()) {
					result = cache->setIcon(AvatarCache::AvatarType, jidFull, ba, item.id());
				}
			}
		}
		else {
			qWarning("avatars.cpp: Unexpected item payload");
		}
	}
	else if (n == PEP_AVATAR_METADATA_NS) {
		QString hash = item.id().toLower();

		if (item.payload().tagName() == QLatin1String(PEP_AVATAR_METADATA_TN) && item.payload().firstChildElement().isNull()) {
			// user wants to stop publishing avatar
			// previously we used "stop" element. now specs are changed
			result = AvatarCache::instance()->removeIcon(AvatarCache::AvatarType, jidFull);
		} else {
			result = cache->appendUser(item.id(), AvatarCache::AvatarType,
											jidFull);
			if (result == AvatarCache::NoData) {
				pa_->pepManager()->get(jid, PEP_AVATAR_DATA_NS, hash);
				return;
			}
		}
	}

	if (result == AvatarCache::UserUpdateRequired) {
		emit avatarChanged(jid);
	}
}

void AvatarFactory::publish_success(const QString& n, const PubSubItem& item)
{
	if (n == PEP_AVATAR_DATA_NS && item.id() == selfAvatarHash_) {
		// Publish metadata
		QDomDocument* doc = account()->client()->doc();
		QImage avatar_image = QImage::fromData(selfAvatarData_);
		QDomElement meta_el = doc->createElement(PEP_AVATAR_METADATA_TN);
		meta_el.setAttribute("xmlns",PEP_AVATAR_METADATA_NS);
		QDomElement info_el = doc->createElement("info");
		info_el.setAttribute("id",selfAvatarHash_);
		info_el.setAttribute("bytes",avatar_image.byteCount());
		info_el.setAttribute("height",avatar_image.height());
		info_el.setAttribute("width",avatar_image.width());
		info_el.setAttribute("type",image2type(selfAvatarData_));
		meta_el.appendChild(info_el);
		account()->pepManager()->publish(PEP_AVATAR_METADATA_NS,PubSubItem(selfAvatarHash_,meta_el));
		selfAvatarData_.clear(); // we don't need it anymore
	}
}

//------------------------------------------------------------------------------

#include "avatars.moc"
