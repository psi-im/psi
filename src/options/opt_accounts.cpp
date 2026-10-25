#include "opt_accounts.h"
#include "accountmanagedlg.h"
#include "profiles.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psioptions.h"
#include "serverless.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <iris/xmpp_linklocalservice.h>

static const QString accSingleOption = "options.ui.account.single";

OptionsTabAccounts::OptionsTabAccounts(QObject *parent) :
    MetaOptionsTab(parent, "accounts", "", tr("Accounts"), tr("Manage accounts"), "psi/account")
{
    addTab(new OptionsTabXmppAccounts(this));
    addTab(new OptionsTabServerless(this));
}
OptionsTabXmppAccounts::OptionsTabXmppAccounts(QObject *parent) :
    OptionsTab(parent, "accounts-xmpp", "", tr("XMPP"), tr("Server accounts"))
{
}
QWidget *OptionsTabXmppAccounts::widget()
{
    if (w_)
        return w_;
    auto *manager = new AccountManageDlg(psi_);
    w_            = manager;
    auto *options = PsiOptions::instance();
    manager->enableElements(!options->getOption(accSingleOption).toBool());
    connect(options, &PsiOptions::optionChanged, manager, [manager](const QString &option) {
        if (option == accSingleOption)
            manager->enableElements(!PsiOptions::instance()->getOption(accSingleOption).toBool());
    });
    return w_;
}
void OptionsTabXmppAccounts::setData(PsiCon *psi, QWidget *) { psi_ = psi; }

