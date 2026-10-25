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
#include "iris/xmpp_client.h"
#include "iris/xmpp_tasks.h"
#include "iris/xmpp_vcard.h"
#include "jidutil.h"
#include "pepmanager.h"
#include "profiles.h"
#include "psiaccount.h"

#include "iris/xmpp-im/xmpp_pubsubitem.h"
#include "iris/xmpp-im/xmpp_serverinfomanager.h"
#include "iris/xmpp-im/xmpp_vcard4.h"

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QObject>
#include <QTextStream>

// #define VCF_DEBUG 1

#define PEP_VCARD4_NODE "urn:xmpp:vcard4"
#define CONTACTS_NODE "urn:xmpp:contacts"
#define PEP_VCARD4_NS "urn:ietf:params:xml:ns:vcard-4.0"

using VCardRequestQueue = QList<VCardRequest *>;

class VCardFactory::QueuedLoader : public QObject {
    Q_OBJECT

    static const int VcardReqInterval = 500; // query vcards

    VCardRequestQueue          queue_;
    QHash<QString, VCardRequest *> jid2req;
    QHash<QString, VCardRequest *> photoReqs_; // independent of full-card requests
    QHash<VCardRequest *, QString> requestKeys_; // exact queued/in-flight identity
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
            const auto jid = request->jid();
            const auto flags = request->flags();
            if (flags & MucUser) {
                // Transient occupant cards are never written to disk.
                if (flags & PhotoOnly) {
                    auto cached = mucVcard(jid);
                    if (request->legacyVcard().photo().isEmpty()) {
                        if (clearPhoto(cached))
                            saveVCard(jid, cached, flags & ~PhotoOnly);
                    } else if (applyLegacyPhoto(cached, request->legacyVcard())) {
                        saveVCard(jid, cached, flags & ~PhotoOnly);
                    }
                } else {
                    saveVCard(jid, request->vcard(), flags);
                }
                return;
            }

            const QString accountId = request->accountId();
            if (flags & PhotoOnly) {
                if (!request->legacyAnswered())
                    return; // A failed IQ must never erase a cached source.

                const QString legacyPath = sourceCardFile(
                    ApplicationInfo::vCardDir(), accountId, jid, flags, false);
                const bool hadLegacySource = QFile::exists(legacyPath);
                auto legacy = readSourceCard(accountId, jid, flags, false);
                const auto newLegacy = request->legacyVcard();
                if (newLegacy.photo().isEmpty())
                    clearPhoto(legacy);
                else
                    applyLegacyPhoto(legacy, newLegacy);
                saveSourceCard(accountId, jid, flags, false, legacy);

                auto modern = readSourceCard(accountId, jid, flags, true);
                auto cached = vcard(jid);
                if (modern) {
                    // Provenance is known: deleting vcard-temp PHOTO cannot
                    // erase a separately published PEP vCard4 image.
                    cached = mergeSourceCards(modern, legacy);
                } else if (newLegacy.photo().isEmpty()) {
                    // Only remove an old embedded image when we know it came
                    // from the legacy source. An unknown old cache may instead
                    // contain a modern image and must not be destroyed.
                    if (hadLegacySource)
                        clearPhoto(cached);
                } else {
                    applyLegacyPhoto(cached, newLegacy);
                }
                saveVCard(jid, cached, flags & ~PhotoOnly);
            } else {
                // A transient failure of one source cannot erase the last
                // confirmed contents of that source; a successful empty
                // response, by contrast, removes its sidecar.
                if (request->modernAnswered())
                    saveSourceCard(accountId, jid, flags, true, request->modernVcard());
                if (request->legacyAnswered())
                    saveSourceCard(accountId, jid, flags, false, request->legacyVcard());

                const auto modern = readSourceCard(accountId, jid, flags, true);
                const auto legacy = readSourceCard(accountId, jid, flags, false);
                const auto merged = (modern || legacy)
                    ? mergeSourceCards(modern, legacy) : request->vcard();
                saveVCard(jid, merged, flags);
            }
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

void VCardFactory::saveSourceCard(const QString &accountId, const Jid &jid, Flags flags,
                                  bool modern, const VCard4::VCard &card)
{
    if (flags & MucUser)
        return; // Occupant cards live only while the room is active.
    const QString dir = ApplicationInfo::vCardDir();
    QDir().mkpath(dir);
    const QString fileName = sourceCardFile(dir, accountId, jid, flags, modern);
    if (card)
        card.save(fileName);
    else
        QFile::remove(fileName);
}

VCard4::VCard VCardFactory::readSourceCard(const QString &accountId, const Jid &jid,
                                            Flags flags, bool modern)
{
    if (flags & MucUser)
        return {};
    return readCachedVCard(sourceCardFile(ApplicationInfo::vCardDir(), accountId, jid, flags, modern));
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

        if (!(flags & Silent)) {
            emit vcardChanged(j, flags);
        }
        return;
    }

