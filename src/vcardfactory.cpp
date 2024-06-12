/*
 * vcardfactory.cpp - class for caching vCards
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

#include "vcardfactory.h"

#include "applicationinfo.h"
#include "avatars.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_tasks.h"
#include "iris/xmpp_vcard.h"
#include "jidutil.h"
#include "pepmanager.h"
#include "profiles.h"
#include "psiaccount.h"

#include "xmpp/xmpp-im/xmpp_caps.h"
#include "xmpp/xmpp-im/xmpp_pubsubitem.h"
#include "xmpp/xmpp-im/xmpp_serverinfomanager.h"
#include "xmpp/xmpp-im/xmpp_vcard4.h"

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QObject>
#include <QTextStream>
#include <ranges>

// #define VCF_DEBUG 1

#define PEP_VCARD4_NODE "urn:xmpp:vcard4"
#define PEP_VCARD4_NS "urn:ietf:params:xml:ns:vcard-4.0"

using VCardRequestQueue = QList<VCardRequest *>;

class VCardFactory::QueuedLoader : public QObject {
    Q_OBJECT

    static const int VcardReqInterval = 500; // query vcards

    VCardFactory              *q;
    VCardRequestQueue          queue_;
    QHash<Jid, VCardRequest *> jid2req;
    QTimer                     timer_;

signals:
    void vcardReceived(const VCardRequest *);

public:
    enum Priority { HighPriority, NormalPriority };

    QueuedLoader(VCardFactory *vcf);
    ~QueuedLoader();
    VCardRequest *enqueue(PsiAccount *acc, const Jid &jid, Flags flags, Priority prio);
};

/**
 * \brief Factory for retrieving and changing VCards.
 */
VCardFactory::VCardFactory() : QObject(qApp), dictSize_(5), queuedLoader_(new QueuedLoader(this))
{
    connect(queuedLoader_, &QueuedLoader::vcardReceived, this, [this](const VCardRequest *request) {
        if (request->success()) {
            saveVCard(request->jid(), request->vcard(), request->flags());
        }
#ifdef VCF_DEBUG
        else {
            qDebug() << "vcard query failed for " << request->jid().full() << ": " << request->errorString();
        }
#endif
    });
}

/**
 * \brief Destroys all cached VCards.
 */
VCardFactory::~VCardFactory() { }

/**
 * \brief Returns the VCardFactory instance.
 */
VCardFactory *VCardFactory::instance()
{
    if (!instance_) {
        instance_ = new VCardFactory();
    }
    return instance_;
}

/**
 * Adds a vcard to the cache (and removes other items if necessary)
 */
void VCardFactory::checkLimit(const QString &jid, const VCard4::VCard &vcard)
{
    if (vcardList_.contains(jid)) {
        vcardList_.removeAll(jid);
        vcardDict_.remove(jid);
    } else if (vcardList_.size() > dictSize_) {
        QString j = vcardList_.takeLast();
        vcardDict_.remove(j);
    }

    vcardDict_.insert(jid, vcard);
    vcardList_.push_front(jid);
}

