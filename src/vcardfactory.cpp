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
#include "profiles.h"
#include "psiaccount.h"

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QObject>
#include <QTextStream>

// #define VCF_DEBUG 1

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
            if (request->flags() & MucUser) {
                Jid   j          = request->jid();
                auto &nick2vcard = mucVcardDict_[j.bare()];
                auto  nickIt     = nick2vcard.find(j.resource());
                if (nickIt == nick2vcard.end()) {
                    nick2vcard.insert(j.resource(), request->vcard());
                    auto &resQueue = lastMucVcards_[j.bare()];
                    resQueue.enqueue(j.resource());
                    while (resQueue.size() > 3) { // keep max 3 vcards per muc
                        nick2vcard.remove(resQueue.dequeue());
                    }
                } else {
                    *nickIt = request->vcard();
                }

                emit vcardChanged(j, request->flags());
            } else {
                saveVCard(request->jid(), request->vcard(), request->flags());
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
void VCardFactory::checkLimit(const QString &jid, const VCard &vcard)
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

void VCardFactory::saveVCard(const Jid &j, const VCard &vcard, Flags flags)
{
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
        doc.appendChild(vcard.toXml(&doc));
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
const VCard VCardFactory::mucVcard(const Jid &j) const
{
    QHash<QString, VCard>                d  = mucVcardDict_.value(j.bare());
    QHash<QString, VCard>::ConstIterator it = d.constFind(j.resource());
    if (it != d.constEnd()) {
        return *it;
    }
    return VCard();
}

/**
 * \brief Call this, when you need a cached vCard.
 */
VCard VCardFactory::vcard(const Jid &j)
{
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

    if (doc.setContent(&file, false)) {
        VCard vcard = VCard::fromXml(doc.documentElement());
        if (!vcard.isNull()) {
            checkLimit(j.bare(), vcard);
            return vcard;
        }
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
JT_VCard *VCardFactory::setVCard(const PsiAccount *account, const VCard &v, const Jid &targetJid, Flags flags)
{
    JT_VCard *jtVCard_ = new JT_VCard(account->client()->rootTask());
    connect(jtVCard_, &JT_VCard::finished, this, [this, jtVCard_, flags]() {
        if (jtVCard_->success()) {
            saveVCard(jtVCard_->jid(), jtVCard_->vcard(), flags);
        }
    });
    jtVCard_->set(targetJid.isNull() ? account->jid() : targetJid, v, !targetJid.isNull());
    jtVCard_->go(true);
    return jtVCard_;
}

/**
 * \brief Call this when you need to retrieve fresh vCard from server (and store it in cache afterwards)
 */
VCardRequest *VCardFactory::getVCard(PsiAccount *account, const Jid &jid, Flags flags)
{
    return queuedLoader_->enqueue(account, jid, flags, QueuedLoader::HighPriority);
}

void VCardFactory::ensureVCardUpdated(PsiAccount *acc, const Jid &jid, Flags flags, const QByteArray &photoHash)
{
    VCard vc;
    if (flags & MucUser) {
        vc = mucVcard(jid);
    } else {
        vc = vcard(jid);
    }
    if (!vc || (flags & InterestPhoto && QCryptographicHash::hash(vc.photo(), QCryptographicHash::Sha1) != photoHash)) {
        // FIXME computing hash everytime is not quite cool. We need to store it in metadata like in FileCache
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
    VCard    vcard;
    ErrorPtr error;
    QString  statusString;

    Private(PsiAccount *pa, const Jid &jid, VCardFactory::Flags flags) :
        accounts({ { pa } }), jid(jid), flags(flags) { }
};

VCardRequest::VCardRequest(PsiAccount *account, const Jid &jid, VCardFactory::Flags flags) :
    d(new Private(account, jid, flags))
{
}

Jid &VCardRequest::jid() const { return d->jid; }

VCardFactory::Flags VCardRequest::flags() const { return d->flags; }

JT_VCard *VCardRequest::execute()
{
    auto paIt = std::ranges::find_if(d->accounts, [](auto pa) { return pa && pa->isConnected(); });
    if (paIt == d->accounts.end())
        return nullptr;

    JT_VCard *task = new JT_VCard((*paIt)->client()->rootTask());
    task->connect(task, &JT_VCard::finished, this, [this, task]() {
        if (task->success()) {
            d->vcard = task->vcard();
        } else if (!task->error().isCancel()
                   || task->error().condition != XMPP::Stanza::Error::ErrorCond::ItemNotFound) {
            // consider not founf vcard as not an error. maybe user removed their vcard intentionally
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

void VCardRequest::merge(PsiAccount *account, const Jid &, VCardFactory::Flags flags)
{
    d->flags |= flags;
    if (!std::any_of(d->accounts.begin(), d->accounts.end(), [account](auto const &p) { return p == account; })) {
        d->accounts << QPointer(account);
    }
}

bool VCardRequest::success() const { return d->error == nullptr; }

VCard VCardRequest::vcard() const { return d->vcard; }

QString VCardRequest::errorString() const { return d->statusString.isEmpty() ? d->error->toString() : d->statusString; }

VCardRequest::~VCardRequest() = default;

VCardFactory::QueuedLoader::QueuedLoader(VCardFactory *vcf) : QObject(vcf), q(vcf)
{
    timer_.setSingleShot(false);
    timer_.setInterval(VcardReqInterval);
    QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
        auto request = queue_.takeFirst();
        auto task    = request->execute();
        if (task) {
            connect(task, &JT_VCard::finished, this, [this, request]() {
                emit vcardReceived(request);
                jid2req.remove(request->jid());
                request->deleteLater();
            });
        } else {
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
        req = new VCardRequest(acc, sanitized_jid, flags);
        queue_.append(req);
    } else {
        req->merge(acc, sanitized_jid, flags);
    }

    if (!timer_.isActive()) {
        timer_.start();
    }
    return req;
}

#include "vcardfactory.moc"
