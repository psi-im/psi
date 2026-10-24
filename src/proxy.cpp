/*
 * proxy.cpp - classes for handling proxy profiles
 * Copyright (C) 2003  Justin Karneges
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

#include "proxy.h"

#include "common.h"
#include "iconwidget.h"
#include "iris/netnames.h"
#include "iris/remotenameprovider.h"
#include "iris/xmpp_xmlcommon.h"
#include "psioptions.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDomElement>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPointer>
#include <QPushButton>
#include <QUrl>

#include <memory>

#define PASSWORDKEY "Cae1voo:ea}fae2OCai|f1il"

//----------------------------------------------------------------------------
// ProxySettings
//----------------------------------------------------------------------------
ProxySettings::ProxySettings()
{
    port    = 0;
    useAuth = false;
    dnsMode = QStringLiteral("off");
    dohUrl  = QStringLiteral("https://cloudflare-dns.com/dns-query");
    dnsHost = QStringLiteral("1.1.1.1");
    dnsPort = 53;
}

void ProxySettings::toOptions(OptionsTree *o, QString base) const
{
    o->setOption(base + ".host", host);
    o->setOption(base + ".port", port);
    o->setOption(base + ".url", url);
    o->setOption(base + ".useAuth", useAuth);
    o->setOption(base + ".user", user);
    o->setOption(base + ".pass", encodePassword(pass, PASSWORDKEY));
    o->setOption(base + ".dnsMode", dnsMode);
    o->setOption(base + ".dohUrl", dohUrl);
    o->setOption(base + ".dnsHost", dnsHost);
    o->setOption(base + ".dnsPort", dnsPort);
}

void ProxySettings::fromOptions(OptionsTree *o, QString base)
{
    host    = o->getOption(base + ".host").toString();
    port    = o->getOption(base + ".port").toInt();
    url     = o->getOption(base + ".url").toString();
    useAuth = o->getOption(base + ".useAuth").toBool();
    user    = o->getOption(base + ".user").toString();
    pass    = decodePassword(o->getOption(base + ".pass").toString(), PASSWORDKEY);
    // Old profiles have no DNS server; new ones start with DoH (see ProxyDlg).
    dnsMode = o->getOption(base + ".dnsMode").toString();
    if (dnsMode != QLatin1String("doh") && dnsMode != QLatin1String("tcp"))
        dnsMode = QStringLiteral("off");
    dohUrl  = o->getOption(base + ".dohUrl", QStringLiteral("https://cloudflare-dns.com/dns-query")).toString();
    dnsHost = o->getOption(base + ".dnsHost", QStringLiteral("1.1.1.1")).toString();
    dnsPort = o->getOption(base + ".dnsPort", 53).toInt();
}

QDomElement ProxySettings::toXml(QDomDocument *doc) const
{
    QDomElement e = doc->createElement("proxySettings");
    e.appendChild(XMLHelper::textTag(*doc, "host", host));
    e.appendChild(XMLHelper::textTag(*doc, "port", QString::number(port)));
    e.appendChild(XMLHelper::textTag(*doc, "url", url));
    e.appendChild(XMLHelper::textTag(*doc, "useAuth", useAuth));
    e.appendChild(XMLHelper::textTag(*doc, "user", user));
    e.appendChild(XMLHelper::textTag(*doc, "pass", pass));
    e.appendChild(XMLHelper::textTag(*doc, "dnsMode", dnsMode));
    e.appendChild(XMLHelper::textTag(*doc, "dohUrl", dohUrl));
    e.appendChild(XMLHelper::textTag(*doc, "dnsHost", dnsHost));
    e.appendChild(XMLHelper::textTag(*doc, "dnsPort", QString::number(dnsPort)));
    return e;
}

bool ProxySettings::fromXml(const QDomElement &e)
{
    host    = XMLHelper::subTagText(e, "host");
    port    = XMLHelper::subTagText(e, "port").toInt();
    url     = XMLHelper::subTagText(e, "url");
    useAuth = XMLHelper::subTagText(e, "useAuth") == "true";
    user    = XMLHelper::subTagText(e, "user");
    pass    = XMLHelper::subTagText(e, "pass");
    dnsMode = XMLHelper::subTagText(e, "dnsMode");
    if (dnsMode != QLatin1String("doh") && dnsMode != QLatin1String("tcp"))
        dnsMode = QStringLiteral("off");
    dohUrl = XMLHelper::subTagText(e, "dohUrl");
    if (dohUrl.isEmpty())
        dohUrl = QStringLiteral("https://cloudflare-dns.com/dns-query");
    dnsHost = XMLHelper::subTagText(e, "dnsHost");
    if (dnsHost.isEmpty())
        dnsHost = QStringLiteral("1.1.1.1");
    dnsPort = XMLHelper::subTagText(e, "dnsPort").toInt();
    if (dnsPort <= 0 || dnsPort > 65535)
        dnsPort = 53;
    return true;
}

QUrl ProxySettings::dnsServer() const
{
    if (dnsMode == QLatin1String("doh"))
        return QUrl(dohUrl.trimmed());
    if (dnsMode != QLatin1String("tcp"))
        return {};
    QUrl url;
    url.setScheme(QStringLiteral("tcp"));
    url.setHost(dnsHost.trimmed());
    if (dnsPort != 53)
        url.setPort(dnsPort);
    return url;
}

//----------------------------------------------------------------------------
// ProxyDlg
//----------------------------------------------------------------------------
class ProxyDlg::Private : public QObject {
    Q_OBJECT
public:
    enum Data {
        HostRole    = Qt::UserRole + 1,
        PortRole    = Qt::UserRole + 2,
        UrlRole     = Qt::UserRole + 3,
        AuthRole    = Qt::UserRole + 4,
        UserRole    = Qt::UserRole + 5,
        PassRole    = Qt::UserRole + 6,
        IdRole      = Qt::UserRole + 7,
        TypeRole    = Qt::UserRole + 8,
        DnsModeRole = Qt::UserRole + 9,
        DohUrlRole  = Qt::UserRole + 10,
        DnsHostRole = Qt::UserRole + 11,
        DnsPortRole = Qt::UserRole + 12
    };

    ProxyDlg     *q;
    ProxyItemList list;

    QPointer<XMPP::ServiceResolver> dnsTest; // the running "Test DNS..." lookup

    Private(ProxyDlg *dialog) : QObject(dialog), q(dialog)
    {
        Q_ASSERT(q);

        connect(q->ui_.lbx_proxy, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this,
                SLOT(currentItemChanged(QListWidgetItem *, QListWidgetItem *)));

        connect(q->ui_.pb_new, SIGNAL(clicked()), SLOT(addProxy()));
        connect(q->ui_.pb_remove, SIGNAL(clicked()), SLOT(removeProxy()));

        connect(q->ui_.buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(accept()));
        connect(q->ui_.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), q, SLOT(reject()));
        q->ui_.buttonBox->button(QDialogButtonBox::Save)->setDefault(true);

        connect(q->ui_.lbx_proxy->itemDelegate(), SIGNAL(closeEditor(QWidget *, QAbstractItemDelegate::EndEditHint)),
                SLOT(closeEditor(QWidget *, QAbstractItemDelegate::EndEditHint)));

        connect(q->ui_.le_host, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_port, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_url, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_user, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_pass, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.gr_auth, SIGNAL(toggled(bool)), SLOT(updateCurrentItem()));
        connect(q->ui_.cb_type, SIGNAL(activated(int)), SLOT(updateCurrentItem()));
        connect(q->ui_.cb_dns_mode, SIGNAL(activated(int)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_doh_url, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_dns_host, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.le_dns_port, SIGNAL(textEdited(const QString &)), SLOT(updateCurrentItem()));
        connect(q->ui_.pb_test_dns, SIGNAL(clicked()), SLOT(testDns()));
    }

public slots:
    void loadProxies(const QString &currentProxy)
    {
        q->ui_.lbx_proxy->clear();

        QListWidgetItem *firstItem   = nullptr;
        QListWidgetItem *currentItem = nullptr;
        for (const ProxyItem &i : std::as_const(list)) {
            QListWidgetItem *item = new QListWidgetItem(i.name);
            addItem(item);

            item->setData(IdRole, QVariant(i.id));
            item->setData(TypeRole, QVariant(i.type));
            item->setData(HostRole, QVariant(i.settings.host));
            item->setData(PortRole, QVariant(i.settings.port));
            item->setData(UserRole, QVariant(i.settings.user));
            item->setData(PassRole, QVariant(i.settings.pass));
            item->setData(AuthRole, QVariant(i.settings.useAuth));
            item->setData(UrlRole, QVariant(i.settings.url));
            item->setData(DnsModeRole, i.settings.dnsMode);
            item->setData(DohUrlRole, i.settings.dohUrl);
            item->setData(DnsHostRole, i.settings.dnsHost);
            item->setData(DnsPortRole, i.settings.dnsPort);

            if (!firstItem) {
                firstItem = item;
            }

            if (i.id == currentProxy) {
                currentItem = item;
            }
        }

        if (currentItem) {
            firstItem = currentItem;
        }

        if (firstItem) {
            q->ui_.lbx_proxy->setCurrentItem(firstItem);
            q->ui_.lbx_proxy->scrollToItem(firstItem);
        }
    }

    void saveProxies()
    {
        list.clear();
        for (int i = 0; i < q->ui_.lbx_proxy->count(); ++i) {
            QListWidgetItem *item = q->ui_.lbx_proxy->item(i);

            ProxyItem pi;
            pi.name             = item->data(Qt::DisplayRole).toString();
            pi.id               = item->data(IdRole).toString();
            pi.type             = item->data(TypeRole).toString();
            pi.settings.host    = item->data(HostRole).toString();
            pi.settings.port    = item->data(PortRole).toInt();
            pi.settings.user    = item->data(UserRole).toString();
            pi.settings.pass    = item->data(PassRole).toString();
            pi.settings.useAuth = item->data(AuthRole).toBool();
            pi.settings.url     = item->data(UrlRole).toString();
            pi.settings.dnsMode = item->data(DnsModeRole).toString();
            pi.settings.dohUrl  = item->data(DohUrlRole).toString();
            pi.settings.dnsHost = item->data(DnsHostRole).toString();
            pi.settings.dnsPort = item->data(DnsPortRole).toInt();
            list << pi;
        }
    }

    void accept()
    {
        saveProxies();
        emit q->applyList(list, q->ui_.lbx_proxy->currentRow());
        q->accept();
    }

    void currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
    {
        Q_UNUSED(previous);
        q->ui_.pb_remove->setEnabled(bool(current));

        QList<QWidget *> editors = QList<QWidget *>()
            << q->ui_.cb_type << q->ui_.le_host << q->ui_.le_port << q->ui_.le_user << q->ui_.le_pass << q->ui_.le_url
            << q->ui_.gr_auth << q->ui_.gr_dns << q->ui_.cb_dns_mode << q->ui_.le_doh_url << q->ui_.le_dns_host
            << q->ui_.le_dns_port << q->ui_.pb_test_dns;
        for (QWidget *w : std::as_const(editors)) {
            w->blockSignals(true);
            w->setEnabled(bool(current));
            if (!current) {
                if (dynamic_cast<QLineEdit *>(w))
                    dynamic_cast<QLineEdit *>(w)->setText(QString());
            }
        }

        if (current) {
            int type = q->ui_.cb_type->findData(current->data(TypeRole).toString());
            q->ui_.cb_type->setCurrentIndex(type == -1 ? 0 : type);
            q->ui_.le_host->setText(current->data(HostRole).toString());
            q->ui_.le_port->setText(current->data(PortRole).toString());
            q->ui_.le_user->setText(current->data(UserRole).toString());
            q->ui_.le_pass->setText(current->data(PassRole).toString());
            q->ui_.le_url->setText(current->data(UrlRole).toString());
            int mode = q->ui_.cb_dns_mode->findData(current->data(DnsModeRole).toString());
            q->ui_.cb_dns_mode->setCurrentIndex(mode < 0 ? 0 : mode);
            q->ui_.le_doh_url->setText(current->data(DohUrlRole).toString());
            q->ui_.le_dns_host->setText(current->data(DnsHostRole).toString());
            q->ui_.le_dns_port->setText(current->data(DnsPortRole).toString());
            q->ui_.gr_auth->setChecked(true); // make sure gr_auth disables its children if AuthRole is false
            q->ui_.gr_auth->setChecked(current->data(AuthRole).toBool());
        }

        for (QWidget *w : std::as_const(editors)) {
            w->blockSignals(false);
        }

        updateCurrentItem();
    }

    void updateCurrentItem()
    {
        QListWidgetItem *item = q->ui_.lbx_proxy->currentItem();
        if (!item)
            return;

        QString type = q->ui_.cb_type->itemData(q->ui_.cb_type->currentIndex()).toString();
        item->setData(TypeRole, type);
        item->setData(HostRole, q->ui_.le_host->text());
        item->setData(PortRole, q->ui_.le_port->text());
        item->setData(UserRole, q->ui_.le_user->text());
        item->setData(PassRole, q->ui_.le_pass->text());
        item->setData(UrlRole, q->ui_.le_url->text());
        item->setData(AuthRole, q->ui_.gr_auth->isChecked());
        const QString dnsMode = q->ui_.cb_dns_mode->currentData().toString();
        item->setData(DnsModeRole, dnsMode);
        item->setData(DohUrlRole, q->ui_.le_doh_url->text());
        item->setData(DnsHostRole, q->ui_.le_dns_host->text());
        item->setData(DnsPortRole, q->ui_.le_dns_port->text().toInt());

        // Lookups through the proxy need Iris with JDNS and a SOCKS5 or HTTP CONNECT proxy.
        const bool supportsDns = XMPP::RemoteNameProvider::isSupported()
            && (type == QLatin1String("socks") || type == QLatin1String("http"));
        q->ui_.gr_dns->setVisible(supportsDns);
        q->ui_.lb_doh_url->setVisible(dnsMode == QLatin1String("doh"));
        q->ui_.le_doh_url->setVisible(dnsMode == QLatin1String("doh"));
        q->ui_.lb_dns_host->setVisible(dnsMode == QLatin1String("tcp"));
        q->ui_.le_dns_host->setVisible(dnsMode == QLatin1String("tcp"));
        q->ui_.lb_dns_port->setVisible(dnsMode == QLatin1String("tcp"));
        q->ui_.le_dns_port->setVisible(dnsMode == QLatin1String("tcp"));
        q->ui_.pb_test_dns->setEnabled(supportsDns && dnsMode != QLatin1String("off"));

        q->ui_.le_url->setEnabled(type == "poll");
    }

    void testDns()
    {
        QListWidgetItem *item = q->ui_.lbx_proxy->currentItem();
        if (!item || dnsTest)
            return;

        const int proxyPort = q->ui_.le_port->text().toInt();
        if (proxyPort <= 0 || proxyPort > 65535 || q->ui_.le_host->text().isEmpty()) {
            QMessageBox::warning(q, tr("Test proxy DNS"), tr("Invalid proxy host or port."));
            return;
        }
        ProxySettings settings;
        settings.dnsMode  = q->ui_.cb_dns_mode->currentData().toString();
        settings.dohUrl   = q->ui_.le_doh_url->text();
        settings.dnsHost  = q->ui_.le_dns_host->text();
        settings.dnsPort  = q->ui_.le_dns_port->text().toInt();
        const QUrl server = settings.dnsServer();
        if (!XMPP::RemoteNameProvider::isValidServer(server)) {
            QMessageBox::warning(q, tr("Test proxy DNS"), tr("Invalid DNS server address."));
            return;
        }

        bool          ok     = false;
        const QString domain = QInputDialog::getText(q, tr("Test proxy DNS"), tr("XMPP domain to look up:"),
                                                     QLineEdit::Normal, QStringLiteral("jabber.org"), &ok)
                                   .trimmed();
        if (!ok || domain.isEmpty())
            return;

        const QString type = item->data(TypeRole).toString();
        QNetworkProxy proxy(type == QLatin1String("socks") ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy,
                            q->ui_.le_host->text(), quint16(proxyPort));
        if (q->ui_.gr_auth->isChecked()) {
            proxy.setUser(q->ui_.le_user->text());
            proxy.setPassword(q->ui_.le_pass->text());
        }

        // The lookups an XMPP connection through this proxy makes: SRV records,
        // the addresses of their targets, then the domain itself.
        auto provider = new XMPP::RemoteNameProvider(server, proxy, this);
        dnsTest       = new XMPP::ServiceResolver(this);
        dnsTest->setNameProvider(provider);
        dnsTest->setProtocol(XMPP::ServiceResolver::IPv4_IPv6);
        auto found  = std::make_shared<QStringList>();
        auto finish = [this, provider, found]() {
            const QString reason = provider->errorString();
            dnsTest->stop();
            dnsTest->disconnect(this);
            dnsTest->deleteLater();
            provider->deleteLater();
            if (found->isEmpty()) {
                QMessageBox::warning(q, tr("Test proxy DNS"),
                                     reason.isEmpty() ? tr("No XMPP server was found.") : reason);
            } else {
                QMessageBox::information(q, tr("Test proxy DNS"), found->join(QLatin1Char('\n')));
            }
        };
        connect(dnsTest, &XMPP::ServiceResolver::resultReady, this,
                [this, found, finish](const QHostAddress &address, quint16 port, const QString &host,
                                      const QString &service) {
                    const QString endpoint = address.protocol() == QAbstractSocket::IPv6Protocol
                        ? QStringLiteral("[%1]:%2").arg(address.toString()).arg(port)
                        : QStringLiteral("%1:%2").arg(address.toString()).arg(port);
                    *found << QStringLiteral("%1: %2 (%3)")
                                  .arg(service.isEmpty() ? tr("no SRV record") : service, host, endpoint);
                    if (found->size() >= 20) {
                        finish();
                        return;
                    }
                    // the next address, once this signal has returned
                    QMetaObject::invokeMethod(
                        dnsTest, [resolver = dnsTest]() { resolver->tryNext(); }, Qt::QueuedConnection);
                });
        // NoHostLeft once every address was listed
        connect(dnsTest, &XMPP::ServiceResolver::error, this, finish);
        dnsTest->start({ QStringLiteral("xmpps-client"), QStringLiteral("xmpp-client") }, QStringLiteral("tcp"), domain,
                       5222);
    }

    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
    {
        Q_UNUSED(editor);

        if (hint == QAbstractItemDelegate::SubmitModelCache) {
            q->ui_.cb_type->setFocus();
        }
    }

    void addItem(QListWidgetItem *item)
    {
        Q_ASSERT(item);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        q->ui_.lbx_proxy->addItem(item);
    }

    void addProxy()
    {
        QListWidgetItem *item = new QListWidgetItem(tr("Unnamed"));
        addItem(item);
        q->ui_.lbx_proxy->reset(); // ensure that open editors won't get in our way
        item->setData(DnsModeRole,
                      XMPP::RemoteNameProvider::isSupported() ? QStringLiteral("doh") : QStringLiteral("off"));
        item->setData(DohUrlRole, QStringLiteral("https://cloudflare-dns.com/dns-query"));
        item->setData(DnsHostRole, QStringLiteral("1.1.1.1"));
        item->setData(DnsPortRole, 53);
        q->ui_.lbx_proxy->setCurrentItem(item);
        q->ui_.lbx_proxy->editItem(item);
    }

    void removeProxy()
    {
        QListWidgetItem *item = q->ui_.lbx_proxy->takeItem(q->ui_.lbx_proxy->currentRow());
        delete item;
    }
};

ProxyDlg::ProxyDlg(const ProxyItemList &list, const QString &def, QWidget *parent) : QDialog(parent)
{
    ui_.setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    d       = new Private(this);
    d->list = list;

    setWindowTitle(CAP(windowTitle()));
    setModal(true);

    ui_.cb_type->addItem("HTTP \"Connect\"", "http");
    ui_.cb_type->addItem("SOCKS Version 5", "socks");
    ui_.cb_type->addItem("HTTP Polling", "poll");
    ui_.cb_dns_mode->addItem(tr("None"), QStringLiteral("off"));
    ui_.cb_dns_mode->addItem(tr("DNS over HTTPS"), QStringLiteral("doh"));
    ui_.cb_dns_mode->addItem(tr("DNS over TCP"), QStringLiteral("tcp"));
    ui_.cb_dns_mode->setToolTip(tr("A DNS server reached through this proxy. Accounts that use the proxy can look up "
                                   "their server with it (remote SRV lookup), so that no DNS query leaves this "
                                   "computer."));

    ui_.le_host->setToolTip(tr("Enter the hostname and port of your proxy server.") + "  "
                            + tr("Consult your network administrator if necessary."));
    ui_.le_port->setToolTip(ui_.le_host->toolTip());
    ui_.le_user->setToolTip(tr("Enter your proxy server login (username) "
                               "or leave this field blank if the proxy server does not require it.")
                            + "  " + tr("Consult your network administrator if necessary."));
    ui_.le_pass->setToolTip(tr("Enter your proxy server password "
                               "or leave this field blank if the proxy server does not require it.")
                            + "  " + tr("Consult your network administrator if necessary."));
    ui_.cb_type->setToolTip(tr("If you require a proxy server to connect, select the type of proxy here.") + "  "
                            + tr("Consult your network administrator if necessary."));

    d->loadProxies(def);
}

ProxyDlg::~ProxyDlg() { delete d; }

//----------------------------------------------------------------------------
// ProxyChooser
//----------------------------------------------------------------------------
class ProxyChooser::Private {
public:
    Private() = default;

    QComboBox    *cb_proxy = nullptr;
    QPushButton  *pb_edit  = nullptr;
    ProxyManager *m        = nullptr;
};

ProxyChooser::ProxyChooser(ProxyManager *m, QWidget *parent) : QWidget(parent)
{
    d    = new Private;
    d->m = m;
    connect(m, SIGNAL(settingsChanged()), SLOT(pm_settingsChanged()));
    QHBoxLayout *hb = new QHBoxLayout(this);
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(4);
    d->cb_proxy = new QComboBox(this);
    d->cb_proxy->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, d->cb_proxy->sizePolicy().verticalPolicy()));
    hb->addWidget(d->cb_proxy);
    d->pb_edit = new QPushButton(tr("Edit..."), this);
    connect(d->pb_edit, SIGNAL(clicked()), SLOT(doOpen()));
    connect(d->cb_proxy, SIGNAL(activated(int)), SIGNAL(itemChanged()));
    hb->addWidget(d->pb_edit);

    buildComboBox();
}

ProxyChooser::~ProxyChooser() { delete d; }

QString ProxyChooser::currentItem() const { return d->cb_proxy->itemData(d->cb_proxy->currentIndex()).toString(); }

void ProxyChooser::setCurrentItem(const QString &id)
{
    int index = d->cb_proxy->findData(id);
    d->cb_proxy->setCurrentIndex(index == -1 ? 0 : index);
}

void ProxyChooser::pm_settingsChangedApply()
{
    disconnect(d->m, SIGNAL(settingsChanged()), this, SLOT(pm_settingsChangedApply()));
    QString prev = currentItem();
    buildComboBox();
    prev  = d->m->lastEdited();
    int x = d->cb_proxy->findData(prev);
    d->cb_proxy->setCurrentIndex(x == -1 ? 0 : x);
}

void ProxyChooser::pm_settingsChanged()
{
    QString prev = currentItem();
    buildComboBox();
    int x = d->cb_proxy->findData(prev);
    if (x == -1) {
        d->cb_proxy->setCurrentIndex(0);
    } else {
        d->cb_proxy->setCurrentIndex(x);
    }
}

void ProxyChooser::buildComboBox()
{
    d->cb_proxy->clear();
    d->cb_proxy->addItem(tr("None"), "");
    ProxyItemList list = d->m->itemList();
    for (const ProxyItem &pi : std::as_const(list)) {
        d->cb_proxy->addItem(pi.name, pi.id);
    }
}

void ProxyChooser::doOpen()
{
    QString x = d->cb_proxy->itemData(d->cb_proxy->currentIndex()).toString();
    connect(d->m, SIGNAL(settingsChanged()), SLOT(pm_settingsChangedApply()));
    d->m->openDialog(x);
}

//----------------------------------------------------------------------------
// ProxyForObject
//----------------------------------------------------------------------------
const QString defaultItemName = "Default";

ProxyForObject::ProxyForObject(OptionsTree *o, QObject *parent) : QObject(parent), ot_(o) { loadItem(defaultItemName); }

ProxyForObject::~ProxyForObject() { }

QString ProxyForObject::itemForObject(const QString &obj)
{
    if (!items_.contains(obj))
        loadItem(obj);

    return items_.value(obj);
}

void ProxyForObject::save()
{
    items_           = tmp_;
    QString     base = "proxy.";
    const auto &keys = items_.keys();
    for (const QString &obj : keys) {
        QString val = items_.value(obj);
        ot_->setOption(base + obj, QVariant(val));
    }
}

QComboBox *ProxyForObject::getComboBox(ProxyChooser *pc, QWidget *p)
{
    tmp_ = items_;
    pc_  = pc;
    cb_  = new QComboBox(p);
    cb_->addItems(items_.keys());
    cb_->setCurrentIndex(0);
    currentItemChanged(0);

    connect(cb_, SIGNAL(currentIndexChanged(int)), SLOT(currentItemChanged(int)));
    connect(pc_, SIGNAL(itemChanged()), SLOT(updateCurrentItem()));

    return cb_;
}

void ProxyForObject::currentItemChanged(int index)
{
    const QString data = cb_->itemText(index);
    pc_->setCurrentItem(tmp_.value(data));
}

void ProxyForObject::loadItem(const QString &obj)
{
    QVariant v = ot_->getOption(QLatin1String("proxy.") + obj, QString());
    if (!v.isValid())
        v = ot_->getOption(QLatin1String("proxy.") + defaultItemName, QString());
    items_[obj] = v.toString();
}

void ProxyForObject::updateCurrentItem() { tmp_[cb_->currentText()] = pc_->currentItem(); }

//----------------------------------------------------------------------------
// ProxyManager
//----------------------------------------------------------------------------
class ProxyManager::Private {
public:
    Private() = default;

    QPointer<ProxyDlg> pd;
    QList<int>         prevMap;
    QString            lastEdited;
    OptionsTree       *o  = nullptr;
    ProxyForObject    *po = nullptr;

    void itemToOptions(ProxyItem pi)
    {
        QString base = "proxies." + pi.id;
        pi.settings.toOptions(o, base);
        o->setOption(base + ".name", pi.name);
        o->setOption(base + ".type", pi.type);
    }
};

ProxyManager::ProxyManager() : QObject(nullptr) { d = new Private; }

void ProxyManager::init(OptionsTree *o)
{
    d->o = o;
    delete d->po;
    d->po = new ProxyForObject(o, this);
}

ProxyManager::~ProxyManager() { delete d; }

ProxyManager *ProxyManager::instance()
{
    if (!instance_)
        instance_ = new ProxyManager();
    return instance_;
}

ProxyChooser *ProxyManager::createProxyChooser(QWidget *parent) { return new ProxyChooser(this, parent); }

ProxyItemList ProxyManager::itemList() const
{
    QList<ProxyItem> proxies;
    QString          opt  = "proxies";
    QStringList      keys = d->o->getChildOptionNames(opt, true, true);
    for (const QString &key : std::as_const(keys)) {
        proxies += getItem(key.mid(opt.length() + 1));
    }
    return proxies;
}

ProxyItem ProxyManager::getItem(const QString &x) const
{
    ProxyItem pi;
    if (!x.isEmpty()) {
        QString base = "proxies." + x;
        pi.settings.fromOptions(d->o, base);
        pi.name = d->o->getOption(base + ".name").toString();
        pi.type = d->o->getOption(base + ".type").toString();
        pi.id   = x;
    }
    return pi;
}

QString ProxyManager::lastEdited() const { return d->lastEdited; }

void ProxyManager::migrateItemList(const ProxyItemList &list)
{
    for (const ProxyItem &pi : list) {
        d->itemToOptions(pi);
    }
}

void ProxyManager::openDialog(QString def)
{
    if (d->pd)
        bringToFront(d->pd);
    else {
        d->pd = new ProxyDlg(itemList(), def, nullptr);
        connect(d->pd, SIGNAL(applyList(const ProxyItemList &, int)), SLOT(pd_applyList(const ProxyItemList &, int)));
        d->pd->show();
    }
}

void ProxyManager::pd_applyList(const ProxyItemList &list, int x)
{
    QSet<QString> current;

    QString     opt = "proxies";
    QStringList old = d->o->getChildOptionNames(opt, true, true);
    for (int i = 0; i < old.size(); i++) {
        old[i] = old[i].mid(opt.length() + 1);
    }

    // Update all
    int idx = 0;
    int i   = 0;
    for (ProxyItem pi : list) {
        if (pi.id.isEmpty()) {
            do {
                pi.id = "a" + QString::number(idx++);
            } while (old.contains(pi.id) || current.contains(pi.id));
        }
        d->itemToOptions(pi);
        current += pi.id;

        if (i++ == x)
            d->lastEdited = pi.id;
    }

    // and remove removed
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const auto keys = QSet<QString>(old.begin(), old.end()) - current;
#else
    const auto keys = old.toSet() - current;
#endif
    for (const QString &key : keys) {
        d->o->removeOption("proxies." + key, true);
        emit proxyRemoved(key);
    }

    emit settingsChanged();
}

ProxyItem ProxyManager::getItemForObject(const QString &obj)
{
    QString str = obj;
    return getItem(d->po->itemForObject(str.replace(QRegularExpression("\\s+"), "_")));
}

ProxyForObject *ProxyManager::proxyForObject() { return d->po; }

ProxyManager *ProxyManager::instance_ = nullptr;

#include "proxy.moc"
