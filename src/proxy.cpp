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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "proxy.h"

#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QLayout>
#include <QComboBox>
#include <QPushButton>
#include <QDomElement>
#include <QPointer>
#include <QApplication>
#include <QHBoxLayout>
#include <QList>

#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"
#include "xmpp_xmlcommon.h"

//----------------------------------------------------------------------------
// ProxySettings
//----------------------------------------------------------------------------
ProxySettings::ProxySettings()
{
	port = 0;
	useAuth = false;
}

#define PASSWORDKEY "Cae1voo:ea}fae2OCai|f1il"

void ProxySettings::toOptions(OptionsTree* o, QString base) const
{
	o->setOption(base + ".host", host);
	o->setOption(base + ".port", port);
	o->setOption(base + ".url", url);
	o->setOption(base + ".useAuth", useAuth);
	o->setOption(base + ".user", user);
	o->setOption(base + ".pass", encodePassword(pass, PASSWORDKEY));
}

void ProxySettings::fromOptions(OptionsTree* o, QString base)
{
	host = o->getOption(base + ".host").toString();
	port = o->getOption(base + ".port").toInt();
	url = o->getOption(base + ".url").toString();
	useAuth = o->getOption(base + ".useAuth").toBool();
	user = o->getOption(base + ".user").toString();
	pass = decodePassword(o->getOption(base + ".pass").toString(), PASSWORDKEY);
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
	return e;
}

bool ProxySettings::fromXml(const QDomElement &e)
{
	host = XMLHelper::subTagText(e, "host");
	port = XMLHelper::subTagText(e, "port").toInt();
	url = XMLHelper::subTagText(e, "url");
	useAuth = (XMLHelper::subTagText(e, "useAuth") == "true") ? true: false;
	user = XMLHelper::subTagText(e, "user");
	pass = XMLHelper::subTagText(e, "pass");
	return true;
}

//----------------------------------------------------------------------------
// ProxyDlg
//----------------------------------------------------------------------------
class ProxyDlg::Private : public QObject
{
	Q_OBJECT
public:
	enum Data {
		HostRole = Qt::UserRole + 1,
		PortRole = Qt::UserRole + 2,
		UrlRole  = Qt::UserRole + 3,
		AuthRole = Qt::UserRole + 4,
		UserRole = Qt::UserRole + 5,
		PassRole = Qt::UserRole + 6,
		IdRole   = Qt::UserRole + 7,
		TypeRole = Qt::UserRole + 8
	};

	ProxyDlg* q;
	ProxyItemList list;

	Private(ProxyDlg* dialog)
		: QObject(dialog)
		, q(dialog)
	{
		Q_ASSERT(q);

		connect(q->ui_.lbx_proxy, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(currentItemChanged(QListWidgetItem*, QListWidgetItem*)));

		connect(q->ui_.pb_new, SIGNAL(clicked()), SLOT(addProxy()));
		connect(q->ui_.pb_remove, SIGNAL(clicked()), SLOT(removeProxy()));

		connect(q->ui_.buttonBox->button(QDialogButtonBox::Save),   SIGNAL(clicked()), this, SLOT(accept()));
		connect(q->ui_.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), q,    SLOT(reject()));
		q->ui_.buttonBox->button(QDialogButtonBox::Save)->setDefault(true);