    checkLimit(j.bare(), vcard);

    // save vCard to disk

    // ensure that there's a vcard directory to save into
    QDir p(pathToProfile(activeProfile, ApplicationInfo::CacheLocation));
    QDir v(pathToProfile(activeProfile, ApplicationInfo::CacheLocation) + "/vcard");
    if (!v.exists())
        p.mkdir("vcard");

    auto filename = ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml";
    if (vcard) {
        vcard.save(filename);
    } else {
        QFile::remove(filename);
    }

    Jid jid = j;
    if (!(flags & Silent)) {
        emit vcardChanged(jid, flags);
    }
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

    // The first successful disk lookup must return the parsed card now,
    // not only after the object has been inserted into the RAM cache.
    const auto fileName = ApplicationInfo::vCardDir() + '/' + JIDUtil::encode(j.bare()).toLower() + ".xml";
    VCard4::VCard card = readCachedVCard(fileName);
    if (card)
        checkLimit(j.bare(), card);
    return card;
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
    if ((flags & MucRoom) || (flags & ForceVCardTemp)) {
        // || !account->client()->serverInfoManager()->hasPersistentStorage()) {
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
    QDomElement   el  = v.toXmlElement(*doc);

    return account->pepManager()->publish(QLatin1String(PEP_VCARD4_NODE), PubSubItem(QLatin1String("current"), el));
}

/**
 * \brief Call this when you need to retrieve fresh vCard from server (and store it in cache afterwards)
 */
VCardRequest *VCardFactory::getVCard(PsiAccount *account, const Jid &jid, Flags flags)
{
    return queuedLoader_->enqueue(account, jid, flags, QueuedLoader::HighPriority);
}

VCardRequest *VCardFactory::getVCardPhoto(PsiAccount *account, const Jid &jid, Flags flags)
{
    return queuedLoader_->enqueue(account, jid, flags | PhotoOnly, QueuedLoader::HighPriority);
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
    if (setEmbeddedPhoto(vc, photo)) {
        saveVCard(sj, vc, flags);
        // Silent only suppresses ordinary vCard notifications. AvatarCache
        // must still see our own avatar changes after the cache is updated.
        if (flags & Silent)
            emit photoChanged(sj, flags);
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
    if (clearPhoto(vc)) {
        saveVCard(sj, vc, flags);
        if (flags & Silent)
            emit photoChanged(sj, flags);
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
        if (clearPhoto(vc))
            saveVCard(jid, vc, flags);
        return;
    }
    auto oldHash = vc ? QCryptographicHash::hash(QByteArray(vc.photo()), QCryptographicHash::Sha1) : QByteArray();
    if (oldHash != newPhotoHash) {
#ifdef VCF_DEBUG
        qDebug() << "hash mismatch. old=" << oldHash.toHex() << "new=" << photoHash.toHex();
#endif
        if (!newPhotoHash.isEmpty()) {
            flags |= VCardFactory::ForceVCardTemp;
        }
        queuedLoader_->enqueue(acc, jid, flags | PhotoOnly, QueuedLoader::NormalPriority);
    }
}

VCardFactory *VCardFactory::instance_ = nullptr;

class VCardRequest::Private {
public:
    QList<QPointer<PsiAccount>> accounts;
    QString                     accountId;
    Jid                         jid;
    VCardFactory::Flags         flags;
    VCard4::VCard               modern;
    VCard4::VCard               legacy;
    bool                        modernAnswered = false;
    bool                        legacyAnswered = false;

    using ErrorPtr = std::unique_ptr<XMPP::Stanza::Error>;
    VCard4::VCard vcard;
    ErrorPtr      error;
    QString       statusString;
    bool          storeAnswered = false;

    Private(PsiAccount *pa, const Jid &jid, VCardFactory::Flags flags) :
        accounts({ { pa } }), accountId(pa ? pa->id() : QString()), jid(jid), flags(flags) { }
};

VCardRequest::VCardRequest(PsiAccount *account, const Jid &jid, VCardFactory::Flags flags) :
    d(new Private(account, jid, flags))
{
}

Jid &VCardRequest::jid() const { return d->jid; }

VCardFactory::Flags VCardRequest::flags() const { return d->flags; }

bool VCardRequest::execute()
{
    auto paIt = std::ranges::find_if(d->accounts, [](auto pa) { return pa && pa->isConnected(); });
    if (paIt == d->accounts.end())
        return false;

    // Client resource capabilities do not describe the account's server-side
    // vCard stores. Only explicit request modes determine which to query.
    if (VCardFactory::shouldFetchModern(d->flags))
        executePubSub(*paIt); // Continues with vcard-temp after PEP.
    else
        executeVCardTemp(*paIt);
    return true;
}

void VCardRequest::executeVCardTemp(PsiAccount *pa)
{
    JT_VCard *task = new JT_VCard(pa->client()->rootTask());
    task->connect(task, &JT_VCard::finished, this, [this, task]() {
        if (task->success()) {
            d->legacy.fromVCardTemp(task->vcard());
            d->legacyAnswered = true;
            VCardFactory::supplementFromLegacy(d->vcard, d->legacy);
            d->storeAnswered = true;
            d->error.reset();
            d->statusString.clear();
        } else if (task->error().condition == Stanza::Error::ErrorCond::ItemNotFound) {
            d->legacyAnswered = true;
            d->storeAnswered = true;
            d->error.reset();
            d->statusString.clear();
        } else if (!d->storeAnswered) {
            d->error.reset(new Stanza::Error(task->error()));
            d->statusString = task->statusString();
        }
        // Emit exactly once after the last store, so cache is updated once.
        emit finished();
        deleteLater();
    });
    task->get(d->jid);
    task->go(true);
}

void VCardRequest::executePubSub(PsiAccount *pa)
{
    auto task = pa->pepManager()->get(d->jid, QLatin1String(PEP_VCARD4_NODE), QLatin1String("current"));
    task->connect(task, &PEPGetTask::finished, this, [this, task, ppa = QPointer<PsiAccount>(pa)]() {
        if (task->success()) {
            d->modernAnswered = true;
            d->storeAnswered = true;
            if (!task->items().empty()) {
                d->modern = VCard4::VCard(task->items().last().payload());
                d->vcard = d->modern;
            }
        } else if (task->error().condition == Stanza::Error::ErrorCond::ItemNotFound) {
            d->modernAnswered = true;
            d->storeAnswered = true;
        } else {
            d->error.reset(new Stanza::Error(task->error()));
            d->statusString = task->statusString();
        }
        if (ppa && ppa->isConnected()) {
            executeVCardTemp(ppa);
            return;
        }
        // Disconnected between the two requests; terminate the queue entry.
        emit finished();
        deleteLater();
    });
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

const QString &VCardRequest::accountId() const { return d->accountId; }
VCard4::VCard VCardRequest::modernVcard() const { return d->modern; }
VCard4::VCard VCardRequest::legacyVcard() const { return d->legacy; }
bool VCardRequest::modernAnswered() const { return d->modernAnswered; }
bool VCardRequest::legacyAnswered() const { return d->legacyAnswered; }

QString VCardRequest::errorString() const { return d->statusString.isEmpty() ? d->error->toString() : d->statusString; }

VCardRequest::~VCardRequest() = default;

VCardFactory::QueuedLoader::QueuedLoader(VCardFactory *vcf) : QObject(vcf)
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
            const QString key = requestKeys_.take(request);
            (request->flags() & PhotoOnly ? photoReqs_ : jid2req).remove(key);
            request->deleteLater();
        });
        auto started = request->execute();
        if (!started) {
            const QString key = requestKeys_.take(request);
            (request->flags() & PhotoOnly ? photoReqs_ : jid2req).remove(key);
            request->deleteLater();
        }
        if (queue_.isEmpty()) {
            timer_.stop();
        }
    });
}

