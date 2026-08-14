/*
 * psiencryptioncontroller.cpp - application-level encryption registry for Psi
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "psiencryptioncontroller.h"

#include "iris/xmpp_caps.h"
#include "iris/xmpp_client.h"
#include "iconset.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "userlist.h"

#ifdef IRIS_ENABLE_OMEMO
#include "iris/xmpp_omemo.h"
#include "psiomemostorage.h"
#endif

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace {
constexpr auto OpenPgpMethodId = "openpgp";

XMPP::EncryptionJob::Error toIrisError(EncryptionMethodProvider::Error error)
{
    using PluginError = EncryptionMethodProvider::Error;
    using IrisError   = XMPP::EncryptionJob::Error;
    switch (error) {
    case PluginError::None:
        return IrisError::None;
    case PluginError::Unsupported:
        return IrisError::Unsupported;
    case PluginError::InvalidInput:
        return IrisError::InvalidInput;
    case PluginError::NoRecipients:
        return IrisError::NoRecipients;
    case PluginError::NoSession:
        return IrisError::NoSession;
    case PluginError::UntrustedIdentity:
        return IrisError::UntrustedIdentity;
    case PluginError::CryptoError:
        return IrisError::CryptoError;
    case PluginError::AuthenticationFailed:
        return IrisError::AuthenticationFailed;
    case PluginError::ProtocolError:
        return IrisError::ProtocolError;
    case PluginError::StorageError:
        return IrisError::StorageError;
    case PluginError::NetworkError:
        return IrisError::NetworkError;
    case PluginError::Cancelled:
        return IrisError::Cancelled;
    }
    return IrisError::ProtocolError;
}

EncryptionMethodProvider::Metadata toPluginMetadata(const XMPP::EncryptionMetadata &metadata)
{
    EncryptionMethodProvider::Metadata result;
    result.sender         = metadata.sender.full();
    result.senderDeviceId = metadata.senderDeviceId;
    result.senderKey      = metadata.senderKey;
    result.protocolOnly   = metadata.protocolOnly;
    result.details        = metadata.details;
    return result;
}

XMPP::EncryptionMetadata toIrisMetadata(const QString &methodId, const EncryptionMethodProvider::Metadata &metadata)
{
    XMPP::EncryptionMetadata result;
    result.methodId       = methodId;
    result.sender         = XMPP::Jid(metadata.sender);
    result.senderDeviceId = metadata.senderDeviceId;
    result.senderKey      = metadata.senderKey;
    result.protocolOnly   = metadata.protocolOnly;
    result.details        = metadata.details;
    return result;
}

EncryptionMethodProvider::Context toPluginContext(const XMPP::EncryptionContext &context)
{
    EncryptionMethodProvider::Context result;
    result.options = context.options;
    result.recipients.reserve(context.recipients.size());
    for (const auto &recipient : context.recipients)
        result.recipients.append(recipient.full());
    if (context.replyTo) {
        result.hasReplyTo = true;
        result.replyTo    = toPluginMetadata(*context.replyTo);
    }
    return result;
}

XMPP::EncryptionMethod::Capabilities toIrisCapabilities(EncryptionMethodProvider::Capabilities capabilities)
{
    XMPP::EncryptionMethod::Capabilities result;
    if (capabilities.testFlag(EncryptionMethodProvider::XmppStanza))
        result |= XMPP::EncryptionMethod::XmppStanza;
    if (capabilities.testFlag(EncryptionMethodProvider::DataMessage))
        result |= XMPP::EncryptionMethod::DataMessage;
    return result;
}

#ifdef IRIS_ENABLE_OMEMO
bool needsManualTrustDecision(XMPP::EncryptionTrustLevel level)
{
    // AutomaticallyTrusted may exist in a profile that was opened once by an
    // earlier development build. Treat it as pending when manual verification
    // is enabled so those identities do not silently bypass the new policy.
    return level == XMPP::EncryptionTrustLevel::Undecided || level == XMPP::EncryptionTrustLevel::AutomaticallyTrusted;
}

QString formatFingerprint(const QByteArray &fingerprint)
{
    const auto hex = fingerprint.toHex().toUpper();
    QString    out;
    out.reserve(hex.size() + hex.size() / 8);
    for (qsizetype i = 0; i < hex.size(); i += 8) {
        if (!out.isEmpty())
            out += QLatin1Char(' ');
        out += QString::fromLatin1(hex.mid(i, 8));
    }
    return out;
}
#endif
} // namespace

class PsiEncryptionController::PluginMethodAdapter final : public XMPP::EncryptionMethod {
public:
    class Session final : public XMPP::EncryptedSession {
    public:
        Session(PluginMethodAdapter *method, const XMPP::EncryptionContext &context,
                EncryptionMethodProvider::Session *pluginSession, QObject *parent = nullptr) :
            XMPP::EncryptedSession(method ? method->id() : QString(), context, parent), method_(method),
            pluginSession_(pluginSession)
        {
            if (method)
                method->sessions_.insert(this);
        }

        ~Session() override
        {
            detachProvider();
            if (method_)
                method_->sessions_.remove(this);
        }

        XMPP::EncryptionJob *encrypt(const QDomElement &xml) override
        {
            return runStanza(true, xml);
        }

        XMPP::EncryptionJob *decrypt(const QDomElement &xml) override
        {
            return runStanza(false, xml);
        }

        XMPP::EncryptionJob *encrypt(const QByteArray &data) override
        {
            return runData(true, data);
        }

        XMPP::EncryptionJob *decrypt(const QByteArray &data) override
        {
            return runData(false, data);
        }

    private:
        void detachProvider()
        {
            const auto operations = activeOperations_;
            activeOperations_.clear();
            for (auto it = operations.cbegin(); it != operations.cend(); ++it) {
                delete it.value(); // asks the plugin to cancel queued/asynchronous work
                if (it.key() && !it.key()->isFinished())
                    it.key()->fail(XMPP::EncryptionJob::Error::Cancelled,
                                   QStringLiteral("Encryption plugin was unloaded"));
            }
            pluginSession_.reset();
        }

        XMPP::EncryptionJob *runStanza(bool encrypting, const QDomElement &xml)
        {
            auto *job = new XMPP::EncryptionJob(this);
            if (!method_ || !pluginSession_) {
                job->fail(XMPP::EncryptionJob::Error::Cancelled,
                          QStringLiteral("Encryption plugin is no longer available"));
                return job;
            }

            auto                         *guard = new QObject(job);
            QPointer<XMPP::EncryptionJob> weakJob(job);
            activeOperations_.insert(job, guard);
            connect(job, &XMPP::EncryptionJob::finished, this, [this, job]() { activeOperations_.remove(job); });
            connect(job, &QObject::destroyed, this, [this, job]() { activeOperations_.remove(job); });
            auto completion = [weakJob, methodId = methodId()](EncryptionMethodProvider::Result result) {
                if (!weakJob || weakJob->isFinished())
                    return;
                if (!result.success) {
                    weakJob->fail(toIrisError(result.error), result.errorString);
                    return;
                }
                weakJob->complete(result.stanza, toIrisMetadata(methodId, result.metadata));
            };

            if (encrypting)
                pluginSession_->encryptStanza(xml, guard, std::move(completion));
            else
                pluginSession_->decryptStanza(xml, guard, std::move(completion));
            return job;
        }

        XMPP::EncryptionJob *runData(bool encrypting, const QByteArray &data)
        {
            auto *job = new XMPP::EncryptionJob(this);
            if (!method_ || !pluginSession_) {
                job->fail(XMPP::EncryptionJob::Error::Cancelled,
                          QStringLiteral("Encryption plugin is no longer available"));
                return job;
            }

            auto                         *guard = new QObject(job);
            QPointer<XMPP::EncryptionJob> weakJob(job);
            activeOperations_.insert(job, guard);
            connect(job, &XMPP::EncryptionJob::finished, this, [this, job]() { activeOperations_.remove(job); });
            connect(job, &QObject::destroyed, this, [this, job]() { activeOperations_.remove(job); });
            auto completion = [weakJob, methodId = methodId()](EncryptionMethodProvider::Result result) {
                if (!weakJob || weakJob->isFinished())
                    return;
                if (!result.success) {
                    weakJob->fail(toIrisError(result.error), result.errorString);
                    return;
                }
                weakJob->complete(result.data, toIrisMetadata(methodId, result.metadata));
            };

            if (encrypting)
                pluginSession_->encryptData(data, guard, std::move(completion));
            else
                pluginSession_->decryptData(data, guard, std::move(completion));
            return job;
        }

        QPointer<PluginMethodAdapter>                    method_;
        std::unique_ptr<EncryptionMethodProvider::Session> pluginSession_;
        QHash<XMPP::EncryptionJob *, QObject *>          activeOperations_;

        friend class PluginMethodAdapter;
    };

    PluginMethodAdapter(int accountId, EncryptionMethodProvider *provider, QObject *parent = nullptr) :
        XMPP::EncryptionMethod(parent), accountId_(accountId), provider_(provider),
        id_(provider ? provider->id() : QString())
    {
    }

    QString      id() const override { return id_; }
    QString      name() const override { return provider_ ? provider_->name() : id_; }
    Capabilities capabilities() const override
    {
        return provider_ ? toIrisCapabilities(provider_->capabilities()) : Capabilities {};
    }
    XMPP::EncryptedSession *startSession(Capabilities capabilities,
                                         const XMPP::EncryptionContext &context) override
    {
        if (!provider_ || !(capabilities & this->capabilities()))
            return nullptr;
        auto *pluginSession = provider_->startSession(accountId_, toPluginContext(context));
        if (!pluginSession)
            return nullptr;
        return new Session(this, context, pluginSession);
    }
    XMPP::Features features() const override
    {
        return provider_ ? XMPP::Features(provider_->features()) : XMPP::Features();
    }
    bool canDecrypt(const QDomElement &stanza) const override
    {
        return provider_ && provider_->canDecrypt(accountId_, stanza);
    }

    void detachProvider()
    {
        const auto sessions = sessions_;
        sessions_.clear();
        for (auto *session : sessions) {
            if (session) {
                session->detachProvider();
                session->method_ = nullptr;
            }
        }
        provider_ = nullptr;
    }

    int                       accountId_ = -1;
    EncryptionMethodProvider *provider_  = nullptr;
    QString                   id_;
    QSet<Session *>           sessions_;
};

struct PsiEncryptionController::PluginRegistration {
    int                       accountId = -1;
    EncryptionMethodProvider *provider  = nullptr;
    PluginMethodAdapter      *adapter   = nullptr;
    QMetaObject::Connection   lifetimeConnection;
};

PsiEncryptionController::PsiEncryptionController(PsiAccount *account, XMPP::Client *client,
                                                 const QMap<QString, QString> &selectedMethods,
                                                 const QString &profileDataPath, const QString &accountId,
                                                 QObject *parent) :
    QObject(parent), account_(account), client_(client), selectedMethods_(selectedMethods)
{
    Q_ASSERT(account_);
    Q_ASSERT(client_);

    connect(client_->encryptionManager(), &XMPP::EncryptionManager::methodRegistered, this,
            [this](const QString &) { emit methodsChanged(); });
    connect(client_->encryptionManager(), &XMPP::EncryptionManager::methodUnregistered, this,
            [this](const QString &) { emit methodsChanged(); });
    connect(client_->capsManager(), &XMPP::CapsManager::capsChanged, this,
            [this](const XMPP::Jid &jid) { emit peerAvailabilityChanged(jid); });
    connect(client_, &XMPP::Client::resourceAvailable, this,
            [this](const XMPP::Jid &jid, const XMPP::Resource &) { emit peerAvailabilityChanged(jid); });
    connect(client_, &XMPP::Client::resourceUnavailable, this,
            [this](const XMPP::Jid &jid, const XMPP::Resource &) { emit peerAvailabilityChanged(jid); });
    connect(client_, &XMPP::Client::stanzaDecryptionFailed, this,
            [](const QString &methodId, const QString &error) {
                qWarning().noquote() << QStringLiteral("%1 stanza decryption failed: %2").arg(methodId, error);
            });
    connect(account_, &PsiAccount::pgpKeyChanged, this, [this]() {
        emit methodStateChanged(QString::fromLatin1(OpenPgpMethodId));
        emit methodsChanged();
    });

#ifdef IRIS_ENABLE_OMEMO
    omemoStorage_ = std::make_unique<PsiOmemoStorage>(profileDataPath, accountId);
    if (omemoStorage_->isOpen()) {
        legacyOmemoEnabled_  = omemoStorage_->legacyEnabledJids();
        legacyOmemoDisabled_ = omemoStorage_->legacyDisabledJids();
        omemo_               = new XMPP::OmemoEncryption(client_, omemoStorage_.get(), omemoStorage_.get(), this);
        // Psi uses explicit device verification. Unknown identities are learned
        // so their fingerprints can be shown, but they cannot be used for a
        // newly built outgoing session until the user makes a decision.
        omemo_->setNewIdentityTrustLevel(XMPP::EncryptionTrustLevel::Undecided);
        omemo_->setAcceptedSessionBuildingTrustLevels(XMPP::EncryptionTrustLevel::ManuallyTrusted
                                                      | XMPP::EncryptionTrustLevel::Authenticated);

        // Preserve historical trust decisions from the old OMEMO plugin.
        for (const auto &device : omemo_->devices()) {
            if (device.identityKey.isEmpty())
                continue;
            const auto legacyTrust = omemoStorage_->legacyTrust(device.owner.bare(), device.id);
            if (!legacyTrust || device.trust != XMPP::EncryptionTrustLevel::Undecided)
                continue;
            switch (*legacyTrust) {
            case PsiOmemoStorage::LegacyTrust::Trusted:
                omemo_->setTrustLevel(device.owner, device.identityKey, XMPP::EncryptionTrustLevel::ManuallyTrusted);
                break;
            case PsiOmemoStorage::LegacyTrust::Untrusted:
                omemo_->setTrustLevel(device.owner, device.identityKey, XMPP::EncryptionTrustLevel::Distrusted);
                break;
            case PsiOmemoStorage::LegacyTrust::Undecided:
                break;
            }
        }

        connect(omemo_, &XMPP::OmemoEncryption::trustChanged, this,
                [this](const XMPP::Jid &owner, const QByteArray &identityKey, XMPP::EncryptionTrustLevel level) {
                    if (!omemoStorage_ || !omemo_)
                        return;
                    for (const auto &device : omemo_->devices(owner)) {
                        if (device.identityKey != identityKey
                            || !device.protocols.testFlag(XMPP::OmemoProtocol::Legacy))
                            continue;
                        const auto legacy = level == XMPP::EncryptionTrustLevel::Distrusted
                            ? PsiOmemoStorage::LegacyTrust::Untrusted
                            : (level == XMPP::EncryptionTrustLevel::Undecided ? PsiOmemoStorage::LegacyTrust::Undecided
                                                                              : PsiOmemoStorage::LegacyTrust::Trusted);
                        omemoStorage_->setLegacyTrust(owner.bare(), device.id, legacy);
                    }
                    emit methodStateChanged(XMPP::OmemoEncryption::methodId());
                });
        connect(omemo_, &XMPP::OmemoEncryption::deviceChanged, this,
                [this](const XMPP::Jid &, uint32_t) { emit methodStateChanged(XMPP::OmemoEncryption::methodId()); });
        connect(omemo_, &XMPP::OmemoEncryption::deviceRemoved, this,
                [this](const XMPP::Jid &, uint32_t) { emit methodStateChanged(XMPP::OmemoEncryption::methodId()); });
        connect(omemo_, &XMPP::OmemoEncryption::warning, this,
                [this](const QString &message) { emit encryptionError({}, message); });
    } else {
        qWarning() << "Could not open OMEMO storage:" << omemoStorage_->errorString();
    }
#else
    Q_UNUSED(profileDataPath);
    Q_UNUSED(accountId);
#endif
}

PsiEncryptionController::~PsiEncryptionController()
{
    const auto providers = pluginMethods_.keys();
    for (auto *provider : providers)
        unregisterPluginMethod(provider);

#ifdef IRIS_ENABLE_OMEMO
    // OmemoEncryption is a QObject child while its storage is a C++ member.
    // Destroy it explicitly before member destruction so it cannot observe an
    // already-destroyed storage backend from QObject's base destructor.
    delete omemo_;
    omemo_ = nullptr;
#endif
}

XMPP::Client *PsiEncryptionController::client() const { return client_; }

QList<PsiEncryptionController::MethodInfo>
PsiEncryptionController::methods(XMPP::EncryptionMethod::Capabilities capabilities) const
{
    QList<MethodInfo> result;

    if ((capabilities == XMPP::EncryptionMethod::Capabilities {}
         || capabilities.testFlag(XMPP::EncryptionMethod::XmppStanza))
        && account_->hasPgp() && PsiOptions::instance()->getOption("plugins.auto-load.openpgp", false).toBool()) {
        MethodInfo pgp;
        pgp.id             = QString::fromLatin1(OpenPgpMethodId);
        pgp.name           = QStringLiteral("OpenPGP");
        pgp.icon           = IconsetFactory::icon("psi/cryptoYes").icon();
        pgp.capabilities   = XMPP::EncryptionMethod::XmppStanza;
        pgp.uiCapabilities = EncryptionMethodProvider::KeyManagement;
        result.append(pgp);
    }

    const auto nativeMethods = client_->encryptionManager()->methods(capabilities);
    for (const auto &[id, name] : nativeMethods) {
        MethodInfo info;
        info.id           = id;
        info.name         = name;
        info.capabilities = client_->encryptionManager()->method(id)->capabilities();

        if (auto *provider = pluginProvider(id)) {
            info.pluginProvided = true;
            info.icon           = provider->icon();
            info.uiCapabilities = provider->uiCapabilities();
        }
#ifdef IRIS_ENABLE_OMEMO
        else if (id == XMPP::OmemoEncryption::methodId()) {
            info.icon           = IconsetFactory::icon("psi/omemo").icon();
            info.uiCapabilities = EncryptionMethodProvider::KeyManagement | EncryptionMethodProvider::DeviceManagement
                | EncryptionMethodProvider::TrustManagement;
        }
#endif
        result.append(info);
    }

    return result;
}

bool PsiEncryptionController::hasMethod(const QString &methodId) const
{
    if (methodId.isEmpty())
        return true;
    if (methodId == QLatin1String(OpenPgpMethodId))
        return account_->hasPgp() && PsiOptions::instance()->getOption("plugins.auto-load.openpgp", false).toBool();
    return client_->encryptionManager()->method(methodId) != nullptr;
}

PsiEncryptionController::MethodInfo PsiEncryptionController::methodInfo(const QString &methodId) const
{
    for (const auto &info : methods(XMPP::EncryptionMethod::Capabilities {})) {
        if (info.id == methodId)
            return info;
    }
    return {};
}

QString PsiEncryptionController::selectedMethod(const XMPP::Jid &jid) const
{
    const QString bare = jid.bare();
    if (bare.isEmpty())
        return {};

    const auto explicitMethod = selectedMethods_.constFind(bare);
    if (explicitMethod != selectedMethods_.cend())
        return *explicitMethod;

    // Migration policy: retain the old OpenPGP/OMEMO per-contact defaults
    // until the user explicitly chooses a method in the unified selector.
    if (account_->hasPgp() && PsiOptions::instance()->getOption("plugins.auto-load.openpgp", false).toBool()) {
        const bool alwaysPgp = PsiOptions::instance()->getOption("options.pgp.always-enabled", false).toBool();
        if (alwaysPgp || account_->isPgpEnabled(jid))
            return QString::fromLatin1(OpenPgpMethodId);
    }

#ifdef IRIS_ENABLE_OMEMO
    if (omemo_ && omemoStorage_) {
        const bool alwaysEnabled
            = PsiOptions::instance()->getOption("plugins.options.omemo.always-enabled", false).toBool();
        const bool enabledByDefault
            = PsiOptions::instance()->getOption("plugins.options.omemo.enabled-by-default", false).toBool();
        if (alwaysEnabled || (enabledByDefault && !legacyOmemoDisabled_.contains(bare))
            || legacyOmemoEnabled_.contains(bare)) {
            return XMPP::OmemoEncryption::methodId();
        }
    }
#endif
    return {};
}

bool PsiEncryptionController::setSelectedMethod(const XMPP::Jid &jid, const QString &methodId)
{
    const QString bare = jid.bare();
    if (bare.isEmpty())
        return false;
    if (!methodId.isEmpty() && !hasMethod(methodId))
        return false;
    if (selectedMethods_.contains(bare) && selectedMethods_.value(bare) == methodId)
        return true;

    selectedMethods_.insert(bare, methodId); // empty means explicit plaintext
    syncLegacySelection(bare, methodId);
    emit selectedMethodChanged(XMPP::Jid(bare), methodId);
    return true;
}

QMap<QString, QString> PsiEncryptionController::selectedMethods() const { return selectedMethods_; }

void PsiEncryptionController::setSelectedMethods(const QMap<QString, QString> &methods)
{
    if (selectedMethods_ == methods)
        return;
    selectedMethods_ = methods;
    emit methodsChanged();
}

XMPP::Jid PsiEncryptionController::capabilityJid(const XMPP::Jid &jid) const
{
    if (!jid.resource().isEmpty())
        return jid;

    auto *item = account_->findFirstRelevant(jid);
    if (!item || !item->isAvailable() || item->userResourceList().isEmpty())
        return jid;

    const auto priority = item->userResourceList().priority();
    if (priority == item->userResourceList().end())
        return jid;
    return jid.withResource(priority->name());
}

PsiEncryptionController::Availability PsiEncryptionController::availability(const QString   &methodId,
                                                                            const XMPP::Jid &jid) const
{
    if (methodId.isEmpty())
        return Availability::Available;
    if (!hasMethod(methodId))
        return Availability::Unavailable;

    const auto peer = capabilityJid(jid);
    if (auto *provider = pluginProvider(methodId))
        return provider->isAvailable(pluginMethods_.value(provider)->accountId, peer.full())
            ? Availability::Available
            : Availability::Unavailable;

#ifdef IRIS_ENABLE_OMEMO
    if (methodId == XMPP::OmemoEncryption::methodId() && omemo_) {
        if (omemo_->supportedProtocols() == XMPP::OmemoProtocols())
            return Availability::Unavailable;
        if (!peer.resource().isEmpty() && omemo_->preferredProtocolFor(peer))
            return Availability::Available;

        // OMEMO devices are discovered through PEP.  Missing resource caps
        // means "unknown", not "unsupported" (offline peers and our own
        // resources are common examples).  A cached active device is enough
        // to make availability definite.
        const auto supported = omemo_->supportedProtocols();
        for (const auto &device : omemo_->devices(peer)) {
            if (device.active && (device.protocols & supported) != XMPP::OmemoProtocols())
                return Availability::Available;
        }
        return Availability::Unknown;
    }
#endif

    return Availability::Available;
}

void PsiEncryptionController::reportError(const XMPP::Jid &jid, const QString &message)
{
    emit encryptionError(jid, message);
}

bool PsiEncryptionController::registerPluginMethod(int accountId, EncryptionMethodProvider *provider,
                                                   QObject *pluginLifetime)
{
    if (!provider || provider->id().isEmpty() || pluginMethods_.contains(provider))
        return false;
    if (provider->id() == QLatin1String(OpenPgpMethodId) || client_->encryptionManager()->method(provider->id()))
        return false;

    auto *registration      = new PluginRegistration;
    registration->accountId = accountId;
    registration->provider  = provider;
    registration->adapter   = new PluginMethodAdapter(accountId, provider, this);

    pluginMethodsById_.insert(provider->id(), provider);
    pluginMethods_.insert(provider, registration);

    auto *adapter = registration->adapter;
    if (!client_->encryptionManager()->registerMethod(adapter)) {
        pluginMethodsById_.remove(provider->id());
        pluginMethods_.remove(provider);
        delete adapter;
        delete registration;
        return false;
    }

    if (pluginLifetime) {
        pluginMethods_.value(provider)->lifetimeConnection = connect(
            pluginLifetime, &QObject::destroyed, this, [this, provider]() { unregisterPluginMethod(provider); });
    }
    return true;
}

void PsiEncryptionController::unregisterPluginMethod(EncryptionMethodProvider *provider)
{
    auto it = pluginMethods_.find(provider);
    if (it == pluginMethods_.end())
        return;

    auto *registration = it.value();
    pluginMethods_.erase(it);
    pluginMethodsById_.remove(registration->adapter->id());
    disconnect(registration->lifetimeConnection);

    registration->adapter->detachProvider();
    client_->encryptionManager()->unregisterMethod(registration->adapter);
    delete registration->adapter;
    delete registration;
}

void PsiEncryptionController::pluginMethodStateChanged(EncryptionMethodProvider *provider)
{
    const auto it = pluginMethods_.constFind(provider);
    if (it == pluginMethods_.cend())
        return;
    emit methodStateChanged(it.value()->adapter->id());
}

QWidget *PsiEncryptionController::createSettingsWidget(const QString &methodId, QWidget *parent) const
{
    auto *provider = pluginProvider(methodId);
    if (!provider)
        return nullptr;
    const auto &registration = pluginMethods_.value(provider);
    return provider->createSettingsWidget(registration->accountId, parent);
}

QWidget *PsiEncryptionController::createKeyManagementWidget(const QString &methodId, const XMPP::Jid &jid,
                                                            QWidget *parent) const
{
    auto *provider = pluginProvider(methodId);
    if (!provider)
        return nullptr;
    const auto &registration = pluginMethods_.value(provider);
    return provider->createKeyManagementWidget(registration->accountId, jid.full(), parent);
}

QWidget *PsiEncryptionController::createDeviceManagementWidget(const QString &methodId, const XMPP::Jid &jid,
                                                               QWidget *parent) const
{
    auto *provider = pluginProvider(methodId);
    if (!provider)
        return nullptr;
    const auto &registration = pluginMethods_.value(provider);
    return provider->createDeviceManagementWidget(registration->accountId, jid.full(), parent);
}

QWidget *PsiEncryptionController::createTrustManagementWidget(const QString &methodId, const XMPP::Jid &jid,
                                                              QWidget *parent) const
{
    auto *provider = pluginProvider(methodId);
    if (!provider)
        return nullptr;
    const auto &registration = pluginMethods_.value(provider);
    return provider->createTrustManagementWidget(registration->accountId, jid.full(), parent);
}

bool PsiEncryptionController::requestTrustDecision(const QString &methodId, const XMPP::Jid &peer,
                                                   bool includeOwnDevices, QWidget *parent,
                                                   std::function<void(bool)> completion)
{
#ifdef IRIS_ENABLE_OMEMO
    if (!omemo_ || methodId != XMPP::OmemoEncryption::methodId())
        return false;

    const QString peerBare = peer.bare();
    if (peerBare.isEmpty())
        return false;

    const QString ownBare        = client_->jid().bare();
    const auto    collectPending = [this, peerBare, ownBare, includeOwnDevices]() {
        QList<XMPP::OmemoDeviceInfo> pending;
        const auto                   append = [&pending](const QList<XMPP::OmemoDeviceInfo> &devices) {
            for (const auto &device : devices) {
                if (!device.identityKey.isEmpty() && device.active && needsManualTrustDecision(device.trust))
                    pending.append(device);
            }
        };
        append(omemo_->devices(XMPP::Jid(peerBare)));
        if (includeOwnDevices && !ownBare.isEmpty() && ownBare != peerBare)
            append(omemo_->devices(XMPP::Jid(ownBare)));
        return pending;
    };

    if (collectPending().isEmpty())
        return false;

    // Coalesce simultaneous failed sends for the same trust scope into one
    // prompt. All callers are resumed after the single user decision.
    const QString promptKey = peerBare + QLatin1Char('|') + (includeOwnDevices ? QLatin1Char('1') : QLatin1Char('0'));
    if (completion)
        trustPromptWaiters_[promptKey].append(std::move(completion));
    if (trustPromptsActive_.contains(promptKey))
        return true;
    trustPromptsActive_.insert(promptKey);

    QPointer<QWidget> parentGuard(parent);
    QTimer::singleShot(0, this, [this, promptKey, peerBare, includeOwnDevices, parentGuard, collectPending]() mutable {
        auto pending      = collectPending();
        bool retryAllowed = false;

        if (!pending.isEmpty()) {
            QDialog dialog(parentGuard.data());
            dialog.setWindowTitle(tr("Review OMEMO devices"));
            dialog.setModal(true);

            auto *layout = new QVBoxLayout(&dialog);
            auto *intro  = new QLabel(tr("New OMEMO device keys need a trust decision before they can be used for "
                                          "outgoing encrypted messages."),
                                      &dialog);
            intro->setWordWrap(true);
            layout->addWidget(intro);

            auto *tree = new QTreeWidget(&dialog);
            tree->setColumnCount(3);
            tree->setHeaderLabels({ tr("Device"), tr("Owner"), tr("Fingerprint") });
            tree->setRootIsDecorated(false);
            tree->setSelectionMode(QAbstractItemView::NoSelection);
            tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);

            for (qsizetype i = 0; i < pending.size(); ++i) {
                const auto   &device     = pending.at(i);
                auto         *item       = new QTreeWidgetItem(tree);
                const QString deviceName = device.label.isEmpty() ? tr("Device %1").arg(device.id)
                                                                  : tr("%1 (%2)").arg(device.label).arg(device.id);
                const bool    ownDevice  = device.owner.bare() == client_->jid().bare();
                item->setText(0, deviceName);
                item->setText(1, ownDevice ? tr("Your account") : device.owner.bare());
                item->setText(2, formatFingerprint(device.identityKey));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(0, Qt::Checked);
                item->setData(0, Qt::UserRole, static_cast<int>(i));
            }
            layout->addWidget(tree);

            auto *hint = new QLabel(
                includeOwnDevices
                    ? tr("Trust allows selected contact devices to receive encrypted messages and selected devices "
                         "of your own account to decrypt message copies. Distrust excludes those devices. Not now "
                         "leaves them undecided and cancels this encrypted send.")
                    : tr("Trust marks the selected sender devices as trusted. Distrust marks them as explicitly "
                         "untrusted. Not now keeps the message visible but leaves the devices undecided."),
                &dialog);
            hint->setWordWrap(true);
            layout->addWidget(hint);

            auto *buttons        = new QDialogButtonBox(&dialog);
            auto *trustButton    = buttons->addButton(tr("Trust selected"), QDialogButtonBox::AcceptRole);
            auto *distrustButton = buttons->addButton(tr("Distrust selected"), QDialogButtonBox::DestructiveRole);
            auto *notNowButton   = buttons->addButton(tr("Not now"), QDialogButtonBox::RejectRole);
            layout->addWidget(buttons);

            enum class Action { None, Trust, Distrust };
            Action action = Action::None;
            connect(trustButton, &QPushButton::clicked, &dialog, [&dialog, &action]() {
                action = Action::Trust;
                dialog.accept();
            });
            connect(distrustButton, &QPushButton::clicked, &dialog, [&dialog, &action]() {
                action = Action::Distrust;
                dialog.accept();
            });
            connect(notNowButton, &QPushButton::clicked, &dialog, &QDialog::reject);

            dialog.resize(760, 340);
            dialog.exec();

            if (action != Action::None) {
                for (int row = 0; row < tree->topLevelItemCount(); ++row) {
                    auto *item = tree->topLevelItem(row);
                    if (item->checkState(0) != Qt::Checked)
                        continue;
                    const int index = item->data(0, Qt::UserRole).toInt();
                    if (index < 0 || index >= static_cast<int>(pending.size()))
                        continue;
                    const auto &device = pending.at(index);
                    const auto  level  = action == Action::Trust ? XMPP::EncryptionTrustLevel::ManuallyTrusted
                                                                 : XMPP::EncryptionTrustLevel::Distrusted;
                    if (!omemo_->setTrustLevel(device.owner, device.identityKey, level))
                        emit encryptionError(device.owner, tr("Could not save the OMEMO trust decision."));
                }
                retryAllowed = collectPending().isEmpty();
            }
        }

        trustPromptsActive_.remove(promptKey);
        const auto waiters = trustPromptWaiters_.take(promptKey);
        for (const auto &waiter : waiters) {
            if (waiter)
                waiter(retryAllowed);
        }
    });
    return true;
#else
    Q_UNUSED(methodId);
    Q_UNUSED(peer);
    Q_UNUSED(includeOwnDevices);
    Q_UNUSED(parent);
    Q_UNUSED(completion);
    return false;
#endif
}

#ifdef IRIS_ENABLE_OMEMO
XMPP::OmemoEncryption *PsiEncryptionController::omemoEncryption() const { return omemo_; }

void PsiEncryptionController::setUpOmemo(const QString &deviceLabel)
{
    if (!omemo_)
        return;
    auto      *job    = omemo_->setUp(deviceLabel);
    const auto report = [this, job]() {
        if (!job->success())
            emit encryptionError({}, tr("OMEMO setup failed: %1").arg(job->errorString()));
    };
    if (job->isFinished())
        report();
    else
        connect(job, &XMPP::EncryptionJob::finished, this, report);
}
#endif

EncryptionMethodProvider *PsiEncryptionController::pluginProvider(const QString &methodId) const
{
    return pluginMethodsById_.value(methodId, nullptr);
}

void PsiEncryptionController::syncLegacySelection(const QString &bareJid, const QString &methodId)
{
    account_->setPgpEnabled(XMPP::Jid(bareJid), methodId == QLatin1String(OpenPgpMethodId));

#ifdef IRIS_ENABLE_OMEMO
    if (omemoStorage_ && omemoStorage_->isOpen()) {
        const bool omemoSelected = methodId == XMPP::OmemoEncryption::methodId();
        const bool enabledByDefault
            = PsiOptions::instance()->getOption("plugins.options.omemo.enabled-by-default", false).toBool();
        if (enabledByDefault) {
            omemoStorage_->setLegacyDisabled(bareJid, !omemoSelected);
            if (omemoSelected)
                legacyOmemoDisabled_.remove(bareJid);
            else
                legacyOmemoDisabled_.insert(bareJid);
        } else {
            omemoStorage_->setLegacyEnabled(bareJid, omemoSelected);
            if (omemoSelected)
                legacyOmemoEnabled_.insert(bareJid);
            else
                legacyOmemoEnabled_.remove(bareJid);
        }
    }
#else
    Q_UNUSED(bareJid);
#endif
}
