/*
 * opt_security.cpp - encryption and key-management options page
 * Copyright (C) 2026 Sergey Ilinykh
 * Copyright (C) 2018 Vyacheslav Karpukhin
 * Copyright (C) 2020 Boris Pek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "opt_security.h"

#include "iris/xmpp_omemo.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psiencryptioncontroller.h"
#include "psioptions.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr auto OmemoAlwaysEnabledOption    = "plugins.options.omemo.always-enabled";
constexpr auto OmemoEnabledByDefaultOption = "plugins.options.omemo.enabled-by-default";
constexpr auto DeviceOwnerRole             = Qt::UserRole;
constexpr auto DeviceIdentityKeyRole       = Qt::UserRole + 1;
constexpr auto OwnDeviceIdRole             = Qt::UserRole;
constexpr auto OwnDeviceProtocolRole       = Qt::UserRole + 1;

QString formatFingerprint(const QByteArray &fingerprint)
{
    const auto hex = fingerprint.toHex();
    QString    out;
    out.reserve(hex.size() + hex.size() / 8);
    for (qsizetype i = 0; i < hex.size(); i += 8) {
        if (!out.isEmpty())
            out += QLatin1Char(' ');
        out += QString::fromLatin1(hex.mid(i, 8));
    }
    return out;
}

QString trustText(XMPP::EncryptionTrustLevel level)
{
    switch (level) {
    case XMPP::EncryptionTrustLevel::Distrusted:
        return QObject::tr("Distrusted");
    case XMPP::EncryptionTrustLevel::Undecided:
    case XMPP::EncryptionTrustLevel::AutomaticallyTrusted:
        return QObject::tr("Undecided");
    default:
        return QObject::tr("Trusted");
    }
}

#ifdef IRIS_ENABLE_OMEMO
QString omemoProtocolText(XMPP::OmemoProtocol protocol)
{
    return protocol == XMPP::OmemoProtocol::Legacy ? QObject::tr("Legacy") : QObject::tr("OMEMO 2");
}
#endif
} // namespace

OptionsTabSecurity::OptionsTabSecurity(QObject *parent) :
    OptionsTab(parent, "security", "", tr("Security"), tr("Encryption methods and key management"), "psi/cryptoYes")
{
}

QWidget *OptionsTabSecurity::widget()
{
    if (w_)
        return nullptr;

    w_           = new QWidget;
    auto *layout = new QVBoxLayout(w_);

    auto *accountLayout = new QHBoxLayout;
    accountLayout->addWidget(new QLabel(tr("Account:"), w_));
    accounts_ = new QComboBox(w_);
    accountLayout->addWidget(accounts_, 1);
    layout->addLayout(accountLayout);

    accountStatus_ = new QLabel(w_);
    accountStatus_->setWordWrap(true);
    layout->addWidget(accountStatus_);

    auto *methodsGroup  = new QGroupBox(tr("Available encryption methods"), w_);
    auto *methodsLayout = new QVBoxLayout(methodsGroup);
    methods_            = new QTreeWidget(methodsGroup);
    methods_->setColumnCount(2);
    methods_->setHeaderLabels({ tr("Method"), tr("Status") });
    methods_->setRootIsDecorated(false);
    methods_->setSelectionMode(QAbstractItemView::NoSelection);
    methods_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    methods_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    methodsLayout->addWidget(methods_);
    layout->addWidget(methodsGroup);

    auto *tabs = new QTabWidget(w_);

    auto *knownKeysPage   = new QWidget(tabs);
    auto *knownKeysLayout = new QVBoxLayout(knownKeysPage);
    auto *knownKeysHint   = new QLabel(
        tr("Review every identity before trusting it. Trusted devices can receive encrypted messages; distrusted "
               "devices are excluded."),
        knownKeysPage);
    knownKeysHint->setWordWrap(true);
    knownKeysLayout->addWidget(knownKeysHint);
    knownKeys_ = new QTreeWidget(knownKeysPage);
    knownKeys_->setColumnCount(5);
    knownKeys_->setHeaderLabels({ tr("User"), tr("Device ID"), tr("Profile"), tr("Trust"), tr("Fingerprint") });
    knownKeys_->setRootIsDecorated(false);
    knownKeys_->setSelectionBehavior(QAbstractItemView::SelectRows);
    knownKeys_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    knownKeys_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    knownKeys_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    knownKeys_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    knownKeys_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    knownKeys_->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    knownKeysLayout->addWidget(knownKeys_, 1);
    auto *knownKeyActions = new QHBoxLayout;
    trustKey_             = new QPushButton(tr("Trust"), knownKeysPage);
    distrustKey_          = new QPushButton(tr("Do not trust"), knownKeysPage);
    auto *copyKey         = new QPushButton(tr("Copy fingerprint"), knownKeysPage);
    knownKeyActions->addWidget(trustKey_);
    knownKeyActions->addWidget(distrustKey_);
    knownKeyActions->addStretch();
    knownKeyActions->addWidget(copyKey);
    knownKeysLayout->addLayout(knownKeyActions);
    tabs->addTab(knownKeysPage, tr("Known keys"));

    auto *ownDevicesPage   = new QWidget(tabs);
    auto *ownDevicesLayout = new QVBoxLayout(ownDevicesPage);
    auto *identityGroup    = new QGroupBox(tr("Current device"), ownDevicesPage);
    auto *identityLayout   = new QFormLayout(identityGroup);
    ownDeviceId_           = new QLabel(identityGroup);
    ownFingerprint_        = new QLabel(identityGroup);
    ownFingerprint_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ownFingerprint_->setWordWrap(true);
    ownDeviceLabel_ = new QLineEdit(identityGroup);
    identityLayout->addRow(tr("Device ID:"), ownDeviceId_);
    identityLayout->addRow(tr("Device name:"), ownDeviceLabel_);
    identityLayout->addRow(tr("Fingerprint:"), ownFingerprint_);
    setUpOmemo_ = new QPushButton(identityGroup);
    identityLayout->addRow(setUpOmemo_);
    ownDevicesLayout->addWidget(identityGroup);

    auto *otherDevicesHint = new QLabel(
        tr("Other devices registered for this account. Retiring one removes it from the published OMEMO device lists, "
           "so it will no longer receive encrypted messages."),
        ownDevicesPage);
    otherDevicesHint->setWordWrap(true);
    ownDevicesLayout->addWidget(otherDevicesHint);
    ownDevices_ = new QTreeWidget(ownDevicesPage);
    ownDevices_->setColumnCount(4);
    ownDevices_->setHeaderLabels({ tr("Device"), tr("Device ID"), tr("Profile"), tr("Fingerprint") });
    ownDevices_->setRootIsDecorated(false);
    ownDevices_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ownDevices_->setSelectionMode(QAbstractItemView::SingleSelection);
    ownDevices_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ownDevices_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ownDevices_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ownDevices_->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    ownDevicesLayout->addWidget(ownDevices_, 1);
    auto *ownDeviceActions = new QHBoxLayout;
    retireOwnDevice_       = new QPushButton(tr("Retire device"), ownDevicesPage);
    sanitizePep_           = new QPushButton(tr("Sanitize OMEMO PEP"), ownDevicesPage);
    ownDeviceActions->addWidget(retireOwnDevice_);
    ownDeviceActions->addWidget(sanitizePep_);
    ownDeviceActions->addStretch();
    ownDevicesLayout->addLayout(ownDeviceActions);
    tabs->addTab(ownDevicesPage, tr("Own keys"));

    auto *configurationPage   = new QWidget(tabs);
    auto *configurationLayout = new QVBoxLayout(configurationPage);
    auto *policyGroup         = new QGroupBox(tr("OMEMO encryption policy"), configurationPage);
    auto *policyLayout        = new QVBoxLayout(policyGroup);
    alwaysEnabled_            = new QRadioButton(tr("Always enabled"), policyGroup);
    enabledByDefault_         = new QRadioButton(tr("Enabled by default"), policyGroup);
    disabledByDefault_        = new QRadioButton(tr("Disabled by default"), policyGroup);
    auto *policyButtons       = new QButtonGroup(policyGroup);
    policyButtons->addButton(alwaysEnabled_);
    policyButtons->addButton(enabledByDefault_);
    policyButtons->addButton(disabledByDefault_);
    policyLayout->addWidget(alwaysEnabled_);
    policyLayout->addWidget(enabledByDefault_);
    policyLayout->addWidget(disabledByDefault_);
    configurationLayout->addWidget(policyGroup);
    auto *policyHint = new QLabel(
        tr("This is the global default for new chats. A method selected in an individual chat overrides it for that "
           "contact. New OMEMO identities always require an explicit trust decision."),
        configurationPage);
    policyHint->setWordWrap(true);
    configurationLayout->addWidget(policyHint);
    configurationLayout->addStretch();
    tabs->addTab(configurationPage, tr("Configuration"));

    layout->addWidget(tabs, 1);

    connect(accounts_, qOverload<int>(&QComboBox::currentIndexChanged), w_, [this](int) {
        updateControllerConnections();
        refresh();
    });
    connect(trustKey_, &QPushButton::clicked, w_, [this]() { setTrustForSelection(true); });
    connect(distrustKey_, &QPushButton::clicked, w_, [this]() { setTrustForSelection(false); });
    connect(knownKeys_, &QTreeWidget::itemSelectionChanged, w_, [this]() {
        const bool selected = !knownKeys_->selectedItems().isEmpty();
        trustKey_->setEnabled(selected);
        distrustKey_->setEnabled(selected);
    });
    connect(ownDevices_, &QTreeWidget::itemSelectionChanged, w_,
            [this]() { retireOwnDevice_->setEnabled(!ownDevices_->selectedItems().isEmpty()); });
    connect(copyKey, &QPushButton::clicked, w_, [this]() {
        QStringList fingerprints;
        for (const auto *item : knownKeys_->selectedItems())
            fingerprints.append(item->text(4));
        QApplication::clipboard()->setText(fingerprints.join(QLatin1Char('\n')));
    });
    connect(setUpOmemo_, &QPushButton::clicked, w_, [this]() {
#ifdef IRIS_ENABLE_OMEMO
        auto *account = currentAccount();
        if (!account)
            return;
        auto *controller = account->encryptionController();
        if (!controller || !controller->omemoEncryption())
            return;
        accountStatus_->setText(tr("Publishing OMEMO keys…"));
        controller->setUpOmemo(ownDeviceLabel_->text());
#endif
    });
    connect(sanitizePep_, &QPushButton::clicked, w_, [this]() {
#ifdef IRIS_ENABLE_OMEMO
        auto *account    = currentAccount();
        auto *controller = account ? account->encryptionController() : nullptr;
        auto *omemo      = controller ? controller->omemoEncryption() : nullptr;
        if (!omemo)
            return;

        const auto answer = QMessageBox::warning(
            w_, tr("Sanitize OMEMO PEP"),
            tr("Psi will verify this account's OMEMO 2 device list and bundles. A broken bundle for this device "
               "will be republished. A broken bundle belonging to another device will be retracted and that "
               "device will stop receiving new encrypted messages."),
            QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
            return;

        accountStatus_->setText(tr("Checking OMEMO PEP data…"));
        auto *job = omemo->sanitizeOwnPep();
        const auto finished = [this, job]() {
            if (!job->success())
                QMessageBox::warning(w_, tr("OMEMO"), job->errorString());
            else
                accountStatus_->setText(tr("OMEMO PEP data has been sanitized."));
            refreshOmemo();
            job->deleteLater();
        };
        if (job->isFinished())
            finished();
        else
            connect(job, &XMPP::EncryptionJob::finished, w_, finished);
#endif
    });
    connect(retireOwnDevice_, &QPushButton::clicked, w_, [this]() {
#ifdef IRIS_ENABLE_OMEMO
        auto *account    = currentAccount();
        auto *controller = account ? account->encryptionController() : nullptr;
        auto *omemo      = controller ? controller->omemoEncryption() : nullptr;
        if (!omemo)
            return;

        const auto selected = ownDevices_->selectedItems();
        if (selected.isEmpty())
            return;
        const auto answer = QMessageBox::warning(
            w_, tr("Retire OMEMO device"),
            tr("The selected device will stop receiving new encrypted messages. Its published bundle will remain on "
               "the server."),
            QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
            return;

        const auto deviceId = selected.constFirst()->data(0, OwnDeviceIdRole).toUInt();
        const auto protocol = static_cast<XMPP::OmemoProtocol>(
            selected.constFirst()->data(0, OwnDeviceProtocolRole).toUInt());
        auto *job = omemo->retireOwnDevice(deviceId, protocol);
        const auto finished = [this, job]() {
            if (!job->success())
                QMessageBox::warning(w_, tr("OMEMO"), job->errorString());
            else
                refreshOmemo();
            job->deleteLater();
        };
        if (job->isFinished())
            finished();
        else
            connect(job, &XMPP::EncryptionJob::finished, w_, finished);
#endif
    });

    refreshAccounts();
    return w_;
}

void OptionsTabSecurity::setData(PsiCon *psi, QWidget *)
{
    psi_ = psi;
    if (w_)
        refreshAccounts();
}

void OptionsTabSecurity::applyOptions()
{
    if (!w_)
        return;

    PsiOptions::instance()->setOption(OmemoAlwaysEnabledOption, alwaysEnabled_->isChecked());
    PsiOptions::instance()->setOption(OmemoEnabledByDefaultOption, enabledByDefault_->isChecked());
}

void OptionsTabSecurity::restoreOptions()
{
    if (!w_)
        return;

    const bool alwaysEnabled    = PsiOptions::instance()->getOption(OmemoAlwaysEnabledOption, false).toBool();
    const bool enabledByDefault = PsiOptions::instance()->getOption(OmemoEnabledByDefaultOption, false).toBool();
    alwaysEnabled_->setChecked(alwaysEnabled);
    enabledByDefault_->setChecked(!alwaysEnabled && enabledByDefault);
    disabledByDefault_->setChecked(!alwaysEnabled && !enabledByDefault);
}

PsiAccount *OptionsTabSecurity::currentAccount() const
{
    if (!psi_ || !accounts_)
        return nullptr;
    return psi_->contactList()->getAccount(accounts_->currentData().toString());
}

void OptionsTabSecurity::refreshAccounts()
{
    if (!accounts_ || !psi_)
        return;

    const QString currentId = accounts_->currentData().toString();
    accounts_->blockSignals(true);
    accounts_->clear();
    for (auto *account : psi_->contactList()->enabledAccounts())
        accounts_->addItem(account->nameWithJid(), account->id());
    const int previousIndex = accounts_->findData(currentId);
    accounts_->setCurrentIndex(previousIndex >= 0 ? previousIndex : (accounts_->count() ? 0 : -1));
    accounts_->blockSignals(false);

    updateControllerConnections();
    refresh();
}

void OptionsTabSecurity::updateControllerConnections()
{
    disconnect(methodsConnection_);
    disconnect(stateConnection_);
    disconnect(errorConnection_);

    auto *account    = currentAccount();
    auto *controller = account ? account->encryptionController() : nullptr;
    if (!controller || !w_)
        return;

    methodsConnection_ = connect(controller, &PsiEncryptionController::methodsChanged, w_, [this]() {
        if (!changingTrust_)
            refresh();
    });
    stateConnection_
        = connect(controller, &PsiEncryptionController::methodStateChanged, w_, [this](const QString &) {
              if (!changingTrust_)
                  refresh();
          });
    errorConnection_ = connect(controller, &PsiEncryptionController::encryptionError, w_,
                               [this](const XMPP::Jid &, const QString &message) { accountStatus_->setText(message); });
}

void OptionsTabSecurity::refresh()
{
    if (!w_)
        return;
    refreshMethods();
    refreshOmemo();
}

void OptionsTabSecurity::refreshMethods()
{
    methods_->clear();
    auto *account    = currentAccount();
    auto *controller = account ? account->encryptionController() : nullptr;
    if (!controller) {
        accountStatus_->setText(tr("Select an account to manage its encryption keys."));
        return;
    }

    for (const auto &method : controller->methods()) {
        auto *item = new QTreeWidgetItem(methods_);
        item->setText(0, method.name);
        item->setIcon(0, method.icon);
#ifdef IRIS_ENABLE_OMEMO
        if (method.id == XMPP::OmemoEncryption::methodId() && controller->omemoEncryption())
            item->setText(1, controller->omemoEncryption()->isReady() ? tr("Ready") : tr("Not set up"));
        else
#endif
            item->setText(1, tr("Available"));
    }
    if (methods_->topLevelItemCount() == 0)
        accountStatus_->setText(tr("No encryption methods are available for this account."));
    else
        accountStatus_->setText(tr("Manage keys and trust decisions for the selected account."));
}

void OptionsTabSecurity::refreshOmemo()
{
    ownDeviceId_->clear();
    ownFingerprint_->clear();
    ownDeviceLabel_->clear();
    knownKeys_->clear();
    ownDevices_->clear();
    trustKey_->setEnabled(false);
    distrustKey_->setEnabled(false);
    retireOwnDevice_->setEnabled(false);
    sanitizePep_->setEnabled(false);

#ifdef IRIS_ENABLE_OMEMO
    auto *account    = currentAccount();
    auto *controller = account ? account->encryptionController() : nullptr;
    auto *omemo      = controller ? controller->omemoEncryption() : nullptr;
    if (!omemo) {
        ownDeviceId_->setText(tr("OMEMO is unavailable in this build."));
        setUpOmemo_->setEnabled(false);
        setUpOmemo_->setText(tr("Set up OMEMO"));
        ownDeviceLabel_->setEnabled(false);
        return;
    }

    ownDeviceId_->setText(omemo->isReady() ? QString::number(omemo->ownDeviceId()) : tr("Not set up"));
    ownFingerprint_->setText(formatFingerprint(omemo->ownIdentityKey()));
    ownDeviceLabel_->setEnabled(true);
    ownDeviceLabel_->setText(omemo->isReady() ? omemo->ownDeviceLabel() : QStringLiteral("Psi"));
    setUpOmemo_->setEnabled(true);
    setUpOmemo_->setText(omemo->isReady() ? tr("Save name and republish") : tr("Set up OMEMO"));
    sanitizePep_->setEnabled(omemo->isReady());

    for (const auto &device : omemo->devices()) {
        if (device.identityKey.isEmpty())
            continue;
        auto *item = new QTreeWidgetItem(knownKeys_);
        item->setText(0, device.owner.bare());
        item->setText(1, QString::number(device.id));
        item->setText(2, omemoProtocolText(device.protocol));
        item->setText(3, trustText(device.trust));
        item->setText(4, formatFingerprint(device.identityKey));
        item->setData(0, DeviceOwnerRole, device.owner.bare());
        item->setData(0, DeviceIdentityKeyRole, device.identityKey);
    }

    for (const auto &device : omemo->devices(account->jid().bare())) {
        if (!device.active || device.id == omemo->ownDeviceId())
            continue;
        auto *item = new QTreeWidgetItem(ownDevices_);
        item->setText(0, device.label.isEmpty() ? tr("Unnamed device") : device.label);
        item->setText(1, QString::number(device.id));
        item->setText(2, omemoProtocolText(device.protocol));
        item->setText(3, formatFingerprint(device.identityKey));
        item->setData(0, OwnDeviceIdRole, device.id);
        item->setData(0, OwnDeviceProtocolRole, static_cast<quint32>(device.protocol));
    }
#else
    ownDeviceId_->setText(tr("OMEMO support is not compiled in."));
    setUpOmemo_->setEnabled(false);
    setUpOmemo_->setText(tr("Set up OMEMO"));
    ownDeviceLabel_->setEnabled(false);
#endif
}

void OptionsTabSecurity::setTrustForSelection(bool trusted)
{
#ifdef IRIS_ENABLE_OMEMO
    auto *account    = currentAccount();
    auto *controller = account ? account->encryptionController() : nullptr;
    auto *omemo      = controller ? controller->omemoEncryption() : nullptr;
    if (!omemo)
        return;

    const auto level = trusted ? XMPP::EncryptionTrustLevel::ManuallyTrusted : XMPP::EncryptionTrustLevel::Distrusted;
    QList<QPair<XMPP::Jid, QByteArray>> keys;
    for (const auto *item : knownKeys_->selectedItems())
        keys.append({ XMPP::Jid(item->data(0, DeviceOwnerRole).toString()),
                      item->data(0, DeviceIdentityKeyRole).toByteArray() });

    changingTrust_ = true;
    for (const auto &[owner, identityKey] : keys) {
        if (!omemo->setTrustLevel(owner, identityKey, level))
            QMessageBox::warning(w_, tr("OMEMO"), tr("Could not save the trust decision."));
    }
    changingTrust_ = false;
    refreshOmemo();
#else
    Q_UNUSED(trusted);
#endif
}
