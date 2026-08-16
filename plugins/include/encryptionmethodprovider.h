/*
 * encryptionmethodprovider.h - plugin-side end-to-end encryption method API
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef ENCRYPTIONMETHODPROVIDER_H
#define ENCRYPTIONMETHODPROVIDER_H

#include <QByteArray>
#include <QDomElement>
#include <QFlags>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <utility>

class QObject;
class QWidget;

/**
 * Stable Psi plugin-side abstraction of an encryption method.
 *
 * This interface deliberately contains no Iris/XMPP implementation types.
 * Psi adapts it internally to the encryption backend used by the application.
 * A provider is owned by the plugin and must remain alive while registered.
 */
class EncryptionMethodProvider {
public:
    enum Capability {
        XmppStanza  = 0x01,
        DataMessage = 0x02,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum UiCapability {
        Settings         = 0x01,
        KeyManagement    = 0x02,
        DeviceManagement = 0x04,
        TrustManagement  = 0x08,
    };
    Q_DECLARE_FLAGS(UiCapabilities, UiCapability)

    enum class Availability {
        Unknown,
        Available,
        Unavailable,
    };

    enum class Error {
        None,
        Unsupported,
        InvalidInput,
        NoRecipients,
        NoSession,
        UntrustedIdentity,
        CryptoError,
        AuthenticationFailed,
        ProtocolError,
        StorageError,
        NetworkError,
        Cancelled,
    };

    struct Metadata {
        QString     sender;
        quint32     senderDeviceId = 0;
        QByteArray  senderKey;
        bool        protocolOnly = false;
        QVariantMap details;
    };

    struct Context {
        QStringList recipients;
        bool        hasReplyTo = false;
        Metadata    replyTo;
        QVariantMap options;
    };

    struct Result {
        bool        success = false;
        Error       error   = Error::None;
        QString     errorString;
        QDomElement stanza;
        QByteArray  data;
        Metadata    metadata;
    };

    using Completion = std::function<void(Result)>;

    class Session {
    public:
        virtual ~Session() = default;

        /** completion must be called exactly once while context is alive. */
        virtual void encryptStanza(const QDomElement &stanza, QObject *context, Completion completion) = 0;
        virtual void decryptStanza(const QDomElement &stanza, QObject *context, Completion completion) = 0;

        virtual void encryptData(const QByteArray &data, QObject *context, Completion completion)
        {
            Q_UNUSED(data);
            Q_UNUSED(context);
            Result result;
            result.error       = Error::Unsupported;
            result.errorString = QStringLiteral("Data encryption is not supported by this session");
            completion(std::move(result));
        }

        virtual void decryptData(const QByteArray &data, QObject *context, Completion completion)
        {
            Q_UNUSED(data);
            Q_UNUSED(context);
            Result result;
            result.error       = Error::Unsupported;
            result.errorString = QStringLiteral("Data decryption is not supported by this session");
            completion(std::move(result));
        }
    };

    virtual ~EncryptionMethodProvider() = default;

    virtual QString      id() const   = 0;
    virtual QString      name() const = 0;
    virtual QIcon        icon() const { return {}; }
    virtual Capabilities capabilities() const = 0;
    virtual QStringList  features() const     = 0;

    virtual UiCapabilities uiCapabilities() const { return {}; }

    /** Return whether the method can decrypt this incoming stanza. */
    virtual bool canDecrypt(int account, const QDomElement &stanza) const = 0;

    /**
     * Application-level availability for a concrete peer/resource.
     * The JID is passed as a string so the plugin API does not expose Iris.
     */
    virtual bool isAvailable(int account, const QString &fullJid) const
    {
        Q_UNUSED(account);
        Q_UNUSED(fullJid);
        return true;
    }

    /**
     * Tri-state availability for methods whose peer support may be unknown.
     * The default preserves the original boolean API for existing providers.
     */
    virtual Availability availability(int account, const QString &fullJid) const
    {
        return isAvailable(account, fullJid) ? Availability::Available : Availability::Unavailable;
    }

    /**
     * Create a conversation/transfer session bound to sessionContext. Psi owns
     * the returned object. Destroying it must cancel its outstanding work.
     */
    virtual Session *startSession(int account, const Context &sessionContext) = 0;

    // Optional UI factories. Psi owns the returned widgets.
    virtual QWidget *createSettingsWidget(int account, QWidget *parent)
    {
        Q_UNUSED(account);
        Q_UNUSED(parent);
        return nullptr;
    }
    virtual QWidget *createKeyManagementWidget(int account, const QString &jid, QWidget *parent)
    {
        Q_UNUSED(account);
        Q_UNUSED(jid);
        Q_UNUSED(parent);
        return nullptr;
    }
    virtual QWidget *createDeviceManagementWidget(int account, const QString &jid, QWidget *parent)
    {
        Q_UNUSED(account);
        Q_UNUSED(jid);
        Q_UNUSED(parent);
        return nullptr;
    }
    virtual QWidget *createTrustManagementWidget(int account, const QString &jid, QWidget *parent)
    {
        Q_UNUSED(account);
        Q_UNUSED(jid);
        Q_UNUSED(parent);
        return nullptr;
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EncryptionMethodProvider::Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(EncryptionMethodProvider::UiCapabilities)

#endif // ENCRYPTIONMETHODPROVIDER_H
