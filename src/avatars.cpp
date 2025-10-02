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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * TODO:
 * - Be more efficient about storing avatars in memory
 * - Move ovotorChanged() to Avatar, and only listen to the active avatars
 *   being changed.
 */

#include "avatars.h"

#include "applicationinfo.h"
#include "filecache.h"
#include "iconset.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_hash.h"
#include "iris/xmpp_pubsubitem.h"
#include "iris/xmpp_resource.h"
#include "iris/xmpp_tasks.h"
#include "iris/xmpp_vcard4.h"
#include "iris/xmpp_xmlcommon.h"
#include "pepmanager.h"
#include "pixmaputil.h"
#include "psiaccount.h"
#include "vcardfactory.h"
#include "xmpp/xmpp-im/xmpp_serverinfomanager.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtCrypto>

// we have retine nowdays and various other huge resolutions.96px is not that big already.
// it would be better to scale images according to monitor properties
#define MAX_AVATAR_SIZE 96
// #define MAX_AVATAR_DISPLAY_SIZE 64

// #define AVATAR_EDBUG 1

//------------------------------------------------------------------------------

namespace {

QByteArray scaleAvatar(const QByteArray &b)
{
    // int maxSize = (LEGOPTS.avatarsSize > MAX_AVATAR_SIZE ? MAX_AVATAR_SIZE : LEGOPTS.avatarsSize);
    int    maxSize = AvatarFactory::maxAvatarSize();
    QImage i       = QImage::fromData(b);
    if (i.isNull()) {
        qWarning("AvatarFactory::scaleAvatar(): Null image (unrecognized format?)");
        return QByteArray();
    } else if (i.width() > maxSize || i.height() > maxSize) {
        QImage     image = i.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray ba;
        QBuffer    buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return ba;
    } else {
        return b;
    }
}

VCardFactory::Flags flags2AvatarFlags(AvatarFactory::Flags flags)
{
    VCardFactory::Flags ret;
    if (flags & AvatarFactory::MucRoom) {
        ret |= VCardFactory::MucRoom;
    }
    if (flags & AvatarFactory::MucUser) {
        ret |= VCardFactory::MucUser;
    }
    if (flags & AvatarFactory::Cache) {
        ret |= VCardFactory::Cache;
    }
    return ret;
}

inline QPixmap ensureSquareAvatar(const QPixmap &original)
{
    if (original.isNull() || original.width() == original.height())
        return original;

    int     size   = qMax(original.width(), original.height());
    QPixmap square = PixmapUtil::createTransparentPixmap(size, size);

    QPainter p(&square);
    p.drawPixmap((size - original.width()) / 2, (size - original.height()) / 2, original);

    return square;
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
class AvatarCache : public FileCache {
    Q_OBJECT
public:
    enum IconType { NoneType, AvatarType, VCardType, AvatarFromVCardType, CustomType };

    enum OpResult {
        NoData,            /* data for gived hash/jid is not in the cache, but it's required */
        NotChanged,        /* data with same hash is already in the cache. or there is nothing to remove */
        Changed,           /* icon changed in the cache but user update is not needed. another is active  */
        UserUpdateRequired /* active icon has changed in the cache and we need to inform the user */
    };

    // design of the structure is matter of priority, not possible ways of avatars receiving.
    struct JidIcons {
        // avatar's photo (posibly much bigger than 64x64 regardless of recommended max 96px)
        FileCacheItem *vcard           = nullptr;
        FileCacheItem *avatar          = nullptr; // pubsub or vcard avatar (64x64 or less. TODO consider retina)
        FileCacheItem *customAvatar    = nullptr; // set by you
        bool           avatarFromVCard = false;
    };

    static AvatarCache *instance()
    {
        if (!_instance) {
            _instance = new AvatarCache();
        }
        return _instance;
    }

    void rememberHash(const Jid &j, const QByteArray &hash) { jid2hash[j] = hash; }
    void removeHash(const Jid &j) { jid2hash.remove(j); }

    void itemPublished(PsiAccount *pa, const Jid &jid, const QString &n, const PubSubItem &item)
    {
        QString               jidFull = jid.full(); // it's always bare
        AvatarCache::OpResult result  = AvatarCache::Changed;

        if (n == PEP_AVATAR_DATA_NS) {
            if (item.payload().tagName() == PEP_AVATAR_DATA_TN) {
                auto hash = QByteArray::fromHex(item.id().toLatin1());
                if (hash.size() < 20)
                    return; // doesn't look like sha1 hash. just ignore it
                            // try append user first. since data may be unexpected and we want to save some cpu cycles.
#ifdef AVATAR_EDBUG
                qDebug() << "append user " << jidFull << AvatarCache::AvatarType;
#endif
                result = appendUser(hash, AvatarCache::AvatarType, jidFull);
                if (result == AvatarCache::NoData) {
                    QByteArray ba = QByteArray::fromBase64(item.payload().text().toLatin1());
                    if (!ba.isEmpty()) {
                        result = setIcon(AvatarCache::AvatarType, jidFull, ba, hash);
                    }
                }
            } else {
                qWarning("avatars.cpp: Unexpected item payload");
            }
        } else if (n == PEP_AVATAR_METADATA_NS && item.payload().tagName() == QLatin1String(PEP_AVATAR_METADATA_TN)) {
            auto info = item.payload().firstChildElement(QLatin1String("info"));
            if (info.isNull()) {
#ifdef AVATAR_EDBUG
                qDebug() << "removeIcon AvatarType " << jidFull << AvatarCache::AvatarType;
#endif
                result  = AvatarCache::instance()->removeIcon(AvatarCache::AvatarType, jidFull);
                auto it = jid2hash.find(jid);
                if (it != jid2hash.end()) {
                    auto vcard_hash = it.value();
                    jid2hash.erase(it);
                    VCardFactory::instance()->ensureVCardPhotoUpdated(pa, jid, {}, vcard_hash);
                }
            } else {
                auto id = item.id().toLatin1();
                if (id == "current") {
                    return; // should we handle singletons? probably previous versions of the xep
                }
                QByteArray hash = QByteArray::fromHex(id);
                if (hash.size() < 20) {
#ifdef AVATAR_EDBUG
                    qDebug() << "not sha1";
#endif
                    return; // doesn't look like sha1 hash. just ignore it
                }

                auto supportedMime = QImageReader::supportedMimeTypes();
                for (; !info.isNull(); info = info.nextSiblingElement(QLatin1String("info"))) {
                    if (!supportedMime.contains(info.attribute(QLatin1String("type")).toLower().toLatin1())) {
                        continue;
                    }
                    if (!info.attribute(QLatin1String("url")).isEmpty()) {
                        continue; // web avatars are not currently supported. TODO but their support is highly expected
                    }
                    if (info.attribute(QLatin1String("id")) != item.id()) {
                        continue; // that's something totally unexpected
                    }
#ifdef AVATAR_EDBUG
                    qDebug() << "appendUser AvatarType " << jidFull;
#endif
                    // found in-band png (by xep84 hash is for png) avatar. So we can make request
                    result = appendUser(hash, AvatarCache::AvatarType, jidFull);
                    if (result == AvatarCache::NoData) {
                        pa->pepManager()->get(jid, PEP_AVATAR_DATA_NS, item.id());
                        return;
                    }
                    break;
                }
            }
        }

        if (result == UserUpdateRequired) {
#ifdef AVATAR_EDBUG
            qDebug() << "remove from iconset and emit avatarChanged on itemPublished" << jidFull;
#endif
            iconset_.removeIcon(QString(QLatin1String("avatars/%1")).arg(jidFull));
            emit avatarChanged(jidFull);
        }
    }

    OpResult ensureVCardUpdated(const Jid &jid, const QByteArray &hash, AvatarFactory::Flags flags)
    {
        QString  fullJid = (flags & AvatarFactory::MucUser) ? jid.full() : jid.bare();
        OpResult result;

        if (hash.isEmpty()) { // photo removal
            result = AvatarCache::instance()->removeIcon(AvatarCache::VCardType, fullJid);
        } else {
            result = appendUser(hash, AvatarCache::VCardType, fullJid);
        }
        if (result == AvatarCache::UserUpdateRequired) {
#ifdef AVATAR_EDBUG
            qDebug() << "remove from iconset and emit avatarChanged. ensureVCardUpdated hash=" << hash.toHex()
                     << fullJid;
#endif
            iconset_.removeIcon(QString(QLatin1String("avatars/%1")).arg(fullJid));
            emit avatarChanged(fullJid);
        }

        return result;
    }

    void importManualAvatar(const Jid &j, const QString &fileName)
    {
        QFile f(fileName);
        if (!(f.open(QIODevice::ReadOnly)
              && AvatarCache::instance()->setIcon(AvatarCache::CustomType, j.bare(), f.readAll()))) {
            qWarning("Failed to set manual avatar");
        }
#ifdef AVATAR_EDBUG
        qDebug() << "remove from iconset and emit avatarChanged. importManualAvatar" << j.bare();
#endif
        iconset_.removeIcon(QString(QLatin1String("avatars/%1")).arg(j.bare()));
        emit avatarChanged(j);
    }

    void removeManualAvatar(const Jid &j)
    {
        if (AvatarCache::instance()->removeIcon(AvatarCache::CustomType, j.bare()) == AvatarCache::UserUpdateRequired) {
#ifdef AVATAR_EDBUG
            qDebug() << "remove from iconset and emit avatarChanged. removeManualAvatar" << j.bare();
#endif
            iconset_.removeIcon(QString(QLatin1String("avatars/%1")).arg(j.bare()));
            emit avatarChanged(j);
        }
    }

    QPixmap getAvatar(const Jid &_jid)
    {
        QString bareJid  = _jid.bare();
        QString iconName = QString("avatars/%1").arg(bareJid);
        auto    iconp    = iconset_.icon(iconName);
        if (iconp) {
#ifdef AVATAR_EDBUG
            qDebug() << "return icons" << iconName << "from iconset for jid" << bareJid;
#endif
            return iconp->pixmap();
        }

        auto   icons = this->icons(bareJid);
        QImage img;
        if (icons.customAvatar) {
            img = QImage::fromData(icons.customAvatar->data());
            if (img.isNull()) {
                removeIcon(AvatarCache::CustomType, bareJid);
            }
        }
        if (img.isNull() && icons.avatar) {
            img = QImage::fromData(icons.avatar->data());
            if (img.isNull()) {
                removeIcon(AvatarCache::AvatarType, bareJid);
            }
        }

        if (img.isNull()) {
#ifdef AVATAR_EDBUG
            qDebug() << "return from vcardfactory for jid " << _jid.full();
#endif
            auto vcard = VCardFactory::instance()->vcard(_jid);
            if (vcard.isNull() || QByteArray(vcard.photo()).isNull()) {
                return QPixmap();
            }
            QByteArray data = vcard.photo();
            if (setIcon(AvatarCache::VCardType, bareJid, data) != AvatarCache::NoData) {
                auto item = this->icons(bareJid).avatar;
                if (!item)
                    qWarning("Avatars cache is damaged");
                else
                    img = QImage::fromData(item->data()); // from scaled avatar
            }
        }

        if (img.isNull()) {
            return QPixmap();
        }

        QPixmap pm = QPixmap::fromImage(std::move(img));
        pm         = ensureSquareAvatar(pm);

        // Update iconset
        PsiIcon icon;
        icon.setImpix(pm);
#ifdef AVATAR_EDBUG
        qDebug() << "setting icon to iconset " << iconName << "=" << pm;
#endif
        iconset_.setIcon(iconName, icon);

        return pm;
    }

    QPixmap getMucAvatar(const Jid &_jid)
    {
        QString fullJid = _jid.full();

        QString iconName = QString("avatars/%1").arg(fullJid);
        auto    iconp    = iconset_.icon(iconName);
        if (iconp) {
            return iconp->pixmap();
        }

        auto       icons = AvatarCache::instance()->icons(fullJid);
        QByteArray data;
        if (!icons.avatar) {
            auto vcard = VCardFactory::instance()->mucVcard(_jid);
            if (vcard.isNull() || QByteArray(vcard.photo()).isNull()) {
                return QPixmap();
            }
            data = vcard.photo();
            AvatarCache::instance()->setIcon(AvatarCache::VCardType, _jid.full(), data);
            icons = AvatarCache::instance()->icons(fullJid); // should return scaled copy
        }

        if (icons.avatar) {
            data = icons.avatar->data();
        }

        // for mucs icons.avatar is always made of vcard and anything else is not supported. at least for now.
        QImage img(QImage::fromData(data));

        if (img.isNull()) {
            return QPixmap();
        }

        QPixmap pm = QPixmap::fromImage(std::move(img));
        pm         = ensureSquareAvatar(pm);

        // Update iconset
        PsiIcon icon;
        icon.setImpix(pm);
#ifdef AVATAR_EDBUG
        qDebug() << "setting icon " << QString("avatars/%1").arg(fullJid) << "=" << pm;
#endif
        iconset_.setIcon(QString("avatars/%1").arg(fullJid), icon); // FIXME do we ever release it?

        return pm;
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
    OpResult setIcon(IconType iconType, const QString &jid, const QByteArray &data,
                     const QByteArray &_hash = QByteArray())
    {
        QString metaType = image2type(data);
        if (metaType.isEmpty()) { // a little bit stupid. It's better to use some enum
            return NoData;
        }

        QByteArray newData;
        if (iconType == VCardType) {
            // do not scale. keep as is. we make avatar from it later
            newData = data;
        } else {
            newData = scaleAvatar(data);
            if (newData.isNull()) { // the image failed to load to QImage. likely supported but broken image
                qWarning("got broken image (type=%d) from %s", int(iconType), qPrintable(jid));
                return NoData;
            }
            if (!newData.isSharedWith(data)) { // some new data. so resized
                metaType = QLatin1String("image/png");
            }
        }

        if (newData.isNull())
            return NoData;

        auto &icons
            = jidToIcons[jid]; // it's fine to make new icons-item here. it's anyway required after all the checks
        if (!canAdd(icons, iconType)) { // for new icons-item we definitely can
            return NotChanged;
        }

        QByteArray hash(_hash);
        if (hash.isEmpty()) { // ignore "newData != data ||" since we need hashing by original hash
            hash = QCryptographicHash::hash(newData, QCryptographicHash::Sha1);
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
        FileCacheItem *item = append(XMPP::Hash { XMPP::Hash::Sha1, hash }, newData, md, 7 * 24 * 3600); // a week

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
        return oldActiveItem == newActiveItem ? Changed : UserUpdateRequired;
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
            return NotChanged; //  no suitable icon to delete
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

        return oldActiveIcon == newActiveIcon ? Changed : UserUpdateRequired;
    }

signals:
    void avatarChanged(const Jid &);

protected:
    // all the active items are protected (undeletabe) during session. so it's fine to not update user of changes.
    // from other side we call removeItem of parent class and even if called this function, by design it's only if
    // no users.
    void removeItem(FileCacheItem *item, bool needSync)
    {
        QStringList jids = item->metadata().value(QLatin1String("jids")).toStringList();
        // weh have user of this icon. And this users can use other icons as well. so we have to properly update
        // them
        for (const QString &j : std::as_const(jids)) {
            IconType itype = extractIconType(j);
            QString  jid   = extractIconJid(j);
            auto     it    = jidToIcons.find(jid);
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
    AvatarCache() : FileCache(AvatarFactory::getCacheDir())
    {
        // Register iconset
        iconset_.setName(QLatin1String("Avatars"));
        iconset_.addToFactory(); // the factory will own the iconset
        updateJids();

        connect(VCardFactory::instance(), &VCardFactory::vcardChanged, this,
                [this](const Jid &j, VCardFactory::Flags flags) {
                    QByteArray ba;
                    QString    fullJid;
                    auto       vcard = VCardFactory::instance()->vcard(j, flags);
                    if (flags & VCardFactory::MucUser) {
                        fullJid = j.full();
                    } else {
                        fullJid = j.bare();
                    }
                    if (!vcard) {
                        return; // wtf??
                    }
                    ba = vcard.photo();
                    OpResult result;
                    if (ba.isEmpty()) {
#ifdef AVATAR_EDBUG
                        qDebug() << "removeIcon VCardType from cache for " << fullJid;
#endif
                        result = AvatarCache::instance()->removeIcon(AvatarCache::VCardType, fullJid);
                    } else {
#ifdef AVATAR_EDBUG
                        qDebug() << "setIcon VCardType to cache for " << fullJid;
#endif
                        result = AvatarCache::instance()->setIcon(AvatarCache::VCardType, fullJid, ba);
                    }
                    if (result == UserUpdateRequired) {
#ifdef AVATAR_EDBUG
                        qDebug() << "removing icon from iconset:" << QString(QLatin1String("avatars/%1")).arg(fullJid);
#endif
                        iconset_.removeIcon(QString(QLatin1String("avatars/%1")).arg(fullJid));
                        emit avatarChanged(j);
                    }
                });
    }

    bool areIconsEmpty(const JidIcons &icons) const { return !icons.avatar && !icons.vcard && !icons.customAvatar; }

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
     *     We come this this function only after checks canAdd/canDel. So it's fine to not check again.
     *
     * @param icons    - user icons hashes
     * @param hash     - a new hash corresponding to iconType
     * @param iconType - type of icon
     * @return previous hash
     */
    FileCacheItem *setIconItem(JidIcons &icons, FileCacheItem *item, IconType iconType)
    {
        FileCacheItem *ret = item;

        switch (iconType) {
        case AvatarFromVCardType:
            if (icons.avatar != item) {
                ret                   = icons.avatar;
                icons.avatar          = item;
                icons.avatarFromVCard = (icons.avatar != nullptr);
            }
            break;
        case AvatarType:
            if (icons.avatar != item) {
                ret                   = icons.avatar;
                icons.avatar          = item;
                icons.avatarFromVCard = false;
            }
            break;
        case VCardType:
            ret         = icons.vcard;
            icons.vcard = item;
            break;
        case CustomType:
            ret                = icons.customAvatar;
            icons.customAvatar = item;
            break;
        case NoneType:
        default:
            break;
        }
        return ret;
    }

    OpResult appendUser(const QByteArray &hash, IconType iconType, const QString &jid)
    {
        auto item = get(XMPP::Hash(XMPP::Hash::Sha1, hash));
        if (!item) {
            return NoData;
        }

        auto &icons = jidToIcons[jid];
        if (!canAdd(icons, iconType)) { // for new icons-item we definitely can
            return NotChanged;
        }

        FileCacheItem *oldActiveIcon = activeAvatarIcon(icons);
        FileCacheItem *prevIcon      = setIconItem(icons, item, iconType);
        if (prevIcon == item) {
            return NotChanged; // not changed
        }
        {
            QVariantMap md   = item->metadata();
            QStringList jids = md.value(QLatin1String("jids")).toStringList();
            jids.append(typedJid(iconType, jid));
            md.insert(QLatin1String("jids"), jids);
            item->setMetadata(md);
            lazySync();
        }

        if (prevIcon) {
            removeUser(prevIcon, iconType, jid);
        }
        if (icons.avatarFromVCard && iconType == VCardType) {
            icons.avatar = nullptr; // we have to regenerate it from new vcard
        }

        FileCacheItem *newActiveIcon
            = ensureHasAvatar(icons, jid); // for example we have added avatar which was shared with smb.

        return oldActiveIcon == newActiveIcon ? Changed : UserUpdateRequired;
    }

    void removeUser(FileCacheItem *item, IconType iconType, const QString &jid)
    {
        auto        md   = item->metadata();
        QStringList jids = md.value(QLatin1String("jids")).toStringList();
        jids.removeOne(typedJid(iconType, jid));
        if (iconType == VCardType) {
            jids.removeOne(typedJid(AvatarFromVCardType, jid));
        }

        if (jids.empty()) {
            item->setUndeletable(false); // it's not necessary actually
            FileCache::removeItem(item, true);
        } else {
            md.insert(QLatin1String("jids"), jids);
            item->setMetadata(md);
            lazySync();
        }
    }

    IconType extractIconType(const QString &jid)
    {
        if (jid.startsWith(QLatin1String("a:"))) { // pep or custom
            return AvatarType;
        } else if (jid.startsWith(QLatin1String("v:"))) {
            return VCardType;
        } else if (jid.startsWith(QLatin1String("av:"))) {
            return AvatarFromVCardType;
        } else if (jid.startsWith(QLatin1String("ca:"))) {
            return CustomType;
        }
        return NoneType;
    }

    QString extractIconJid(const QString &jid) { return jid.section(QLatin1Char(':'), 1); }

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
        QHashIterator<XMPP::Hash, FileCacheItem *> it(_items);
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
            bool                       jidsChanged = false;
            while (jIt.hasNext()) {
                QString  jid     = jIt.next();
                IconType itype   = extractIconType(jid);
                QString  realJid = extractIconJid(jid);

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
                case AvatarFromVCardType: {
                    auto &ref           = jidToIcons[realJid];
                    ref.avatar          = it.value();
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
                lazySync();
            }
        }
    }

    QHash<QString, JidIcons> jidToIcons;
    QHash<Jid, QByteArray>   jid2hash;
    Iconset                  iconset_;

    static AvatarCache *_instance;
};

AvatarCache *AvatarCache::_instance = nullptr;

//------------------------------------------------------------------------------
// Avatar factory
//------------------------------------------------------------------------------

class AvatarFactory::Private {
public:
    QByteArray selfAvatarData_;
    QString    selfAvatarHash_;

    PsiAccount *pa_;
};

AvatarFactory::AvatarFactory(PsiAccount *pa) : d(new Private)
{
    d->pa_ = pa;

    // Connect signals
    connect(AvatarCache::instance(), &AvatarCache::avatarChanged, this, &AvatarFactory::avatarChanged);

    connect(d->pa_->client(), &XMPP::Client::resourceAvailable, this, [this](const Jid &jid, const Resource &r) {
        if (jid.compare(d->pa_->jid(), false)) {
            return; // to avoid endless recursion with vcard update
        }
        statusUpdate(jid.withResource(QString()), r.status());
    });

    // PEP
    connect(d->pa_->pepManager(), &PEPManager::itemPublished, this,
            [this](const Jid &jid, const QString &n, const PubSubItem &item) {
                AvatarCache::instance()->itemPublished(d->pa_, jid, n, item);
            });
    connect(d->pa_->pepManager(), &PEPManager::publish_success, this, &AvatarFactory::publish_success);
}

AvatarFactory::~AvatarFactory() { delete d; }

PsiAccount *AvatarFactory::account() const { return d->pa_; }

QPixmap AvatarFactory::getAvatar(const Jid &_jid) { return AvatarCache::instance()->getAvatar(_jid); }
QPixmap AvatarFactory::getMucAvatar(const Jid &_jid) { return AvatarCache::instance()->getMucAvatar(_jid); }

#if 0
QPixmap AvatarFactory::getAvatarByHash(const QString &hash)
{
    FileCacheItem *item = AvatarCache::instance()->get(hash, true);
    if (item) {
        // ensureSquareAvatar may be unexpected in some cases. actually almost always and not only here
        return ensureSquareAvatar(QPixmap::fromImage(std::move(QImage::fromData(item->data()))));
    }
    return QPixmap();
}
#endif
AvatarFactory::AvatarData AvatarFactory::avatarDataByHash(const QByteArray &hash)
{
    FileCacheItem *item = AvatarCache::instance()->get({ XMPP::Hash::Sha1, hash }, true);
    if (item) {
        return AvatarData { item->data(), item->metadata().value("type").toString() };
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
        auto flags = jid.resource().isEmpty() ? VCardFactory::Flags {} : VCardFactory::MucUser;
        auto vcard = VCardFactory::instance()->vcard(jid, flags);
        if (!vcard.isNull() && !QByteArray(vcard.photo()).isNull()) {
            if (AvatarCache::instance()->setIcon(AvatarCache::VCardType, jid.full(), vcard.photo())
                != AvatarCache::NoData) {
                icons = AvatarCache::instance()->icons(jid.full());
            }
        }
    }

    FileCacheItem *active = AvatarCache::instance()->activeAvatarIcon(icons);
    UserHashes     ret;
    ret.avatar = active ? active->id().data() : QByteArray();
    ret.vcard  = icons.vcard ? icons.vcard->id().data() : QByteArray();

    return ret;
}

void AvatarFactory::setSelfAvatar(const QString &fileName)
{
    if (!fileName.isEmpty()) {
        setSelfAvatar(QImage(fileName));
    } else {
        account()->pepManager()->disable(PEP_AVATAR_METADATA_TN, PEP_AVATAR_METADATA_NS, {});
    }
}

void AvatarFactory::setSelfAvatar(const QImage &image)
{
    if (image.isNull()) {
        account()->pepManager()->disable(PEP_AVATAR_METADATA_TN, PEP_AVATAR_METADATA_NS, {});
    } else {
        // Publish data

        QByteArray data;
        QBuffer    buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        auto maxSz = maxAvatarSize();
        image.scaled(image.size().boundedTo(QSize(maxSz, maxSz)), Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .save(&buffer, "PNG", 0);
        auto    hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        QString id   = QString::fromLatin1(hash.toHex());

        QDomDocument *doc = account()->client()->doc();
        QDomElement   el  = doc->createElementNS(PEP_AVATAR_DATA_NS, PEP_AVATAR_DATA_TN);
        el.appendChild(doc->createTextNode(QString::fromLatin1(data.toBase64())));
        d->selfAvatarData_ = data;
        d->selfAvatarHash_ = id;
        account()->pepManager()->publish(PEP_AVATAR_DATA_NS, PubSubItem(id, el), PEPManager::Access::Open);
    }
}

void AvatarFactory::importManualAvatar(const Jid &j, const QString &fileName)
{
    AvatarCache::instance()->importManualAvatar(j, fileName);
}

void AvatarFactory::removeManualAvatar(const Jid &j) { AvatarCache::instance()->removeManualAvatar(j); }

bool AvatarFactory::hasManualAvatar(const Jid &j)
{
    return bool(AvatarCache::instance()->icons(j.bare()).customAvatar);
}

void AvatarFactory::statusUpdate(const Jid &jid, const XMPP::Status &status, Flags flags)
{
    if (status.type() == Status::Offline) {
        AvatarCache::instance()->removeHash((flags & MucUser) ? jid : jid.withResource({}));
    } else {
        if (status.photoHash()) { // even if it's empty. user adretises something.
            auto hash = *status.photoHash();
            ensureVCardUpdated(jid, hash, flags);
        }
    }
}

void AvatarFactory::ensureVCardUpdated(const Jid &jid, const QByteArray &hash, Flags flags)
{
    auto xj = jid;
    if (!(flags & MucUser)) {
        xj = jid.withResource({});
    }
    auto result = AvatarCache::instance()->ensureVCardUpdated(xj, hash, flags);

    if (result == AvatarCache::NoData) {
        VCardFactory::instance()->ensureVCardPhotoUpdated(d->pa_, xj, flags2AvatarFlags(flags), hash);
    } else {
        AvatarCache::instance()->rememberHash(xj, hash);
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

int AvatarFactory::maxAvatarSize() { return MAX_AVATAR_SIZE; }

/**
 * @brief Scales input pixmap and rounds its corners with given radius
 * @param pix Input pixmap
 * @param rad Radius in pixels
 * @param avSize max height or width
 * @return rounded pixmap
 */
QPixmap AvatarFactory::roundedAvatar(const QPixmap &pix, int rad, int avSize)
{
    QPixmap avatar_icon;
    QPixmap av = pix;
    if (!pix.isNull()) {
        if (avSize != 0) {
            if (rad != 0) {
                avSize         = qMax(avSize, rad * 2);
                av             = av.scaled(avSize, avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                int          w = av.width(), h = av.height();
                QPainterPath pp;
                pp.addRoundedRect(0, 0, w, h, rad, rad);
                avatar_icon = QPixmap(w, h);
                avatar_icon.fill(QColor(0, 0, 0, 0));
                QPainter mp(&avatar_icon);
                mp.setBackgroundMode(Qt::TransparentMode);
                mp.setRenderHints(QPainter::Antialiasing, true);
                mp.fillPath(pp, QBrush(av));
            } else {
                avatar_icon = av.scaled(avSize, avSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        } else {
            avatar_icon = QPixmap();
        }
    }

    return avatar_icon;
}

void AvatarFactory::publish_success(const QString &n, const PubSubItem &item)
{
    if (n == PEP_AVATAR_DATA_NS && item.id() == d->selfAvatarHash_) {
        // Publish metadata
        QDomDocument *doc          = account()->client()->doc();
        QImage        avatar_image = QImage::fromData(d->selfAvatarData_);
        QDomElement   meta_el      = doc->createElementNS(PEP_AVATAR_METADATA_NS, PEP_AVATAR_METADATA_TN);
        QDomElement   info_el      = doc->createElement("info");
        info_el.setAttribute("id", d->selfAvatarHash_);
        info_el.setAttribute("bytes", avatar_image.sizeInBytes());
        info_el.setAttribute("height", avatar_image.height());
        info_el.setAttribute("width", avatar_image.width());
        info_el.setAttribute("type", image2type(d->selfAvatarData_));
        meta_el.appendChild(info_el);
        account()->pepManager()->publish(PEP_AVATAR_METADATA_NS, PubSubItem(d->selfAvatarHash_, meta_el),
                                         PEPManager::Access::Open);
        if (account()->client()->serverInfoManager()->accountFeatures().hasAvatarConversion()) {
            VCardFactory::instance()->setPhoto(account()->jid(), d->selfAvatarData_, VCardFactory::Silent);
        }
        d->selfAvatarData_.clear(); // we don't need it anymore
    } else if (n == PEP_AVATAR_METADATA_NS) {
        bool removed = item.payload().firstChildElement("metadata").firstChildElement("info").isNull();
        if (account()->client()->serverInfoManager()->accountFeatures().hasAvatarConversion() && removed) {
            VCardFactory::instance()->deletePhoto(account()->jid(), VCardFactory::Silent);
        }
    }
}

//------------------------------------------------------------------------------

#include "avatars.moc"