void VCardFactory::saveVCard(const Jid &j, const VCard4::VCard &vcard, Flags flags)
{
#ifdef VCF_DEBUG
    qDebug() << "VCardFactory::saveVCard" << j.full();
#endif
    if (flags & MucUser) {

        auto &nick2vcard = mucVcardDict_[j.bare()];
        auto  nickIt     = nick2vcard.find(j.resource());
        if (nickIt == nick2vcard.end()) {
            nick2vcard.insert(j.resource(), vcard);
            auto &resQueue = lastMucVcards_[j.bare()];
            resQueue.enqueue(j.resource());
            while (resQueue.size() > 3) { // keep max 3 vcards per muc
                nick2vcard.remove(resQueue.dequeue());
            }
        } else {
            *nickIt = vcard;
        }

        emit vcardChanged(j, flags);
        return;
    }

    checkLimit(j.bare(), vcard);

    // save vCard to disk

    // ensure that there's a vcard directory to save into
    QDir p(pathToProfile(activeProfile, ApplicationInfo::CacheLocation));
    QDir v(pathToProfile(activeProfile, ApplicationInfo::CacheLocation) + "/vcard");
    if (!v.exists())
        p.mkdir("vcard");

    QFile file(ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml");
    if (vcard) {
        file.open(QIODevice::WriteOnly);
        QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif
        QDomDocument doc;
        doc.appendChild(vcard.toXmlElement(doc));
        out << doc.toString(4);
    } else if (file.exists()) {
        file.remove();
    }

    Jid  jid = j;
    emit vcardChanged(jid, flags);
}

/**
 * \brief Call this, when you need a runtime cached vCard.
 */
const VCard4::VCard VCardFactory::mucVcard(const Jid &j) const
{
    QHash<QString, VCard4::VCard>                d  = mucVcardDict_.value(j.bare());
    QHash<QString, VCard4::VCard>::ConstIterator it = d.constFind(j.resource());
    if (it != d.constEnd()) {
        return *it;
    }
    return {};
}

/**
 * \brief Call this, when you need a cached vCard.
 */
VCard4::VCard VCardFactory::vcard(const Jid &j, Flags flags)
{
    if (flags & MucUser) {
        return mucVcard(j);
    }

    // first, try to get vCard from runtime cache
    if (vcardDict_.contains(j.bare())) {
        return vcardDict_[j.bare()];
    }

    // then try to load from cache on disk
    QFile file(ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml");
    if (!file.open(QIODevice::ReadOnly)) {
        // REVIEW we can cache which files were really missed. or maybe set a flag for the contact
        return {};
    }
    QDomDocument doc;

    VCard4::VCard v4 = VCard4::VCard::fromDevice(&file);
    if (!v4) {
        file.seek(0);
        if (doc.setContent(&file, false)) {
            VCard vcard = VCard::fromXml(doc.documentElement());
            if (!vcard.isNull()) {
                v4.fromVCardTemp(vcard);
            }
        }
    }
    if (v4) {
        checkLimit(j.bare(), v4);
    }

    return {};
}

/**
 * \brief Call this when you need to update vCard in cache.
 */
// void VCardFactory::setVCard(const Jid &j, const VCard &v) { saveVCard(j, v); }

/**
 * \brief Updates vCard on specified \a account.
 */
Task *VCardFactory::setVCard(PsiAccount *account, const VCard4::VCard &v, const Jid &targetJid, Flags flags)
{
    if ((flags & MucRoom) || !account->client()->serverInfoManager()->hasPersistentStorage()) {
        JT_VCard *jtVCard_ = new JT_VCard(account->client()->rootTask());
        connect(jtVCard_, &JT_VCard::finished, this, [this, v, jtVCard_, flags]() {
            if (jtVCard_->success()) {
                saveVCard(jtVCard_->jid(), v, flags);
            }
        });
        jtVCard_->set(targetJid.isNull() ? account->jid() : targetJid, v.toVCardTemp(), !targetJid.isNull());
        jtVCard_->go(true);
        return jtVCard_;
    }
    QDomDocument *doc = account->client()->doc();
    QDomElement   el;
    QByteArray    avatarData = v.photo();
    if (avatarData.isEmpty() || !account->serverInfoManager()->accountFeatures().hasAvatarConversion()) {
        el = v.toXmlElement(*doc);
    } else {
        // let's better publish avatar via avatar service
        QImage image = QImage::fromData(avatarData);
        account->avatarFactory()->setSelfAvatar(image);
        VCard4::VCard v2 = v;
        v2.detach();
        VCard4::PAdvUris photos;
        for (auto const &uri :
             v2.photo() | std::views::filter([](auto const &item) { return item.data.data.isEmpty(); })) {
            photos.append(uri);
        }
        v2.setPhoto(photos);
        el = v2.toXmlElement(*doc);
    }
    return account->pepManager()->publish(QLatin1String(PEP_VCARD4_NODE), PubSubItem(QLatin1String("current"), el));
}

/**
 * \brief Call this when you need to retrieve fresh vCard from server (and store it in cache afterwards)
 */
VCardRequest *VCardFactory::getVCard(PsiAccount *account, const Jid &jid, Flags flags)
{
    return queuedLoader_->enqueue(account, jid, flags, QueuedLoader::HighPriority);
}

void VCardFactory::setPhoto(const Jid &j, const QByteArray &photo, Flags flags)
{
    VCard4::VCard vc;
    Jid           sj;
    if (flags & MucUser) {
        sj = j;
        vc = mucVcard(j);
    } else {
        sj = j.withResource({});
        vc = vcard(sj);
    }
    if (vc && vc.photo() != photo) {
        vc.setPhoto(VCard4::UriValue {});
        saveVCard(sj, vc, flags);
    }
}

void VCardFactory::deletePhoto(const Jid &j, Flags flags)
{
    VCard4::VCard vc;
    Jid           sj;
    if (flags & MucUser) {
        sj = j;
        vc = mucVcard(j);
    } else {
        sj = j.withResource({});
        vc = vcard(sj);
    }
    if (vc && !vc.photo().isEmpty()) {
        vc.setPhoto(VCard4::UriValue {});
        saveVCard(sj, vc, flags);
    }
}

void VCardFactory::ensureVCardPhotoUpdated(PsiAccount *acc, const Jid &jid, Flags flags, const QByteArray &newPhotoHash)
{
    VCard4::VCard vc;
    if (flags & MucUser) {
        vc = mucVcard(jid);
    } else {
        vc = vcard(jid);
    }
    if (newPhotoHash.isEmpty()) {
        if (!vc || vc.photo().isEmpty()) { // we didn't have it and still don't
            return;
        }
        vc.setPhoto(VCard4::UriValue {}); // reset photo;
        saveVCard(jid, vc, flags);

        return;
    }
    auto oldHash = vc ? QCryptographicHash::hash(QByteArray(vc.photo()), QCryptographicHash::Sha1) : QByteArray();
    if (oldHash != newPhotoHash) {
#ifdef VCF_DEBUG
        qDebug() << "hash mismatch. old=" << oldHash.toHex() << "new=" << photoHash.toHex();
#endif
        queuedLoader_->enqueue(acc, jid, flags, QueuedLoader::NormalPriority);
    }
}

VCardFactory *VCardFactory::instance_ = nullptr;

class VCardRequest::Private {
public:
    QList<QPointer<PsiAccount>> accounts;
    Jid                         jid;
    VCardFactory::Flags         flags;

    using ErrorPtr = std::unique_ptr<XMPP::Stanza::Error>;
    VCard4::VCard vcard;
    ErrorPtr      error;
    QString       statusString;

    Private(PsiAccount *pa, const Jid &jid, VCardFactory::Flags flags) :
        accounts({ { pa } }), jid(jid), flags(flags) { }
};

VCardRequest::VCardRequest(PsiAccount *account, const Jid &jid, VCardFactory::Flags flags) :
    d(new Private(account, jid, flags))
{
}

Jid &VCardRequest::jid() const { return d->jid; }

VCardFactory::Flags VCardRequest::flags() const { return d->flags; }

Task *VCardRequest::execute()
{
    auto paIt = std::ranges::find_if(d->accounts, [](auto pa) { return pa && pa->isConnected(); });
    if (paIt == d->accounts.end())
        return nullptr;

    bool doTemp = (d->flags & VCardFactory::MucRoom) || (d->flags & VCardFactory::MucUser);
    auto client = (*paIt)->client();
    auto cm     = client->capsManager();

    if (!doTemp) {
        if (d->jid.compare((*paIt)->jid(), false)) {
            doTemp = !client->serverInfoManager()->hasPersistentStorage();
        } else {
            doTemp = !cm->features(d->jid).hasVCard4();
        }
    }
    if (!doTemp) {
        auto task = (*paIt)->pepManager()->get(d->jid, PEP_VCARD4_NODE, QLatin1String("current"));
        task->connect(task, &PEPGetTask::finished, this, [this, task]() {
            if (task->success()) {
                if (!task->items().empty()) {
                    d->vcard = VCard4::VCard(task->items().last().payload());
                }
            } else if (!task->error().isCancel()
                       || task->error().condition != XMPP::Stanza::Error::ErrorCond::ItemNotFound) {
                // consider not found vcard as not an error. maybe user removed their vcard intentionally
                d->error.reset(new Stanza::Error(task->error()));
                d->statusString = task->statusString();
            }
            emit finished();
            deleteLater();
        });
        return task;
    } else {
        JT_VCard *task = new JT_VCard((*paIt)->client()->rootTask());
        task->connect(task, &JT_VCard::finished, this, [this, task]() {
            if (task->success()) {
                d->vcard.fromVCardTemp(task->vcard());
            } else if (!task->error().isCancel()
                       || task->error().condition != XMPP::Stanza::Error::ErrorCond::ItemNotFound) {
                // consider not found vcard as not an error. maybe user removed their vcard intentionally
                d->error.reset(new Stanza::Error(task->error()));
                d->statusString = task->statusString();
            }
            emit finished();
            deleteLater();
        });
        task->get(d->jid);
        task->go(true);
        return task;
    }
}

void VCardRequest::merge(PsiAccount *account, const Jid &, VCardFactory::Flags flags)
{
    d->flags |= flags;
    if (!std::any_of(d->accounts.begin(), d->accounts.end(), [account](auto const &p) { return p == account; })) {
        d->accounts << QPointer(account);
    }
}

bool VCardRequest::success() const { return d->error == nullptr; }

VCard4::VCard VCardRequest::vcard() const { return d->vcard; }

QString VCardRequest::errorString() const { return d->statusString.isEmpty() ? d->error->toString() : d->statusString; }

VCardRequest::~VCardRequest() = default;

VCardFactory::QueuedLoader::QueuedLoader(VCardFactory *vcf) : QObject(vcf), q(vcf)
{
    timer_.setSingleShot(false);
    timer_.setInterval(VcardReqInterval);
    QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
        if (queue_.isEmpty()) {
            timer_.stop();
            return;
        }
        auto request = queue_.takeFirst();
        connect(request, &VCardRequest::finished, this, [this, request]() {
#ifdef VCF_DEBUG
            qDebug() << "received VCardRequest" << request->jid().full();
#endif
            emit vcardReceived(request);
            jid2req.remove(request->jid());
            request->deleteLater();
        });
        auto task = request->execute();
        if (!task) {
            jid2req.remove(request->jid());
            request->deleteLater();
        }
        if (queue_.isEmpty()) {
            timer_.stop();
        }
    });
}

VCardFactory::QueuedLoader::~QueuedLoader() { qDeleteAll(jid2req); }

VCardRequest *VCardFactory::QueuedLoader::enqueue(PsiAccount *acc, const Jid &jid, Flags flags, Priority prio)
{
    auto sanitized_jid = jid;
    if (!(flags & MucUser)) {
        sanitized_jid = jid.withResource({});
    }
    auto req = jid2req[sanitized_jid];
    if (!req) {
#ifdef VCF_DEBUG
        qDebug() << "new VCardRequest" << sanitized_jid.full() << flags;
#endif
        req                    = new VCardRequest(acc, sanitized_jid, flags);
        jid2req[sanitized_jid] = req;
        queue_.append(req);
    } else {
#ifdef VCF_DEBUG
        qDebug() << "merge VCardRequest" << sanitized_jid.full() << flags;
#endif
        req->merge(acc, sanitized_jid, flags);
    }

    if (!timer_.isActive()) {
        timer_.start();
    }
    return req;
}

#include "vcardfactory.moc"
