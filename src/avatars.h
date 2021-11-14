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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef AVATARS_H
#define AVATARS_H

#include <QByteArray>
#include <QMap>
#include <QPixmap>
#include <QQueue>
#include <QString>
#include <QTimer>

#define PEP_AVATAR_DATA_TN "data"
#define PEP_AVATAR_DATA_NS "urn:xmpp:avatar:data"
#define PEP_AVATAR_METADATA_TN "metadata"
#define PEP_AVATAR_METADATA_NS "urn:xmpp:avatar:metadata"

class Avatar;
class FileAvatar;
class PEPAvatar;
class PsiAccount;
class VCardAvatar;
class VCardMucAvatar;
class VCardStaticAvatar;

namespace XMPP {
class Jid;
class Resource;
class PubSubItem;
class Status;
}
using namespace XMPP;

//------------------------------------------------------------------------------

class AvatarFactory : public QObject {
    Q_OBJECT

    static const int VcardReqInterval = 500; // query vCard avatars once per half second

public:
    struct UserHashes {
        QByteArray avatar; // current active avatar
        QByteArray vcard;  // avatar hash just in case
    };

    struct AvatarData {
        QByteArray data;
        QString    metaType;
    };

    AvatarFactory(PsiAccount *pa);
    ~AvatarFactory();

    QPixmap getAvatar(const Jid &jid);
    // QPixmap getAvatarByHash(const QString& hash);
    static AvatarData avatarDataByHash(const QByteArray &hash);
    UserHashes        userHashes(const Jid &jid) const;
    PsiAccount *      account() const;
    void              setSelfAvatar(const QString &fileName);

    void importManualAvatar(const Jid &j, const QString &fileName);
    void removeManualAvatar(const Jid &j);
    bool hasManualAvatar(const Jid &j);

    void    newMucItem(const Jid &fullJid, const Status &s);
    QPixmap getMucAvatar(const Jid &jid);

    static QString getCacheDir();
    static int     maxAvatarSize();
    static QPixmap roundedAvatar(const QPixmap &pix, int rad, int avatarSize);

    void statusUpdate(const Jid &jid, const XMPP::Status &status);
signals:
    void avatarChanged(const XMPP::Jid &);

protected slots:
    void itemPublished(const XMPP::Jid &, const QString &, const PubSubItem &);
    void publish_success(const QString &, const XMPP::PubSubItem &);
    void resourceAvailable(const XMPP::Jid &, const XMPP::Resource &);

private slots:
    void vcardUpdated(const XMPP::Jid &, bool isMuc);

private:
    class Private;
    Private *d;
};

#endif // AVATARS_H
