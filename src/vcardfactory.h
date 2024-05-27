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

#include <QHash>
#include <QMap>
#include <QObject>
#include <QStringList>

#include <functional>

class PsiAccount;

namespace XMPP {
class JT_VCard;
class Jid;
class Task;
class VCard;
}
using namespace XMPP;

class VCardRequest;

class VCardFactory : public QObject {
    Q_OBJECT

public:
    enum Flag {
        MucRoom       = 0x1,
        MucUser       = 0x2,
        Cache         = 0x4,
        InterestInfo  = 0x8,  // text data
        InterestPhoto = 0x10, // graphics. photoHash MUST be provided
        InterestAll   = InterestInfo | InterestPhoto
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    static VCardFactory *instance();
    VCard                vcard(const Jid &);
    const VCard          mucVcard(const Jid &j) const;
    // void                 setVCard(const Jid &, const VCard &);

    JT_VCard     *setVCard(const PsiAccount *account, const VCard &v, const Jid &targetJid, VCardFactory::Flags flags);
    VCardRequest *getVCard(PsiAccount *account, const Jid &, VCardFactory::Flags flags = InterestInfo);

    // 1. check if it's needed to do a request,
    // 2. enqueue request if necessary (no vcard, or if InterestPhoto then also if hash doesn't match)
    // 3. vcardChanged() will be sent as usually when vcard is updated
    void ensureVCardUpdated(PsiAccount *acc, const Jid &jid, Flags flags = InterestInfo,
                            const QByteArray &photoHash = {});

signals:
    void vcardChanged(const Jid &, VCardFactory::Flags);

protected:
    void checkLimit(const QString &jid, const VCard &vcard);

private:
    VCardFactory();
    ~VCardFactory();
    friend class VCardRequest;
    void saveVCard(const Jid &, const VCard &, VCardFactory::Flags flags);

    static VCardFactory                 *instance_;
    const int                            dictSize_;
    QStringList                          vcardList_;
    QMap<QString, VCard>                 vcardDict_;
    QMap<QString, QHash<QString, VCard>> mucVcardDict_; // QHash in case of big mucs mucBareJid => {resoure => vcard}

    // to limit the hash above. this one keeps ordered resource. mucBareJid => resource_list
    QMap<QString, QQueue<QString>> lastMucVcards_;

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

    JT_VCard *execute();
    void      merge(PsiAccount *account, const Jid &, VCardFactory::Flags flags);

    // result stuff
    bool    success() const; // item-not-found is considered success but vcard will be null
    VCard   vcard() const;
    QString errorString() const;

signals:
    void finished();

private:
    class Private;
    friend class QueuedLoader;
    std::unique_ptr<Private> d;
};

#endif // VCARDFACTORY_H
