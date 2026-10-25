/*
 * vcardfactory.h - class for caching vCards
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef VCARDFACTORY_H
#define VCARDFACTORY_H

#include "avatarepochpolicy.h"

#include <QHash>
#include <QMap>
#include <QQueue>
#include <QObject>
#include <QStringList>

#include <memory>

class PsiAccount;

namespace XMPP {
class JT_VCard;
class Jid;
class Task;
class VCard;
namespace VCard4 {
    class VCard;
}
}
using namespace XMPP;

class VCardRequest;

class VCardFactory : public QObject {
    Q_OBJECT

public:
    enum Flag { MucRoom = 0x1, MucUser = 0x2, Cache = 0x4, ForceVCardTemp = 0x8, Silent = 0x10, PhotoOnly = 0x20 };
    Q_DECLARE_FLAGS(Flags, Flag);

    static VCardFactory *instance();
    VCard4::VCard        vcard(const Jid &, Flags flags = {}); // Legacy unscoped compatibility
    const VCard4::VCard  mucVcard(const Jid &j) const;
    // Account-aware lookups never read another account's card or disk cache.
    VCard4::VCard        vcard(PsiAccount *account, const Jid &, Flags flags = {});
    VCard4::VCard        mucVcard(PsiAccount *account, const Jid &j) const;
    // Called on MUC occupant departure and every room rejoin. Pending
    // pre-reset IQs are fenced off by generation, never replayed into cache.
    void invalidateMucOccupant(PsiAccount *account, const Jid &occupant);
    void resetMucRoom(PsiAccount *account, const Jid &room);
    quint64 mucGeneration(PsiAccount *account, const Jid &occupant) const;
    quint64 mucRoomGeneration(PsiAccount *account, const Jid &room) const;
    quint64 accountGeneration(PsiAccount *account) const;
    void invalidateAccountSession(PsiAccount *account);


    Task *setVCard(PsiAccount *account, const VCard4::VCard &v, const Jid &targetJid, VCardFactory::Flags flags);
    VCardRequest *getVCard(PsiAccount *account, const Jid &, VCardFactory::Flags flags = {});
    // Fetch only the legacy vcard-temp store, which owns XEP-0153 photo hashes.
    // The caller still receives the full vcard-temp IQ payload; only the
    // photo field is applied to the cached vCard4 representation.
    VCardRequest *getVCardPhoto(PsiAccount *account, const Jid &, VCardFactory::Flags flags = {});

    void setPhoto(const Jid &j, const QByteArray &photo, Flags flags);
    void deletePhoto(const Jid &j, Flags flags);
    void setPhoto(PsiAccount *account, const Jid &j, const QByteArray &photo, Flags flags);
    void deletePhoto(PsiAccount *account, const Jid &j, Flags flags);

    // 1. check if it's needed to do a request,
    // 2. enqueue request if necessary (no vcard, or if hash doesn't match)
    // 3. vcardChanged() will be sent as usually when vcard is updated
    void ensureVCardPhotoUpdated(PsiAccount *acc, const Jid &jid, Flags flags, const QByteArray &newPhotoHash);

signals:
    void vcardChanged(const Jid &, VCardFactory::Flags);
    // Internal cache notification when a PHOTO is changed with Silent: other
    // vCard consumers still only receive vcardChanged for non-Silent writes.
    void photoChanged(const Jid &, VCardFactory::Flags);
    void vcardChangedScoped(PsiAccount *, const Jid &, VCardFactory::Flags);
    void photoChangedScoped(PsiAccount *, const Jid &, VCardFactory::Flags);

protected:
    void checkLimit(const QString &jid, const VCard4::VCard &vcard);

private:
    VCardFactory();
    ~VCardFactory();
    friend class VCardRequest;
    void saveVCard(const Jid &, const VCard4::VCard &, VCardFactory::Flags flags);
    void saveVCard(PsiAccount *account, const Jid &, const VCard4::VCard &, Flags flags);
    static QString accountCacheKey(const QString &accountId, const Jid &, Flags flags);
    static QString mergedCardFile(const QString &dir, const QString &accountId,
                                  const Jid &, Flags flags);

    // Keep the disk parser out of headers; tests call the same production
    // reader without needing to link all of PsiAccount and PEPManager.
    static VCard4::VCard readCachedVCard(const QString &fileName);
    static bool shouldFetchModern(Flags flags);
    // Queue keys separate accounts, reconnecting instances and protocol
    // modes. This prevents one account's requests from using another's IQ.
    static QString requestQueueKey(const QString &accountId, quintptr instance,
                                   const Jid &jid, Flags flags);
    static void supplementFromLegacy(VCard4::VCard &modern, const VCard4::VCard &legacy);
    static bool applyLegacyPhoto(VCard4::VCard &cached, const VCard4::VCard &legacy);
    // The merged display card cannot establish whether PHOTO originated from
    // PEP vCard4 or legacy vcard-temp. Store the sources independently.
    static QString sourceCardFile(const QString &dir, const QString &accountId,
                                  const Jid &jid, Flags flags, bool modern);
    static VCard4::VCard mergeSourceCards(VCard4::VCard modern, const VCard4::VCard &legacy);
    void saveSourceCard(const QString &accountId, const Jid &jid, Flags flags,
                        bool modern, const VCard4::VCard &card);
    VCard4::VCard readSourceCard(const QString &accountId, const Jid &jid, Flags flags, bool modern);

    static bool setEmbeddedPhoto(VCard4::VCard &card, const QByteArray &data);
    static bool clearPhoto(VCard4::VCard &card);
    static void rememberMucCard(
        QMap<QString, QHash<QString, VCard4::VCard>> &cards,
        QMap<QString, QQueue<QString>> &order,
        const QString &roomKey, const QString &nick, const VCard4::VCard &vcard);

    friend class VCardDiskCacheTest;
    friend class VCardRequestMergeTest;

    static VCardFactory         *instance_;
    const int                    dictSize_;
    QStringList                  vcardList_;
    QMap<QString, VCard4::VCard> vcardDict_;

    // QHash in case of big mucs mucBareJid => {resoure => vcard}
    QMap<QString, QHash<QString, VCard4::VCard>> mucVcardDict_;

    // to limit the hash above. this one keeps ordered resource. mucBareJid => resource_list
    QMap<QString, QQueue<QString>> lastMucVcards_;
    AvatarEpochPolicy             occupantEpochs_;
    AvatarEpochPolicy             accountEpochs_;

    class QueuedLoader;
    QueuedLoader *queuedLoader_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VCardFactory::Flags)

class VCardRequest : public QObject {
    Q_OBJECT
public:
    VCardRequest(PsiAccount *account, const Jid &, VCardFactory::Flags flags);
    ~VCardRequest();

    Jid                &jid() const;
    VCardFactory::Flags flags() const;

    bool execute();
    void merge(PsiAccount *account, const Jid &, VCardFactory::Flags flags);

    // result stuff
    bool          success() const; // item-not-found is considered success but vcard will be null
    VCard4::VCard vcard() const;
    const QString &accountId() const;
    PsiAccount *account() const;
    VCard4::VCard modernVcard() const;
    VCard4::VCard legacyVcard() const;
    bool modernAnswered() const;
    bool legacyAnswered() const;
    quint64 requestedOccupantEpoch() const;
    quint64 requestedRoomEpoch() const;
    quint64 requestedAccountEpoch() const;
    bool wasQueuedForAccount(PsiAccount *account) const;
    bool currentEpoch() const;
    bool wasQueuedFor(PsiAccount *account, const Jid &occupant) const;
    void invalidate(bool notifyPending = false);

    QString       errorString() const;

signals:
    void finished();

private:
    void executeVCardTemp(PsiAccount *pa);
    void executePubSub(PsiAccount *pa);

    class Private;
    friend class QueuedLoader;
    std::unique_ptr<Private> d;
};

#endif // VCARDFACTORY_H
