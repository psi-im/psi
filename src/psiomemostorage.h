/*
 * psiomemostorage.h - persistent OMEMO state for Psi
 * Copyright (C) 2018-2024 Vyacheslav Karpukhin, Psi IM team
 * Copyright (C) 2020 Boris Pek
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef PSIOMEMOSTORAGE_H
#define PSIOMEMOSTORAGE_H

#ifdef IRIS_ENABLE_OMEMO

#include "iris/xmpp_encryption.h"
#include "iris/xmpp_omemostorage.h"

#include <QSet>
#include <QString>

#include <memory>
#include <optional>

class PsiOmemoStorage final : public XMPP::OmemoStorage, public XMPP::EncryptionTrustStorage {
public:
    enum class LegacyTrust { Undecided = 0, Trusted = 1, Untrusted = 2 };

    PsiOmemoStorage(const QString &dataPath, const QString &accountId);
    ~PsiOmemoStorage() override;

    bool    isOpen() const;
    QString errorString() const;

    OmemoData allData() const override;
    bool      setOwnDevice(const std::optional<OwnDevice> &device) override;
    bool      addSignedPreKeyPair(uint32_t keyId, const SignedPreKeyPair &keyPair) override;
    bool      removeSignedPreKeyPair(uint32_t keyId) override;
    bool      addPreKeyPairs(const QHash<uint32_t, QByteArray> &keyPairs) override;
    bool      removePreKeyPair(uint32_t keyId) override;
    bool      addDevice(const QString &jid, uint32_t deviceId, const Device &device) override;
    bool      removeDevice(const QString &jid, uint32_t deviceId) override;
    bool      removeDevices(const QString &jid) override;
    bool      resetAll() override;

    XMPP::EncryptionTrustLevel trustLevel(const QString &methodId, const XMPP::Jid &owner,
                                          const QByteArray &keyId) const override;
    bool                       setTrustLevel(const QString &methodId, const XMPP::Jid &owner, const QByteArray &keyId,
                                             XMPP::EncryptionTrustLevel level) override;
    bool removeTrust(const QString &methodId, const XMPP::Jid &owner, const QByteArray &keyId) override;

    // Compatibility with the historical Psi OMEMO plugin database.  These are
    // used only while migrating user policy/trust and keeping downgrade
    // compatibility; Iris never depends on this API.
    QSet<QString>              legacyEnabledJids() const;
    QSet<QString>              legacyDisabledJids() const;
    bool                       setLegacyEnabled(const QString &jid, bool enabled);
    bool                       setLegacyDisabled(const QString &jid, bool disabled);
    std::optional<LegacyTrust> legacyTrust(const QString &jid, uint32_t deviceId) const;
    bool                       setLegacyTrust(const QString &jid, uint32_t deviceId, LegacyTrust trust);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // IRIS_ENABLE_OMEMO
#endif // PSIOMEMOSTORAGE_H
