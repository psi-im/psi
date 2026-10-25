/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardfactory.h"
#include "iris/xmpp_jid.h"

#include "iris/xmpp_vcard.h"
#include "iris/xmpp_vcard4.h"

#include <QDomDocument>
#include <QFile>
#include <QCryptographicHash>
#include <QMimeDatabase>

// Keep cache read and vCard field merging in one translation unit. Tests use
// these production private methods with real Iris without linking all of Psi.
// Only full, non-MUC requests consult the account's modern PEP vCard store.
// Caps advertised by a client's resources say nothing about that store.
bool VCardFactory::shouldFetchModern(Flags flags)
{
    return !(flags & (PhotoOnly | ForceVCardTemp | MucRoom | MucUser));
}

QString VCardFactory::requestQueueKey(const QString &accountId, quintptr instance,
                                      const Jid &jid, Flags flags)
{
    const auto mode = flags & (PhotoOnly | ForceVCardTemp | MucUser | MucRoom | Silent);
    const QString peer = flags & MucUser ? jid.full() : jid.bare();
    // Account ID is stable; the instance distinguishes an old disconnected
    // account from a replacement during reconnect.
    return accountId + QChar(0x1f) + QString::number(qulonglong(instance), 16)
        + QChar(0x1f) + QString::number(int(mode)) + QChar(0x1f) + peer;
}

XMPP::VCard4::VCard VCardFactory::readCachedVCard(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    XMPP::VCard4::VCard card = XMPP::VCard4::VCard::fromDevice(&file);
    if (!card) {
        file.seek(0);
        QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        const bool parsed = doc.setContent(&file, false);
#else
        const auto parsed = doc.setContent(&file);
#endif
        if (parsed) {
            const XMPP::VCard legacy = XMPP::VCard::fromXml(doc.documentElement());
            if (!legacy.isNull())
                card.fromVCardTemp(legacy);
        }
    }
    return card;
}

// Stable, short and account-specific sidecar names; the merged legacy disk
// path remains unchanged for compatibility. This path is not used on getters
// and does not trigger any network traffic.
namespace {
QString scopedCardFile(const QString &dir, const QString &accountId,
                       const Jid &jid, bool occupant, const QString &suffix)
{
    QByteArray identity = accountId.toUtf8();
    identity.append('\0');
    identity.append((occupant ? jid.full() : jid.bare()).toUtf8());
    const QByteArray digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex();
    return dir + QLatin1String("/source-") + QString::fromLatin1(digest) + suffix;
}
} // namespace

QString VCardFactory::accountCacheKey(const QString &accountId, const Jid &jid, Flags flags)
{
    return accountId + QChar(0x1f) + (flags & MucUser ? jid.full() : jid.bare());
}

QString VCardFactory::mergedCardFile(const QString &dir, const QString &accountId,
                                    const Jid &jid, Flags flags)
{
    return scopedCardFile(dir, accountId, jid, flags & MucUser, QStringLiteral("-merged.xml"));
}

QString VCardFactory::sourceCardFile(const QString &dir, const QString &accountId,
                                    const Jid &jid, Flags flags, bool modern)
{
    return scopedCardFile(dir, accountId, jid, flags & MucUser,
                          modern ? QStringLiteral("-modern.xml") : QStringLiteral("-legacy.xml"));
}

XMPP::VCard4::VCard VCardFactory::mergeSourceCards(XMPP::VCard4::VCard modern,
                                                    const XMPP::VCard4::VCard &legacy)
{
    supplementFromLegacy(modern, legacy);
    return modern;
}