VCardFactory::QueuedLoader::~QueuedLoader()
{
    qDeleteAll(jid2req);
    qDeleteAll(photoReqs_);
}

VCardRequest *VCardFactory::QueuedLoader::enqueue(PsiAccount *acc, const Jid &jid, Flags flags, Priority prio)
{
    auto sanitized_jid = jid;
    if (!(flags & MucUser)) {
        sanitized_jid = jid.withResource({});
    }
    const QString key = VCardFactory::requestQueueKey(
        acc->id(), reinterpret_cast<quintptr>(acc), sanitized_jid, flags);
    auto &requests = flags & PhotoOnly ? photoReqs_ : jid2req;
    auto req = requests.value(key);
    if (!req) {
#ifdef VCF_DEBUG
        qDebug() << "new VCardRequest" << sanitized_jid.full() << flags;
#endif
        req                    = new VCardRequest(acc, sanitized_jid, flags);
        requests.insert(key, req);
        requestKeys_.insert(req, key);
        if (prio == HighPriority) {
            queue_.prepend(req);
        } else {
            queue_.append(req);
        }
    } else {
#ifdef VCF_DEBUG
        qDebug() << "merge VCardRequest" << sanitized_jid.full() << flags;
#endif
        req->merge(acc, sanitized_jid, flags);
        // A previously queued low-priority photo refresh should not delay
        // an explicit high-priority request with the same identity.
        if (prio == HighPriority) {
            const int index = queue_.indexOf(req);
            if (index > 0) {
                queue_.removeAt(index);
                queue_.prepend(req);
            }
        }
    }

    if (!timer_.isActive()) {
        timer_.start();
    }
    return req;
}

#include "vcardfactory.moc"