OptionsTabServerless::OptionsTabServerless(QObject *parent) :
    OptionsTab(parent, "accounts-serverless", "", tr("Serverless"), tr("Chat on the local network without a server"))
{
}
void        OptionsTabServerless::setData(PsiCon *psi, QWidget *) { psi_ = psi; }
PsiAccount *OptionsTabServerless::account() const
{
    if (psi_)
        for (auto *account : psi_->contactList()->accounts())
            if (account->isServerless())
                return account;
    return nullptr;
}
QWidget *OptionsTabServerless::widget()
{
    if (w_)
        return w_;
    w_           = new QWidget;
    auto *layout = new QVBoxLayout(w_);
    auto *description
        = new QLabel(tr("Discover and chat with people on the selected local network using Bonjour / XEP-0174. "
                        "Messages currently use unencrypted, unauthenticated connections. "
                        "Enable this only on a network you trust."),
                     w_);
    description->setWordWrap(true);
    layout->addWidget(description);
    enabled_ = new QCheckBox(tr("Enable serverless messaging"), w_);
    enabled_->setObjectName(QStringLiteral("serverlessEnabled"));
    enabled_->setEnabled(XMPP::LinkLocalService::isSupported());
    if (!enabled_->isEnabled())
        description->setText(tr("Serverless messaging is unavailable: this Iris build does not include JDNS support."));
    layout->addWidget(enabled_);
    auto *group = new QGroupBox(tr("Local network account"), w_);
    auto *form  = new QFormLayout(group);
    name_       = new QLineEdit(group);
    name_->setObjectName(QStringLiteral("serverlessName"));
    name_->setMaxLength(200);
    form->addRow(tr("Display name:"), name_);
    identity_ = new QLabel(group);
    identity_->setObjectName(QStringLiteral("serverlessIdentity"));
    identity_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Local address:"), identity_);
    auto *interfaceRow    = new QWidget(group);
    auto *interfaceLayout = new QHBoxLayout(interfaceRow);
    interfaceLayout->setContentsMargins(0, 0, 0, 0);
    interface_ = new QComboBox(interfaceRow);
    interface_->setObjectName(QStringLiteral("serverlessInterface"));
    interfaceLayout->addWidget(interface_, 1);
    auto *refresh = new QPushButton(tr("Refresh"), interfaceRow);
    interfaceLayout->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &OptionsTabServerless::refreshInterfaces);
    form->addRow(tr("Network interface:"), interfaceRow);
    port_ = new QSpinBox(group);
    port_->setObjectName(QStringLiteral("serverlessPort"));
    port_->setRange(0, 65535);
    port_->setSpecialValueText(tr("Automatic"));
    port_->setToolTip(tr("Leave Automatic unless a firewall requires a fixed TCP listening port."));
    form->addRow(tr("Listening port:"), port_);
    automatic_ = new QCheckBox(tr("Connect automatically when Psi starts"), group);
    form->addRow(automatic_);
    status_ = new QLabel(group);
    status_->setObjectName(QStringLiteral("serverlessStatus"));
    status_->setWordWrap(true);
    form->addRow(tr("Status:"), status_);
    connect(enabled_, &QCheckBox::toggled, group, &QGroupBox::setEnabled);
    group->setEnabled(enabled_->isChecked());
    layout->addWidget(group);
    layout->addStretch();
    refreshInterfaces();
    if (auto *current = account())
        connect(current, &PsiAccount::updatedActivity, this, &OptionsTabServerless::updateStatus);
    return w_;
}
void OptionsTabServerless::refreshInterfaces()
{
    const auto           selected = interface_->currentData().toString();
    const QSignalBlocker block(interface_);
    interface_->clear();
    interface_->addItem(tr("Automatic (active local network)"), QString());
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        const auto address = Serverless::addressForInterface(interface);
        if (address.isNull())
            continue;
        interface_->addItem(tr("%1 (%2)").arg(interface.humanReadableName(), address.toString()), interface.name());
    }
    int index = interface_->findData(selected);
    if (index < 0) {
        interface_->addItem(tr("%1 (unavailable)").arg(selected), selected);
        index = interface_->count() - 1;
    }
    interface_->setCurrentIndex(index);
}
void OptionsTabServerless::updateStatus()
{
    if (!w_)
        return;
    auto *current = account();
    identity_->setText(current ? current->jid().full() : tr("Assigned when enabled"));
    if (!current || !current->enabled())
        status_->setText(tr("Disabled"));
    else if (!current->currentConnectionError().isEmpty())
        status_->setText(current->currentConnectionError());
    else if (current->isConnected())
        status_->setText(tr("Online on the local network"));
    else if (current->isActive())
        status_->setText(tr("Announcing on the local network…"));
    else
        status_->setText(tr("Offline"));
}
void OptionsTabServerless::restoreOptions()
{
    if (!w_)
        return;
    const auto *current = account();
    UserAccount config;
    if (current)
        config = current->accountOptions();
    enabled_->setChecked(current && config.opt_enabled);
    name_->setText(current ? config.serverlessName : Serverless::defaultName());
    automatic_->setChecked(current ? config.opt_auto : true);
    port_->setValue(current ? config.serverlessPort : 0);
    int index = interface_->findData(config.serverlessInterface);
    if (index < 0) {
        interface_->addItem(tr("%1 (unavailable)").arg(config.serverlessInterface), config.serverlessInterface);
        index = interface_->count() - 1;
    }
    interface_->setCurrentIndex(index);
    updateStatus();
}
void OptionsTabServerless::applyOptions()
{
    if (!w_ || !psi_)
        return;
    auto *current = account();
    if (!current && !enabled_->isChecked())
        return;
    UserAccount config;
    if (current)
        config = current->accountOptions();
    else {
        config.type                  = UserAccount::Type::Serverless;
        config.name                  = tr("Local network");
        config.jid                   = Serverless::defaultIdentity(config.id);
        config.ignore_global_actions = false;
        config.opt_autoSameStatus    = false;
        config.opt_sm = config.opt_keepAlive = config.opt_compress = false;
        config.ssl                                                 = UserAccount::TLS_No;
    }
    const bool enableNow  = enabled_->isChecked() && (!current || !current->enabled());
    config.opt_enabled    = enabled_->isChecked();
    config.opt_auto       = automatic_->isChecked();
    config.serverlessName = name_->text().trimmed();
    if (config.serverlessName.isEmpty())
        config.serverlessName = Serverless::defaultName();
    config.serverlessInterface = interface_->currentData().toString();
    config.serverlessPort      = quint16(port_->value());
    if (current)
        current->setUserAccount(config);
    else {
        psi_->contactList()->createAccount(config);
        current = account();
        connect(current, &PsiAccount::updatedActivity, this, &OptionsTabServerless::updateStatus);
    }
    if (enableNow)
        current->setStatus(XMPP::Status::Online);
    updateStatus();
}