// XEP-0292 and XEP-0054 expose separate account-level stores. Modern
// values take precedence; legacy values supplement missing modern fields.
void VCardFactory::supplementFromLegacy(XMPP::VCard4::VCard &modern, const XMPP::VCard4::VCard &legacy)
{
    if (!legacy)
        return;
    if (!modern || modern.isEmpty()) {
        modern = legacy;
        return;
    }
    // Iris vCard4 uses explicitly shared data: its setters do not detach.
    // Never mutate an unrelated cached copy when supplementing a card.
    modern.detach();
    if (modern.fullName().isEmpty()) modern.setFullName(legacy.fullName());
    if (modern.names().data.isEmpty()) modern.setNames(legacy.names());
    if (modern.nickName().isEmpty()) modern.setNickName(legacy.nickName());
    // Prefer a directly usable legacy PHOTO over a modern URI-only image.
    if (modern.photo().isEmpty()
        || (QByteArray(modern.photo()).isEmpty() && !QByteArray(legacy.photo()).isEmpty()))
        modern.setPhoto(legacy.photo());
    if (modern.emails().isEmpty()) modern.setEmails(legacy.emails());
    if (modern.phones().isEmpty()) modern.setPhones(legacy.phones());
    if (modern.addresses().isEmpty()) modern.setAddresses(legacy.addresses());
    if (modern.org().isEmpty()) modern.setOrg(legacy.org());
    if (modern.title().isEmpty()) modern.setTitle(legacy.title());
    if (modern.role().isEmpty()) modern.setRole(legacy.role());
    if (modern.note().isEmpty()) modern.setNote(legacy.note());
    if (modern.urls().isEmpty()) modern.setUrls(legacy.urls());
    if (QString(modern.bday()).isEmpty()) modern.setBday(legacy.bday());
    if (modern.categories().isEmpty()) modern.setCategories(legacy.categories());
    if (modern.logo().isEmpty()) modern.setLogo(legacy.logo());
    if (modern.impp().isEmpty()) modern.setImpp(legacy.impp());
    if (modern.timeZone().isEmpty()) modern.setTimeZone(legacy.timeZone());
}

// vcard-temp can return the entire contact card even for an avatar refresh.
// Only the PHOTO field may change during such refreshes.
bool VCardFactory::applyLegacyPhoto(XMPP::VCard4::VCard &cached, const XMPP::VCard4::VCard &legacy)
{
    const auto photo = legacy.photo();
    if (photo.isEmpty())
        return false;
    if (cached)
        cached.detach();
    cached.setPhoto(photo);
    return true;
}

// Install actual image bytes in the vCard cache. Iris's VCard4 is explicitly
// shared and its setters do not detach, so never modify another cached copy.
void VCardFactory::rememberMucCard(
    QMap<QString, QHash<QString, VCard4::VCard>> &cards,
    QMap<QString, QQueue<QString>> &order,
    const QString &roomKey, const QString &nick, const VCard4::VCard &vcard)
{
    auto &byNick = cards[roomKey];
    auto it = byNick.find(nick);
    if (it != byNick.end()) {
        *it = vcard; // Refreshing the current occupant does not reorder FIFO.
        return;
    }
    byNick.insert(nick, vcard);
    auto &nicks = order[roomKey];
    nicks.enqueue(nick);
    while (nicks.size() > 3)
        byNick.remove(nicks.dequeue());
}

bool VCardFactory::setEmbeddedPhoto(XMPP::VCard4::VCard &card, const QByteArray &data)
{
    if (data.isEmpty())
        return clearPhoto(card);

    const QString mime = QMimeDatabase().mimeTypeForData(data).name();
    if (!mime.startsWith(QLatin1String("image/")))
        return false;

    if (!card.photo().isEmpty() && QByteArray(card.photo()) == data)
        return false;

    if (card)
        card.detach();
    card.setPhoto(XMPP::VCard4::UriValue(data, mime));
    return true;
}

bool VCardFactory::clearPhoto(XMPP::VCard4::VCard &card)
{
    if (!card || card.photo().isEmpty())
        return false;
    card.detach();
    // A zero-valued UriValue would still create a PHOTO property.
    card.setPhoto(XMPP::VCard4::PAdvUris {});
    return true;
}