		connect(q->ui_.lbx_proxy->itemDelegate(), SIGNAL(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)), SLOT(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)));

		connect(q->ui_.le_host, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
		connect(q->ui_.le_port, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
		connect(q->ui_.le_url, SIGNAL(textEdited(const QString&)),  SLOT(updateCurrentItem()));
		connect(q->ui_.le_user, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
		connect(q->ui_.le_pass, SIGNAL(textEdited(const QString&)), SLOT(updateCurrentItem()));
		connect(q->ui_.gr_auth, SIGNAL(toggled(bool)),              SLOT(updateCurrentItem()));
		connect(q->ui_.cb_type, SIGNAL(activated (int)),            SLOT(updateCurrentItem()));
	}

public slots:
	void loadProxies(const QString& currentProxy)
	{
		q->ui_.lbx_proxy->clear();

		QListWidgetItem* firstItem = 0;
		QListWidgetItem* currentItem = 0;
		foreach(ProxyItem i, list) {
			QListWidgetItem* item = new QListWidgetItem(i.name);
			addItem(item);

			item->setData(IdRole,   QVariant(i.id));
			item->setData(TypeRole, QVariant(i.type));
			item->setData(HostRole, QVariant(i.settings.host));
			item->setData(PortRole, QVariant(i.settings.port));
			item->setData(UserRole, QVariant(i.settings.user));
			item->setData(PassRole, QVariant(i.settings.pass));
			item->setData(AuthRole, QVariant(i.settings.useAuth));
			item->setData(UrlRole,  QVariant(i.settings.url));

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
			QListWidgetItem* item = q->ui_.lbx_proxy->item(i);

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
			list << pi;
		}
	}

	void accept()
	{
		saveProxies();
		emit q->applyList(list, q->ui_.lbx_proxy->currentRow());
		q->accept();
	}

	void currentItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
	{
		Q_UNUSED(previous);
		q->ui_.pb_remove->setEnabled(current);

		QList<QWidget*> editors = QList<QWidget*>()
			<< q->ui_.cb_type
			<< q->ui_.le_host
			<< q->ui_.le_port
			<< q->ui_.le_user
			<< q->ui_.le_pass
			<< q->ui_.le_url
			<< q->ui_.gr_auth;
		foreach(QWidget* w, editors) {
			w->blockSignals(true);
			w->setEnabled(current);
			if (!current) {
				if (dynamic_cast<QLineEdit*>(w))
					dynamic_cast<QLineEdit*>(w)->setText(QString());
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
			q->ui_.gr_auth->setChecked(true); // make sure gr_auth disables its children if AuthRole is false
			q->ui_.gr_auth->setChecked(current->data(AuthRole).toBool());
		}

		foreach(QWidget* w, editors) {
			w->blockSignals(false);
		}

		updateCurrentItem();
	}

	void updateCurrentItem()
	{
		QListWidgetItem* item = q->ui_.lbx_proxy->currentItem();
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

		q->ui_.le_url->setEnabled(type == "poll");
	}

	void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
	{
		Q_UNUSED(editor);

		if (hint == QAbstractItemDelegate::SubmitModelCache) {
			q->ui_.cb_type->setFocus();
		}
	}

	void addItem(QListWidgetItem* item)
	{
		Q_ASSERT(item);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		q->ui_.lbx_proxy->addItem(item);
	}

	void addProxy()
	{
		QListWidgetItem* item = new QListWidgetItem(tr("Unnamed"));
		addItem(item);
		q->ui_.lbx_proxy->reset(); // ensure that open editors won't get in our way
		q->ui_.lbx_proxy->setCurrentItem(item);
		q->ui_.lbx_proxy->editItem(item);
	}

	void removeProxy()
	{
		QListWidgetItem* item = q->ui_.lbx_proxy->takeItem(q->ui_.lbx_proxy->currentRow());
		delete item;
	}
};

ProxyDlg::ProxyDlg(const ProxyItemList &list, const QString &def, QWidget *parent)
	: QDialog(parent)
{
	ui_.setupUi(this);

	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private(this);
	d->list = list;

	setWindowTitle(CAP(windowTitle()));
	setModal(true);

	ui_.cb_type->addItem("HTTP \"Connect\"", "http");
	ui_.cb_type->addItem("SOCKS Version 5",  "socks");
	ui_.cb_type->addItem("HTTP Polling",     "poll");

	ui_.le_host->setWhatsThis(
		tr("Enter the hostname and port of your proxy server.") + "  " +
		tr("Consult your network administrator if necessary."));
	ui_.le_port->setWhatsThis(ui_.le_host->whatsThis());
	ui_.le_user->setWhatsThis(
		tr("Enter your proxy server login (username) "
		"or leave this field blank if the proxy server does not require it.") + "  " +
		tr("Consult your network administrator if necessary."));
	ui_.le_pass->setWhatsThis(
		tr("Enter your proxy server password "
		"or leave this field blank if the proxy server does not require it.") + "  " +
		tr("Consult your network administrator if necessary."));
	ui_.cb_type->setWhatsThis(
		tr("If you require a proxy server to connect, select the type of proxy here.") + "  " +
		tr("Consult your network administrator if necessary."));

	d->loadProxies(def);
}

ProxyDlg::~ProxyDlg()
{
	delete d;
}

//----------------------------------------------------------------------------
// ProxyChooser
//----------------------------------------------------------------------------
class ProxyChooser::Private
{
public:
	Private() {}

	QComboBox *cb_proxy;
	QPushButton *pb_edit;
	ProxyManager *m;
};

ProxyChooser::ProxyChooser(ProxyManager* m, QWidget* parent)
	: QWidget(parent)
{
	d = new Private;
	d->m = m;
	connect(m, SIGNAL(settingsChanged()), SLOT(pm_settingsChanged()));
	QHBoxLayout *hb = new QHBoxLayout(this);
	hb->setMargin(0);
	hb->setSpacing(4);
	d->cb_proxy = new QComboBox(this);
	d->cb_proxy->setSizePolicy( QSizePolicy(QSizePolicy::Expanding, d->cb_proxy->sizePolicy().verticalPolicy()) );
	hb->addWidget(d->cb_proxy);
	d->pb_edit = new QPushButton(tr("Edit..."), this);
	connect(d->pb_edit, SIGNAL(clicked()), SLOT(doOpen()));
	connect(d->cb_proxy, SIGNAL(activated(int)), SIGNAL(itemChanged()));
	hb->addWidget(d->pb_edit);

	buildComboBox();
}

ProxyChooser::~ProxyChooser()
{
	delete d;
}

QString ProxyChooser::currentItem() const
{
	return d->cb_proxy->itemData(d->cb_proxy->currentIndex()).toString();
}

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
	prev = d->m->lastEdited();
	int x = d->cb_proxy->findData(prev);
	d->cb_proxy->setCurrentIndex(x == -1 ? 0 : x);
}

void ProxyChooser::pm_settingsChanged()
{
	QString prev = currentItem();
	buildComboBox();
	int x = d->cb_proxy->findData(prev);
	if(x == -1) {
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
	foreach(ProxyItem pi, list) {
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

ProxyForObject::ProxyForObject(OptionsTree *o, QObject *parent)
	: QObject(parent)
	, ot_(o)
{
	loadItem(defaultItemName);
}

ProxyForObject::~ProxyForObject()
{
}

QString ProxyForObject::itemForObject(const QString& obj)
{
	if(!items_.contains(obj))
		loadItem(obj);

	return items_.value(obj);
}

void ProxyForObject::save()
{
	items_ = tmp_;
	QString base = "proxy.";
	foreach(QString obj, items_.keys()) {
		QString val = items_.value(obj);
		ot_->setOption(base+obj, QVariant(val));
	}
}

QComboBox* ProxyForObject::getComboBox(ProxyChooser* pc, QWidget* p)
{
	tmp_ = items_;
	pc_ = pc;
	cb_ = new QComboBox(p);
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

void ProxyForObject::loadItem(const QString& obj)
{
	QVariant v = ot_->getOption(("proxy." + obj));
	if(!v.isValid())
		v = ot_->getOption(("proxy." + defaultItemName));
	items_[obj] = v.toString();
}

void ProxyForObject::updateCurrentItem()
{
	tmp_[cb_->currentText()] = pc_->currentItem();
}

//----------------------------------------------------------------------------
// ProxyManager
//----------------------------------------------------------------------------
class ProxyManager::Private
{
public:
	Private() : po(0) {}

	QPointer<ProxyDlg> pd;
	QList<int> prevMap;
	QString lastEdited;
	OptionsTree *o;
	ProxyForObject* po;

	void itemToOptions(ProxyItem pi) {
		QString base = "proxies." + pi.id;
		pi.settings.toOptions( o, base);
		o->setOption(base + ".name", pi.name);
		o->setOption(base + ".type", pi.type);
	}
};

ProxyManager::ProxyManager()
		: QObject(0)
{
	d = new Private;
}

void ProxyManager::init(OptionsTree *o)
{
	d->o = o;
	delete d->po;
	d->po = new ProxyForObject(o, this);
}

ProxyManager::~ProxyManager()
{
	delete d;
}

ProxyManager* ProxyManager::instance()
{
	if(!instance_)
		instance_ = new ProxyManager();
	return instance_;
}

ProxyChooser *ProxyManager::createProxyChooser(QWidget *parent)
{
	return new ProxyChooser(this, parent);
}

ProxyItemList ProxyManager::itemList() const
{
	QList<ProxyItem> proxies;
	QString opt = "proxies";
	QStringList keys = d->o->getChildOptionNames(opt, true, true);
	foreach(QString key, keys) {
		proxies += getItem(key.mid(opt.length()+1));
	}
	return proxies;
}

ProxyItem ProxyManager::getItem(const QString &x) const
{
	ProxyItem pi;
	if(!x.isEmpty()) {
		QString base = "proxies." + x;
		pi.settings.fromOptions( d->o, base);
		pi.name = d->o->getOption(base + ".name").toString();
		pi.type = d->o->getOption(base + ".type").toString();
		pi.id = x;
	}
	return pi;
}

QString ProxyManager::lastEdited() const
{
	return d->lastEdited;
}

void ProxyManager::migrateItemList(const ProxyItemList &list)
{
	foreach(ProxyItem pi, list) {
		d->itemToOptions(pi);
	}
}

void ProxyManager::openDialog(QString def)
{
	if(d->pd)
		bringToFront(d->pd);
	else {
		d->pd = new ProxyDlg(itemList(), def, 0);
		connect(d->pd, SIGNAL(applyList(const ProxyItemList &, int)), SLOT(pd_applyList(const ProxyItemList &, int)));
		d->pd->show();
	}
}

void ProxyManager::pd_applyList(const ProxyItemList &list, int x)
{
	QSet<QString> current;

	QString opt = "proxies";
	QStringList old = d->o->getChildOptionNames(opt, true, true);
	for (int i=0; i < old.size(); i++) {
		old[i] = old[i].mid(opt.length()+1);
	}

	// Update all
	int idx = 0;
	int i = 0;
	foreach(ProxyItem pi, list) {
		if (pi.id.isEmpty()) {
			do {
				pi.id = "a"+QString::number(idx++);
			} while (old.contains(pi.id) || current.contains(pi.id));
		}
		d->itemToOptions(pi);
		current += pi.id;

		if (i++ == x) d->lastEdited = pi.id;
	}

	// and remove removed
	foreach(QString key, old.toSet() - current) {
		d->o->removeOption("proxies." + key, true);
		emit proxyRemoved(key);
	}

	settingsChanged();
}

ProxyItem ProxyManager::getItemForObject(const QString& obj)
{
	QString str = obj;
	return getItem(d->po->itemForObject(str.replace(QRegExp("\\s+"), "_")));
}

ProxyForObject* ProxyManager::proxyForObject()
{
	return d->po;
}

ProxyManager* ProxyManager::instance_ = NULL;

#include "proxy.moc"
