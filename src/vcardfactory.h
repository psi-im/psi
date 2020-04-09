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

class VCardFactory : public QObject {
    Q_OBJECT

public:
    static VCardFactory *instance();
    VCard                vcard(const Jid &);
    const VCard          mucVcard(const Jid &j) const;
    void                 setVCard(const Jid &, const VCard &, bool notifyPhoto = true);
    void setVCard(const PsiAccount *account, const VCard &v, QObject *obj = nullptr, const char *slot = nullptr);
    void setTargetVCard(const PsiAccount *account, const VCard &v, const Jid &mucJid, QObject *obj, const char *slot);
    JT_VCard *getVCard(const Jid &, Task *rootTask, const QObject *, std::function<void()> &&cb, bool cacheVCard = true,
                       bool isMuc = false, bool notifyPhoto = true);

signals:
    void vcardChanged(const Jid &);
    void vcardPhotoAvailable(
        const Jid &,
        bool isMuc); // dedicated for AvatarFactory. it will almost always work except requests from AvatarFactory

protected:
    void checkLimit(const QString &jid, const VCard &vcard);

private slots:
    void updateVCardFinished();
    void taskFinished();
    void mucTaskFinished();

private:
    VCardFactory();
    ~VCardFactory();

    static VCardFactory *                instance_;
    const int                            dictSize_;
    QStringList                          vcardList_;
    QMap<QString, VCard>                 vcardDict_;
    QMap<QString, QHash<QString, VCard>> mucVcardDict_; // QHash in case of big mucs mucBareJid => {resoure => vcard}
    QMap<QString, QQueue<QString>>
        lastMucVcards_; // to limit the hash above. this one keeps ordered resource. mucBareJid => resource_list

    void saveVCard(const Jid &, const VCard &, bool notifyPhoto);
};

#endif // VCARDFACTORY_H
