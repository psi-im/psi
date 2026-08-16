/*
 * psiomemostorage.cpp - persistent OMEMO state for Psi
 * Copyright (C) 2018-2024 Vyacheslav Karpukhin, Psi IM team
 * Copyright (C) 2020 Boris Pek
 * Copyright (C) 2026 Sergey Ilinykh
 *
 * The compatibility schema and legacy state migration are based on the
 * historical Psi OMEMO plugin storage written by Vyacheslav Karpukhin and
 * Boris Pek.  New protocol state is stored alongside it so an upgraded profile
 * keeps its device identity, legacy sessions and trust decisions.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "psiomemostorage.h"

#ifdef IRIS_ENABLE_OMEMO

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString bareJid(const QString &jid)
{
    const auto bare = XMPP::Jid(jid).bare();
    return bare.isEmpty() ? jid : bare;
}

QDateTime dateTimeFromDb(const QVariant &value)
{
    auto dt = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(value.toString(), Qt::ISODate);
    return dt;
}

QString dateTimeToDb(const QDateTime &value)
{
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QString();
}
} // namespace

class PsiOmemoStorage::Private {
public:
    QString connectionName;
    QString error;

    QSqlDatabase database() const { return QSqlDatabase::database(connectionName, false); }

    bool exec(const QString &sql) const
    {
        QSqlQuery q(database());
        if (q.exec(sql))
            return true;
        qWarning() << "OMEMO storage:" << q.lastError() << sql;
        return false;
    }

    QVariant simpleValue(const QString &key) const
    {
        QSqlQuery q(database());
        q.prepare(QStringLiteral("SELECT value FROM simple_store WHERE key = ?"));
        q.addBindValue(key);
        if (!q.exec() || !q.next())
            return {};
        return q.value(0);
    }

    bool setSimpleValue(const QString &key, const QVariant &value) const
    {
        QSqlQuery q(database());
        q.prepare(QStringLiteral("INSERT OR REPLACE INTO simple_store (key, value) VALUES (?, ?)"));
        q.addBindValue(key);
        q.addBindValue(value);
        return q.exec();
    }

    bool removeSimpleValue(const QString &key) const
    {
        QSqlQuery q(database());
        q.prepare(QStringLiteral("DELETE FROM simple_store WHERE key = ?"));
        q.addBindValue(key);
        return q.exec();
    }

    bool initialize()
    {
        auto db = database();
        if (!db.isValid() || !db.isOpen())
            return false;

        const QStringList statements {
            QStringLiteral("CREATE TABLE IF NOT EXISTS enabled_buddies (jid TEXT NOT NULL PRIMARY KEY)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS disabled_buddies (jid TEXT NOT NULL PRIMARY KEY)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS devices (jid TEXT NOT NULL, device_id INTEGER NOT NULL, "
                           "trust INTEGER NOT NULL, label TEXT, PRIMARY KEY(jid, device_id))"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS identity_key_store (jid TEXT NOT NULL, device_id INTEGER NOT "
                           "NULL, key BLOB NOT NULL, PRIMARY KEY(jid, device_id))"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS pre_key_store (id INTEGER NOT NULL PRIMARY KEY, pre_key BLOB "
                           "NOT NULL)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS session_store (jid TEXT NOT NULL, device_id INTEGER NOT NULL, "
                           "session BLOB NOT NULL, PRIMARY KEY(jid, device_id))"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS simple_store (key TEXT NOT NULL PRIMARY KEY, value BLOB NOT "
                           "NULL)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS omemo_signed_pre_key_store (id INTEGER NOT NULL PRIMARY KEY, "
                           "created_at TEXT, data BLOB NOT NULL)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS omemo_device_meta (jid TEXT NOT NULL, device_id INTEGER NOT "
                           "NULL, label TEXT, label_signature BLOB, label_verified INTEGER NOT NULL DEFAULT 0, PRIMARY "
                           "KEY(jid, device_id))"),
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS omemo_protocol_state (jid TEXT NOT NULL, device_id INTEGER NOT "
                "NULL, protocol INTEGER NOT NULL, label TEXT, label_signature BLOB, label_verified INTEGER, "
                "key_id BLOB, session BLOB, last_received_ratchet_key BLOB, unresponded_sent INTEGER NOT NULL "
                "DEFAULT 0, unresponded_received INTEGER NOT NULL DEFAULT 0, removed_at TEXT, PRIMARY KEY(jid, "
                "device_id, protocol))"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS encryption_trust (method TEXT NOT NULL, jid TEXT NOT NULL, key "
                           "BLOB NOT NULL, trust INTEGER NOT NULL, PRIMARY KEY(method, jid, key))")
        };
        for (const auto &sql : statements) {
            if (!exec(sql))
                return false;
        }

        // Old versions of the plugin had no label column.
        QSqlQuery info(db);
        bool      hasLabel = false;
        if (info.exec(QStringLiteral("PRAGMA table_info(devices)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("label")) {
                    hasLabel = true;
                    break;
                }
            }
        }
        if (!hasLabel && !exec(QStringLiteral("ALTER TABLE devices ADD COLUMN label TEXT")))
            return false;

        const auto ensureColumn = [this, &db](const QString &table, const QString &column, const QString &definition) {
            QSqlQuery columns(db);
            if (!columns.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
                return false;
            while (columns.next()) {
                if (columns.value(1).toString() == column)
                    return true;
            }
            return exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, definition));
        };
        // Protocol-specific device metadata used to live in omemo_device_meta
        // under (jid, device_id). Keep that table as a migration fallback, but
        // store all new metadata with the protocol state so equal numeric ids
        // in legacy OMEMO and OMEMO 2 remain independent.
        if (!ensureColumn(QStringLiteral("omemo_protocol_state"), QStringLiteral("label"), QStringLiteral("TEXT"))
            || !ensureColumn(QStringLiteral("omemo_protocol_state"), QStringLiteral("label_signature"),
                             QStringLiteral("BLOB"))
            || !ensureColumn(QStringLiteral("omemo_protocol_state"), QStringLiteral("label_verified"),
                             QStringLiteral("INTEGER"))) {
            return false;
        }
        return true;
    }
};

PsiOmemoStorage::PsiOmemoStorage(const QString &dataPath, const QString &accountId) : d(std::make_unique<Private>())
{
    QDir dir(dataPath);
    dir.mkpath(QStringLiteral("."));

    // Match the historical plugin filename byte-for-byte. OMEMO::getSignal()
    // stripped QUuid braces before passing the account id to Storage::init().
    QString legacyAccountId = accountId;
    legacyAccountId.remove(QLatin1Char('{'));
    legacyAccountId.remove(QLatin1Char('}'));
    const QString oldShared = dir.filePath(QStringLiteral("omemo.sqlite"));
    const QString fileName  = QStringLiteral("omemo-%1.sqlite").arg(legacyAccountId);
    const QString filePath  = dir.filePath(fileName);
    if (QFileInfo::exists(oldShared) && !QFileInfo::exists(filePath))
        QFile::rename(oldShared, filePath);

    d->connectionName = QStringLiteral("Psi OMEMO %1 %2").arg(accountId, QUuid::createUuid().toString());
    auto db           = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), d->connectionName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        d->error = db.lastError().text();
        return;
    }
    if (!d->initialize())
        d->error = db.lastError().text().isEmpty() ? QStringLiteral("Could not initialize OMEMO database")
                                                   : db.lastError().text();
}

PsiOmemoStorage::~PsiOmemoStorage()
{
    const QString name = d->connectionName;
    {
        auto db = QSqlDatabase::database(name, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

bool    PsiOmemoStorage::isOpen() const { return d->error.isEmpty() && d->database().isOpen(); }
QString PsiOmemoStorage::errorString() const { return d->error; }

XMPP::OmemoStorage::OmemoData PsiOmemoStorage::allData() const
{
    OmemoData data;
    if (!isOpen())
        return data;

    const auto registrationId = d->simpleValue(QStringLiteral("registration_id"));
    const auto publicKey      = d->simpleValue(QStringLiteral("own_public_key"));
    const auto privateKey     = d->simpleValue(QStringLiteral("own_private_key"));
    if (registrationId.isValid() && !publicKey.toByteArray().isEmpty() && !privateKey.toByteArray().isEmpty()) {
        OwnDevice own;
        own.id                   = registrationId.toUInt();
        own.publicIdentityKey    = publicKey.toByteArray();
        own.privateIdentityKey   = privateKey.toByteArray();
        own.label                = d->simpleValue(QStringLiteral("device_label")).toString();
        own.latestSignedPreKeyId = qMax<uint32_t>(1, d->simpleValue(QStringLiteral("signed_pre_key_id")).toUInt());

        QSqlQuery maxPre(d->database());
        if (maxPre.exec(QStringLiteral("SELECT MAX(id) FROM pre_key_store")) && maxPre.next())
            own.latestPreKeyId = qMax<uint32_t>(1, maxPre.value(0).toUInt());
        data.ownDevice = own;
    }

    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral("SELECT id, created_at, data FROM omemo_signed_pre_key_store"))) {
            while (q.next()) {
                SignedPreKeyPair pair;
                pair.creationDate = dateTimeFromDb(q.value(1));
                pair.data         = q.value(2).toByteArray();
                data.signedPreKeyPairs.insert(q.value(0).toUInt(), pair);
            }
        }
        const auto oldId   = d->simpleValue(QStringLiteral("signed_pre_key_id"));
        const auto oldData = d->simpleValue(QStringLiteral("signed_pre_key"));
        if (oldId.isValid() && !oldData.toByteArray().isEmpty() && !data.signedPreKeyPairs.contains(oldId.toUInt())) {
            data.signedPreKeyPairs.insert(
                oldId.toUInt(),
                { QFileInfo(d->database().databaseName()).lastModified().toUTC(), oldData.toByteArray() });
        }
    }

    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral("SELECT id, pre_key FROM pre_key_store"))) {
            while (q.next())
                data.preKeyPairs.insert(q.value(0).toUInt(), q.value(1).toByteArray());
        }
    }

    // Legacy active device list and labels. Keep the active keys in memory so
    // loading identity/session rows does not perform an SQL query per row.
    QSet<QString> activeLegacyDevices;
    const auto    legacyDeviceKey
        = [](const QString &owner, uint32_t id) { return owner + QLatin1Char('\n') + QString::number(id); };
    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral("SELECT jid, device_id, label FROM devices"))) {
            while (q.next()) {
                const QString owner = bareJid(q.value(0).toString());
                const auto    id    = q.value(1).toUInt();
                auto         &state = data.devices[owner][id].protocols[XMPP::OmemoProtocol::Legacy];
                state.label         = q.value(2).toString();
                // Active legacy record: invalid removal date.
                activeLegacyDevices.insert(legacyDeviceKey(owner, id));
            }
        }
    }

    // Legacy identities and sessions are intentionally retained even after a
    // device drops from the active list.  Preserve that historical behavior.
    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral("SELECT jid, device_id, key FROM identity_key_store"))) {
            while (q.next()) {
                const QString owner = bareJid(q.value(0).toString());
                const auto    id    = q.value(1).toUInt();
                auto         &state = data.devices[owner][id].protocols[XMPP::OmemoProtocol::Legacy];
                state.keyId         = q.value(2).toByteArray();
                if (!activeLegacyDevices.contains(legacyDeviceKey(owner, id)))
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
                    state.removalFromDeviceListDate = QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
#else
                    state.removalFromDeviceListDate = QDateTime::fromMSecsSinceEpoch(0, QTimeZone::UTC);
#endif
            }
        }
    }
    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral("SELECT jid, device_id, session FROM session_store"))) {
            while (q.next()) {
                const QString owner = bareJid(q.value(0).toString());
                const auto    id    = q.value(1).toUInt();
                auto         &state = data.devices[owner][id].protocols[XMPP::OmemoProtocol::Legacy];
                state.session       = q.value(2).toByteArray();
                if (!activeLegacyDevices.contains(legacyDeviceKey(owner, id)))
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
                    state.removalFromDeviceListDate = QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
#else
                    state.removalFromDeviceListDate = QDateTime::fromMSecsSinceEpoch(0, QTimeZone::UTC);
#endif
            }
        }
    }

    // Migration fallback for databases written before device metadata became
    // protocol-specific. Do not create a device from this table alone: a
    // stale metadata row must not resurrect a profile which is absent from
    // omemo_protocol_state.
    struct OldModernMeta {
        QString    label;
        QByteArray signature;
        bool       verified = false;
    };
    QHash<QString, OldModernMeta> oldModernMeta;
    const auto                    metaKey
        = [](const QString &owner, uint32_t id) { return owner + QLatin1Char('\n') + QString::number(id); };
    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral(
                "SELECT jid, device_id, label, label_signature, label_verified FROM omemo_device_meta"))) {
            while (q.next()) {
                const QString owner = bareJid(q.value(0).toString());
                oldModernMeta.insert(metaKey(owner, q.value(1).toUInt()),
                                     { q.value(2).toString(), q.value(3).toByteArray(), q.value(4).toBool() });
            }
        }
    }
    {
        QSqlQuery q(d->database());
        if (q.exec(QStringLiteral(
                "SELECT jid, device_id, protocol, label, label_signature, label_verified, key_id, session, "
                "last_received_ratchet_key, unresponded_sent, unresponded_received, removed_at "
                "FROM omemo_protocol_state"))) {
            while (q.next()) {
                const auto protocol = static_cast<XMPP::OmemoProtocol>(q.value(2).toUInt());
                if (protocol != XMPP::OmemoProtocol::Legacy && protocol != XMPP::OmemoProtocol::Omemo2)
                    continue;
                auto &state = data.devices[bareJid(q.value(0).toString())][q.value(1).toUInt()].protocols[protocol];
                // Rows created before this schema change expose NULL in these
                // columns. Preserve the OMEMO 2 migration fallback in that
                // case, but only for a real protocol-state row.
                const auto oldMeta
                    = oldModernMeta.constFind(metaKey(bareJid(q.value(0).toString()), q.value(1).toUInt()));
                if (!q.value(3).isNull())
                    state.label = q.value(3).toString();
                else if (protocol == XMPP::OmemoProtocol::Omemo2 && oldMeta != oldModernMeta.cend())
                    state.label = oldMeta->label;
                if (!q.value(4).isNull())
                    state.labelSignature = q.value(4).toByteArray();
                else if (protocol == XMPP::OmemoProtocol::Omemo2 && oldMeta != oldModernMeta.cend())
                    state.labelSignature = oldMeta->signature;
                if (!q.value(5).isNull())
                    state.labelVerified = q.value(5).toBool();
                else if (protocol == XMPP::OmemoProtocol::Omemo2 && oldMeta != oldModernMeta.cend())
                    state.labelVerified = oldMeta->verified;
                state.keyId                           = q.value(6).toByteArray();
                state.session                         = q.value(7).toByteArray();
                state.lastReceivedRatchetKey          = q.value(8).toByteArray();
                state.unrespondedSentStanzasCount     = q.value(9).toInt();
                state.unrespondedReceivedStanzasCount = q.value(10).toInt();
                state.removalFromDeviceListDate       = dateTimeFromDb(q.value(11));
            }
        }
    }
    return data;
}

bool PsiOmemoStorage::setOwnDevice(const std::optional<OwnDevice> &device)
{
    if (!isOpen())
        return false;
    if (!device) {
        return d->removeSimpleValue(QStringLiteral("registration_id"))
            && d->removeSimpleValue(QStringLiteral("own_public_key"))
            && d->removeSimpleValue(QStringLiteral("own_private_key"))
            && d->removeSimpleValue(QStringLiteral("device_label"))
            && d->removeSimpleValue(QStringLiteral("signed_pre_key_id"))
            && d->removeSimpleValue(QStringLiteral("signed_pre_key"));
    }
    bool ok = d->setSimpleValue(QStringLiteral("registration_id"), device->id)
        && d->setSimpleValue(QStringLiteral("own_public_key"), device->publicIdentityKey)
        && d->setSimpleValue(QStringLiteral("own_private_key"), device->privateIdentityKey)
        && d->setSimpleValue(QStringLiteral("device_label"), device->label)
        && d->setSimpleValue(QStringLiteral("signed_pre_key_id"), device->latestSignedPreKeyId);

    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("SELECT data FROM omemo_signed_pre_key_store WHERE id = ?"));
    q.addBindValue(device->latestSignedPreKeyId);
    if (q.exec() && q.next())
        ok = d->setSimpleValue(QStringLiteral("signed_pre_key"), q.value(0).toByteArray()) && ok;
    else
        ok = d->removeSimpleValue(QStringLiteral("signed_pre_key")) && ok;
    return ok;
}

bool PsiOmemoStorage::addSignedPreKeyPair(uint32_t keyId, const SignedPreKeyPair &keyPair)
{
    QSqlQuery q(d->database());
    q.prepare(
        QStringLiteral("INSERT OR REPLACE INTO omemo_signed_pre_key_store (id, created_at, data) VALUES (?, ?, ?)"));
    q.addBindValue(keyId);
    q.addBindValue(dateTimeToDb(keyPair.creationDate));
    q.addBindValue(keyPair.data);
    if (!q.exec())
        return false;
    if (d->simpleValue(QStringLiteral("signed_pre_key_id")).toUInt() == keyId)
        return d->setSimpleValue(QStringLiteral("signed_pre_key"), keyPair.data);
    return true;
}

bool PsiOmemoStorage::removeSignedPreKeyPair(uint32_t keyId)
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("DELETE FROM omemo_signed_pre_key_store WHERE id = ?"));
    q.addBindValue(keyId);
    const bool ok = q.exec();
    if (d->simpleValue(QStringLiteral("signed_pre_key_id")).toUInt() == keyId)
        d->removeSimpleValue(QStringLiteral("signed_pre_key"));
    return ok;
}

bool PsiOmemoStorage::addPreKeyPairs(const QHash<uint32_t, QByteArray> &keyPairs)
{
    auto db = d->database();
    if (!db.transaction())
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO pre_key_store (id, pre_key) VALUES (?, ?)"));
    for (auto it = keyPairs.cbegin(); it != keyPairs.cend(); ++it) {
        q.bindValue(0, it.key());
        q.bindValue(1, it.value());
        if (!q.exec()) {
            db.rollback();
            return false;
        }
        q.finish();
    }
    return db.commit();
}

bool PsiOmemoStorage::removePreKeyPair(uint32_t keyId)
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("DELETE FROM pre_key_store WHERE id = ?"));
    q.addBindValue(keyId);
    return q.exec();
}

bool PsiOmemoStorage::addDevice(const QString &jid, uint32_t deviceId, const Device &device)
{
    const QString owner = bareJid(jid);
    auto          db    = d->database();
    if (!db.transaction())
        return false;

    QSqlQuery clear(db);
    clear.prepare(QStringLiteral("DELETE FROM omemo_protocol_state WHERE jid = ? AND device_id = ?"));
    clear.addBindValue(owner);
    clear.addBindValue(deviceId);
    if (!clear.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery stateQuery(db);
    stateQuery.prepare(
        QStringLiteral("INSERT INTO omemo_protocol_state "
                       "(jid, device_id, protocol, label, label_signature, label_verified, key_id, session, "
                       "last_received_ratchet_key, unresponded_sent, unresponded_received, removed_at) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    for (auto it = device.protocols.cbegin(); it != device.protocols.cend(); ++it) {
        stateQuery.bindValue(0, owner);
        stateQuery.bindValue(1, deviceId);
        stateQuery.bindValue(2, static_cast<int>(it.key()));
        stateQuery.bindValue(3, it->label);
        stateQuery.bindValue(4, it->labelSignature);
        stateQuery.bindValue(5, it->labelVerified ? 1 : 0);
        stateQuery.bindValue(6, it->keyId);
        stateQuery.bindValue(7, it->session);
        stateQuery.bindValue(8, it->lastReceivedRatchetKey);
        stateQuery.bindValue(9, it->unrespondedSentStanzasCount);
        stateQuery.bindValue(10, it->unrespondedReceivedStanzasCount);
        stateQuery.bindValue(11, dateTimeToDb(it->removalFromDeviceListDate));
        if (!stateQuery.exec()) {
            db.rollback();
            return false;
        }
        stateQuery.finish();
    }

    const auto legacy = device.protocols.constFind(XMPP::OmemoProtocol::Legacy);
    if (legacy != device.protocols.cend()) {
        QSqlQuery identity(db);
        if (legacy->keyId.isEmpty()) {
            identity.prepare(QStringLiteral("DELETE FROM identity_key_store WHERE jid = ? AND device_id = ?"));
        } else {
            identity.prepare(
                QStringLiteral("INSERT OR REPLACE INTO identity_key_store (jid, device_id, key) VALUES (?, ?, ?)"));
        }
        identity.addBindValue(owner);
        identity.addBindValue(deviceId);
        if (!legacy->keyId.isEmpty())
            identity.addBindValue(legacy->keyId);
        if (!identity.exec()) {
            db.rollback();
            return false;
        }

        QSqlQuery session(db);
        if (legacy->session.isEmpty()) {
            session.prepare(QStringLiteral("DELETE FROM session_store WHERE jid = ? AND device_id = ?"));
        } else {
            session.prepare(
                QStringLiteral("INSERT OR REPLACE INTO session_store (jid, device_id, session) VALUES (?, ?, ?)"));
        }
        session.addBindValue(owner);
        session.addBindValue(deviceId);
        if (!legacy->session.isEmpty())
            session.addBindValue(legacy->session);
        if (!session.exec()) {
            db.rollback();
            return false;
        }

        if (!legacy->removalFromDeviceListDate.isValid()) {
            QSqlQuery insertDevice(db);
            insertDevice.prepare(
                QStringLiteral("INSERT OR IGNORE INTO devices (jid, device_id, trust, label) VALUES (?, ?, 0, ?)"));
            insertDevice.addBindValue(owner);
            insertDevice.addBindValue(deviceId);
            insertDevice.addBindValue(legacy->label);
            if (!insertDevice.exec()) {
                db.rollback();
                return false;
            }
            QSqlQuery label(db);
            label.prepare(QStringLiteral("UPDATE devices SET label = ? WHERE jid = ? AND device_id = ?"));
            label.addBindValue(legacy->label);
            label.addBindValue(owner);
            label.addBindValue(deviceId);
            if (!label.exec()) {
                db.rollback();
                return false;
            }
        } else {
            QSqlQuery removeActive(db);
            removeActive.prepare(QStringLiteral("DELETE FROM devices WHERE jid = ? AND device_id = ?"));
            removeActive.addBindValue(owner);
            removeActive.addBindValue(deviceId);
            if (!removeActive.exec()) {
                db.rollback();
                return false;
            }
        }
    } else {
        // Do not expose an OMEMO-2-only device to a downgraded legacy plugin.
        QSqlQuery removeLegacyActive(db);
        removeLegacyActive.prepare(QStringLiteral("DELETE FROM devices WHERE jid = ? AND device_id = ?"));
        removeLegacyActive.addBindValue(owner);
        removeLegacyActive.addBindValue(deviceId);
        if (!removeLegacyActive.exec()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool PsiOmemoStorage::removeDevice(const QString &jid, uint32_t deviceId)
{
    const QString owner = bareJid(jid);
    auto          db    = d->database();
    if (!db.transaction())
        return false;
    const QStringList tables { QStringLiteral("omemo_device_meta"), QStringLiteral("omemo_protocol_state"),
                               QStringLiteral("devices"), QStringLiteral("identity_key_store"),
                               QStringLiteral("session_store") };
    for (const auto &table : tables) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE jid = ? AND device_id = ?").arg(table));
        q.addBindValue(owner);
        q.addBindValue(deviceId);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool PsiOmemoStorage::removeDevices(const QString &jid)
{
    const QString owner = bareJid(jid);
    auto          db    = d->database();
    if (!db.transaction())
        return false;
    const QStringList tables { QStringLiteral("omemo_device_meta"), QStringLiteral("omemo_protocol_state"),
                               QStringLiteral("devices"), QStringLiteral("identity_key_store"),
                               QStringLiteral("session_store") };
    for (const auto &table : tables) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE jid = ?").arg(table));
        q.addBindValue(owner);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool PsiOmemoStorage::resetAll()
{
    auto db = d->database();
    if (!db.transaction())
        return false;
    const QStringList tables { QStringLiteral("omemo_signed_pre_key_store"),
                               QStringLiteral("omemo_device_meta"),
                               QStringLiteral("omemo_protocol_state"),
                               QStringLiteral("encryption_trust"),
                               QStringLiteral("devices"),
                               QStringLiteral("identity_key_store"),
                               QStringLiteral("pre_key_store"),
                               QStringLiteral("session_store") };
    for (const auto &table : tables) {
        if (!d->exec(QStringLiteral("DELETE FROM %1").arg(table))) {
            db.rollback();
            return false;
        }
    }
    const QStringList simpleKeys { QStringLiteral("registration_id"), QStringLiteral("own_public_key"),
                                   QStringLiteral("own_private_key"), QStringLiteral("signed_pre_key_id"),
                                   QStringLiteral("signed_pre_key"),  QStringLiteral("device_label") };
    for (const auto &key : simpleKeys) {
        if (!d->removeSimpleValue(key)) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

XMPP::EncryptionTrustLevel PsiOmemoStorage::trustLevel(const QString &methodId, const XMPP::Jid &owner,
                                                       const QByteArray &keyId) const
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("SELECT trust FROM encryption_trust WHERE method = ? AND jid = ? AND key = ?"));
    q.addBindValue(methodId);
    q.addBindValue(owner.bare());
    q.addBindValue(keyId);
    if (!q.exec() || !q.next())
        return XMPP::EncryptionTrustLevel::Undecided;
    return static_cast<XMPP::EncryptionTrustLevel>(q.value(0).toUInt());
}

bool PsiOmemoStorage::setTrustLevel(const QString &methodId, const XMPP::Jid &owner, const QByteArray &keyId,
                                    XMPP::EncryptionTrustLevel level)
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO encryption_trust (method, jid, key, trust) VALUES (?, ?, ?, ?)"));
    q.addBindValue(methodId);
    q.addBindValue(owner.bare());
    q.addBindValue(keyId);
    q.addBindValue(static_cast<int>(level));
    return q.exec();
}

bool PsiOmemoStorage::removeTrust(const QString &methodId, const XMPP::Jid &owner, const QByteArray &keyId)
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("DELETE FROM encryption_trust WHERE method = ? AND jid = ? AND key = ?"));
    q.addBindValue(methodId);
    q.addBindValue(owner.bare());
    q.addBindValue(keyId);
    return q.exec();
}

QSet<QString> PsiOmemoStorage::legacyEnabledJids() const
{
    QSet<QString> out;
    QSqlQuery     q(d->database());
    if (q.exec(QStringLiteral("SELECT jid FROM enabled_buddies"))) {
        while (q.next())
            out.insert(bareJid(q.value(0).toString()));
    }
    return out;
}

QSet<QString> PsiOmemoStorage::legacyDisabledJids() const
{
    QSet<QString> out;
    QSqlQuery     q(d->database());
    if (q.exec(QStringLiteral("SELECT jid FROM disabled_buddies"))) {
        while (q.next())
            out.insert(bareJid(q.value(0).toString()));
    }
    return out;
}

bool PsiOmemoStorage::setLegacyEnabled(const QString &jid, bool enabled)
{
    QSqlQuery q(d->database());
    q.prepare(enabled ? QStringLiteral("INSERT OR REPLACE INTO enabled_buddies (jid) VALUES (?)")
                      : QStringLiteral("DELETE FROM enabled_buddies WHERE jid = ?"));
    q.addBindValue(bareJid(jid));
    return q.exec();
}

bool PsiOmemoStorage::setLegacyDisabled(const QString &jid, bool disabled)
{
    QSqlQuery q(d->database());
    q.prepare(disabled ? QStringLiteral("INSERT OR REPLACE INTO disabled_buddies (jid) VALUES (?)")
                       : QStringLiteral("DELETE FROM disabled_buddies WHERE jid = ?"));
    q.addBindValue(bareJid(jid));
    return q.exec();
}

std::optional<PsiOmemoStorage::LegacyTrust> PsiOmemoStorage::legacyTrust(const QString &jid, uint32_t deviceId) const
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("SELECT trust FROM devices WHERE jid = ? AND device_id = ?"));
    q.addBindValue(bareJid(jid));
    q.addBindValue(deviceId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    const int value = q.value(0).toInt();
    if (value < 0 || value > 2)
        return std::nullopt;
    return static_cast<LegacyTrust>(value);
}

bool PsiOmemoStorage::setLegacyTrust(const QString &jid, uint32_t deviceId, LegacyTrust trust)
{
    QSqlQuery q(d->database());
    q.prepare(QStringLiteral("UPDATE devices SET trust = ? WHERE jid = ? AND device_id = ?"));
    q.addBindValue(static_cast<int>(trust));
    q.addBindValue(bareJid(jid));
    q.addBindValue(deviceId);
    return q.exec();
}

#endif // IRIS_ENABLE_OMEMO
