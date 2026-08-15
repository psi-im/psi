/*
 * psiencryptioncontroller.h - application-level encryption registry for Psi
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef PSIENCRYPTIONCONTROLLER_H
#define PSIENCRYPTIONCONTROLLER_H

#include "encryptionmethodprovider.h"
#include "iris/xmpp_encryption.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include <functional>
#include <memory>

class PsiAccount;
class QWidget;

namespace XMPP {
class Client;
}

#ifdef IRIS_ENABLE_OMEMO
class PsiOmemoStorage;
namespace XMPP {
class OmemoEncryption;
}
#endif

class PsiEncryptionController final : public QObject {
    Q_OBJECT
public:
    enum class Availability { Unknown, Available, Unavailable };

    struct MethodInfo {
        QString                                  id;
        QString                                  name;
        QIcon                                    icon;
        XMPP::EncryptionMethod::Capabilities     capabilities;
        EncryptionMethodProvider::UiCapabilities uiCapabilities;
        bool                                     pluginProvided = false;
    };

    PsiEncryptionController(PsiAccount *account, XMPP::Client *client, const QMap<QString, QString> &selectedMethods,
                            const QString &profileDataPath, const QString &accountId, QObject *parent = nullptr);
    ~PsiEncryptionController() override;

    XMPP::Client *client() const;

    QList<MethodInfo> methods(XMPP::EncryptionMethod::Capabilities capabilities
                              = XMPP::EncryptionMethod::XmppStanza) const;
    bool              hasMethod(const QString &methodId) const;
    MethodInfo        methodInfo(const QString &methodId) const;

    // Persistent per-contact preference. An open chat copies this value when
    // it is created and then owns an independent active method/session.
    QString selectedMethod(const XMPP::Jid &jid) const;
    bool    setSelectedMethod(const XMPP::Jid &jid, const QString &methodId);

    QMap<QString, QString> selectedMethods() const;
    void                   setSelectedMethods(const QMap<QString, QString> &methods);

    XMPP::Jid    capabilityJid(const XMPP::Jid &jid) const;
    Availability availability(const QString &methodId, const XMPP::Jid &jid) const;
    void         reportError(const XMPP::Jid &jid, const QString &message);

    bool registerPluginMethod(int accountId, EncryptionMethodProvider *provider, QObject *pluginLifetime = nullptr);
    void unregisterPluginMethod(EncryptionMethodProvider *provider);
    void pluginMethodStateChanged(EncryptionMethodProvider *provider);

    QWidget *createSettingsWidget(const QString &methodId, QWidget *parent = nullptr) const;
    QWidget *createKeyManagementWidget(const QString &methodId, const XMPP::Jid &jid, QWidget *parent = nullptr) const;
    QWidget *createDeviceManagementWidget(const QString &methodId, const XMPP::Jid &jid,
                                          QWidget *parent = nullptr) const;
    QWidget *createTrustManagementWidget(const QString &methodId, const XMPP::Jid &jid,
                                         QWidget *parent = nullptr) const;

    /**
     * Ask the user to resolve identities that currently block an encrypted
     * operation.  The callback receives true only when the operation can be
     * retried without another pending trust decision. A non-zero deviceId
     * requests explicit approval to repair that device's broken session even
     * when its identity was already trusted.
     */
    bool requestTrustDecision(const QString &methodId, const XMPP::Jid &peer, bool includeOwnDevices,
                              QWidget *parent = nullptr, std::function<void(bool)> completion = {},
                              quint32 deviceId = 0);

#ifdef IRIS_ENABLE_OMEMO
    XMPP::OmemoEncryption *omemoEncryption() const;
    void                   setUpOmemo(const QString &deviceLabel);
#endif

signals:
    void methodsChanged();
    void methodStateChanged(const QString &methodId);
    void peerAvailabilityChanged(const XMPP::Jid &jid);
    void selectedMethodChanged(const XMPP::Jid &jid, const QString &methodId);
    void encryptionError(const XMPP::Jid &jid, const QString &message);

private:
    class PluginMethodAdapter;
    struct PluginRegistration;

    EncryptionMethodProvider *pluginProvider(const QString &methodId) const;
    void                      syncLegacySelection(const QString &bareJid, const QString &methodId);
    void                      recoverDecryption(const QString &methodId, const XMPP::Jid &displayPeer,
                                                const XMPP::EncryptionMetadata &metadata);
    void                      performDecryptionRecovery(const QString &methodId, const XMPP::Jid &displayPeer,
                                                       const XMPP::EncryptionMetadata &metadata);

    PsiAccount                                             *account_ = nullptr;
    XMPP::Client                                           *client_  = nullptr;
    QMap<QString, QString>                                  selectedMethods_;
    QHash<EncryptionMethodProvider *, PluginRegistration *> pluginMethods_;
    QHash<QString, EncryptionMethodProvider *>              pluginMethodsById_;
    QHash<QString, QList<std::function<void(bool)>>>        trustPromptWaiters_;
    QSet<QString>                                           trustPromptsActive_;
    QHash<QString, qint64>                                  decryptionRecoveryAttempts_;
    QSet<QString>                                           decryptionRecoveriesActive_;

#ifdef IRIS_ENABLE_OMEMO
    std::unique_ptr<PsiOmemoStorage> omemoStorage_;
    XMPP::OmemoEncryption           *omemo_ = nullptr;
    QSet<QString>                    legacyOmemoEnabled_;
    QSet<QString>                    legacyOmemoDisabled_;
#endif
};

#endif // PSIENCRYPTIONCONTROLLER_H
