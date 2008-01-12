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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "proxy.h"

#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <q3grid.h>
#include <q3hbox.h>
#include <q3vbox.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <q3listbox.h>
#include <qdom.h>
#include <qpointer.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QHBoxLayout>
#include <QList>
#include <QDebug>
#include "common.h"
#include "iconwidget.h"
#include "psioptions.h"

static QDomElement textTag(QDomDocument &doc, const QString &name, const QString &content)
{
	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(content);
	tag.appendChild(text);

	return tag;
}

static QDomElement textTag(QDomDocument &doc, const QString &name, bool content)
{
	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(content ? "true" : "false");
	tag.appendChild(text);

	return tag;
}

static QDomElement findSubTag(const QDomElement &e, const QString &name)
{
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == name)
			return i;
	}

	return QDomElement();
}

//----------------------------------------------------------------------------
// HostPortEdit
//----------------------------------------------------------------------------
class HostPortEdit::Private
{
public:
	Private() {}

	QLineEdit *le_host, *le_port;
};

HostPortEdit::HostPortEdit(QWidget *parent, const char *name)
:QWidget(parent, name)
{
	d = new Private;

	QLabel *l;
	QHBoxLayout *hb = new QHBoxLayout(this, 0, 4);
	l = new QLabel(tr("Host:"), this);
	hb->addWidget(l);
	d->le_host = new QLineEdit(this);
	d->le_host->setMinimumWidth(128);
	hb->addWidget(d->le_host);
	l = new QLabel(tr("Port:"), this);
	hb->addWidget(l);
	d->le_port = new QLineEdit(this);
	d->le_port->setFixedWidth(64);
	hb->addWidget(d->le_port);
	setPort(0);
}

HostPortEdit::~HostPortEdit()
{
	delete d;
}

QString HostPortEdit::host() const
{
	return d->le_host->text();
}

int HostPortEdit::port() const
{
	return d->le_port->text().toInt();
}

void HostPortEdit::setHost(const QString &s)
{
	d->le_host->setText(s);
}

void HostPortEdit::setPort(int x)
{
	d->le_port->setText(QString::number(x));
}

void HostPortEdit::fixTabbing(QWidget *a, QWidget *b)
{
	setTabOrder(a, d->le_host);
	setTabOrder(d->le_host, d->le_port);
	setTabOrder(d->le_port, b);
}


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
	e.appendChild(textTag(*doc, "host", host));
	e.appendChild(textTag(*doc, "port", QString::number(port)));
	e.appendChild(textTag(*doc, "url", url));
	e.appendChild(textTag(*doc, "useAuth", useAuth));
	e.appendChild(textTag(*doc, "user", user));
	e.appendChild(textTag(*doc, "pass", pass));
	return e;
}

bool ProxySettings::fromXml(const QDomElement &e)
{
	host = findSubTag(e, "host").text();
	port = findSubTag(e, "port").text().toInt();
	url = findSubTag(e, "url").text();
	useAuth = (findSubTag(e, "useAuth").text() == "true") ? true: false;
	user = findSubTag(e, "user").text();
	pass = findSubTag(e, "pass").text();
	return true;
}


//----------------------------------------------------------------------------
// ProxyEdit
//----------------------------------------------------------------------------
class ProxyEdit::Private
{
public:
	Private() {}

	HostPortEdit *hp_host;
	QCheckBox *ck_auth;
	QLabel *lb_url;
	QLineEdit *le_url, *le_user, *le_pass;
	Q3Grid *gr_auth;
};

ProxyEdit::ProxyEdit(QWidget *parent, const char *name)
:Q3GroupBox(1, Qt::Horizontal, tr("Settings"), parent, name)
{
	d = new Private;

	//QVBoxLayout *vb = new QVBoxLayout(this);
	//QVBox *gb = new QVBox(this);
	//gb->setSpacing(4);
	//vb->addWidget(gb);

	QLabel *l;

	d->hp_host = new HostPortEdit(this);

	Q3HBox *hb = new Q3HBox(this);
	d->lb_url = new QLabel(tr("Polling URL:"), hb);
	d->le_url = new QLineEdit(hb);

	d->ck_auth = new QCheckBox(tr("Use authentication"), this);
	connect(d->ck_auth, SIGNAL(toggled(bool)), SLOT(ck_toggled(bool)));

	d->gr_auth = new Q3Grid(2, Qt::Horizontal, this);
	d->gr_auth->setSpacing(4);
	l = new QLabel(tr("Username:"), d->gr_auth);
	d->le_user = new QLineEdit(d->gr_auth);
	l = new QLabel(tr("Password:"), d->gr_auth);
	d->le_pass = new QLineEdit(d->gr_auth);
	d->le_pass->setEchoMode(QLineEdit::Password);
	d->gr_auth->setEnabled(false);

	d->hp_host->setWhatsThis(
		tr("Enter the hostname and port of your proxy server.") + "  " +
		tr("Consult your network administrator if necessary."));
	d->le_user->setWhatsThis(
		tr("Enter your proxy server login (username) "
		"or leave this field blank if the proxy server does not require it.") + "  " +
		tr("Consult your network administrator if necessary."));
	d->le_pass->setWhatsThis(
		tr("Enter your proxy server password "
		"or leave this field blank if the proxy server does not require it.") + "  " +
		tr("Consult your network administrator if necessary."));
}

ProxyEdit::~ProxyEdit()
{
	delete d;
}

void ProxyEdit::reset()
{
	d->hp_host->setHost("");
	d->hp_host->setPort(0);
	d->le_url->setText("");
	d->ck_auth->setChecked(false);
	d->le_user->setText("");
	d->le_pass->setText("");
}

void ProxyEdit::setType(const QString &s)
{
	if(s == "poll") {
		d->lb_url->setEnabled(true);
		d->le_url->setEnabled(true);
	}
	else {
		d->lb_url->setEnabled(false);
		d->le_url->setEnabled(false);
	}
}

ProxySettings ProxyEdit::proxySettings() const
{
	ProxySettings s;
	s.host = d->hp_host->host();
	s.port = d->hp_host->port();
	if(d->le_url->isEnabled())
		s.url = d->le_url->text();
	s.useAuth = d->ck_auth->isChecked();
	s.user = d->le_user->text();
	s.pass = d->le_pass->text();
	return s;
}

void ProxyEdit::setProxySettings(const ProxySettings &s)
{
	d->hp_host->setHost(s.host);
	d->hp_host->setPort(s.port);
	d->le_url->setText(s.url);
	d->ck_auth->setChecked(s.useAuth);
	d->le_user->setText(s.user);
	d->le_pass->setText(s.pass);
}

void ProxyEdit::ck_toggled(bool b)
{
	d->gr_auth->setEnabled(b);
}

void ProxyEdit::fixTabbing(QWidget *a, QWidget *b)
{
	d->hp_host->fixTabbing(a, d->le_url);
	setTabOrder(d->le_url, d->ck_auth);
	setTabOrder(d->ck_auth, d->le_user);
	setTabOrder(d->le_user, d->le_pass);
	setTabOrder(d->le_pass, b);
}


//----------------------------------------------------------------------------
// ProxyDlg
//----------------------------------------------------------------------------
class ProxyDlg::Private
{
public:
	Private() {}

	ProxyEdit *pe_settings;
	ProxyItemList list;
	int last;
};

ProxyDlg::ProxyDlg(const ProxyItemList &list, const QStringList &methods, const QString &def, QWidget *parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	setupUi(this);
	setWindowTitle(CAP(caption()));
	setModal(QApplication::activeModalWidget() ? true: false);

	d->list = list;
	d->last = -1;
	d->pe_settings = new ProxyEdit(gb_prop);
	replaceWidget(lb_proxyedit, d->pe_settings);
	d->pe_settings->fixTabbing(cb_type, pb_close);

	hookEdit();

	connect(pb_new, SIGNAL(clicked()), SLOT(proxy_new()));
	connect(pb_remove, SIGNAL(clicked()), SLOT(proxy_remove()));
	connect(pb_save, SIGNAL(clicked()), SLOT(doSave()));
	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	gb_prop->setEnabled(false);
	pb_remove->setEnabled(false);
	pb_save->setDefault(true);

	cb_type->insertStringList(methods);
	connect(cb_type, SIGNAL(activated(int)), SLOT(cb_activated(int)));

	int defIdx = -1;
	int i = 0;
	for(ProxyItemList::ConstIterator it = d->list.begin(); it != d->list.end(); ++it) {
		lbx_proxy->insertItem((*it).name);
		if ((*it).id == def) defIdx= i;
		++i;
	}
	if(!list.isEmpty()) {
		if(defIdx < 0)
			defIdx = 0;
		lbx_proxy->setCurrentItem(defIdx);
		selectCurrent();
	}

	cb_type->setWhatsThis(
		tr("If you require a proxy server to connect, select the type of proxy here.") + "  " +
		tr("Consult your network administrator if necessary."));
	// TODO: whatsthis for name
}

ProxyDlg::~ProxyDlg()
{
	delete d;
}

void ProxyDlg::proxy_new()
{
	ProxyItem s;
	s.id = ""; // id of "" means 'new'
	s.name = getUniqueName();
	d->list += s;

	lbx_proxy->insertItem(s.name);
	lbx_proxy->setCurrentItem(lbx_proxy->count()-1);
	selectCurrent();
}

void ProxyDlg::proxy_remove()
{
	int x = lbx_proxy->currentItem();
	if(x != -1) {
		ProxyItemList::Iterator it = d->list.begin();
		for(int n = 0; n < x; ++n)
			++it;
		d->list.remove(it);

		d->last = -1;
		lbx_proxy->removeItem(x);
		selectCurrent();
	}
}

void ProxyDlg::cb_activated(int x)
{
	if(x == 0)
		d->pe_settings->setType("http");
	else if(x == 1)
		d->pe_settings->setType("socks");
	else if(x == 2)
		d->pe_settings->setType("poll");
}

void ProxyDlg::selectCurrent()
{
	int x = lbx_proxy->currentItem();
	if(x != -1)
		lbx_proxy->setSelected(x, true);
}

QString ProxyDlg::getUniqueName() const
{
	QString str;
	int x = 0;
	bool found;
	do {
		str = QString("Proxy %1").arg(++x);
		found = false;
		for(ProxyItemList::ConstIterator it = d->list.begin(); it != d->list.end(); ++it) {
			if(str == (*it).name) {
				found = true;
				break;
			}
		}
	} while(found);
	return str;
}

void ProxyDlg::saveIntoItem(int x)
{
	ProxyItem &s = d->list[x];
	s.name = le_name->text();
	int i = cb_type->currentItem();
	s.type = "http";
	if(i == 0)
		s.type = "http";
	else if(i == 1)
		s.type = "socks";
	else if(i == 2)
		s.type = "poll";
	s.settings = d->pe_settings->proxySettings();
}

void ProxyDlg::qlbx_highlighted(int x)
{
	// if we are moving off of another item, save its content
	if(d->last != -1)
		saveIntoItem(d->last);
	d->last = x;

	// display the new item's content
	if(x == -1) {
		le_name->setText("");
		cb_type->setCurrentItem(0);
		d->pe_settings->reset();
		gb_prop->setEnabled(false);
		pb_remove->setEnabled(false);
	}
	else {
		ProxyItem &s = d->list[x];
		le_name->setText(s.name);
		int i = 0;
		if(s.type == "http")
			i = 0;
		else if(s.type == "socks")
			i = 1;
		else if(s.type == "poll")
			i = 2;
		cb_type->setCurrentItem(i);
		d->pe_settings->setProxySettings(s.settings);
		cb_activated(i);
		gb_prop->setEnabled(true);
		pb_remove->setEnabled(true);
	}
}

void ProxyDlg::qle_textChanged(const QString &s)
{
	int x = lbx_proxy->currentItem();
	if(x != -1) {
		unhookEdit();
		lbx_proxy->changeItem(s, x);
		hookEdit();
	}
}

// hookEdit / unhookEdit - disconnect some signals to prevent recursion
void ProxyDlg::hookEdit()
{
	connect(lbx_proxy, SIGNAL(highlighted(int)), this, SLOT(qlbx_highlighted(int)));
	connect(le_name, SIGNAL(textChanged(const QString &)), this, SLOT(qle_textChanged(const QString &)));
}

void ProxyDlg::unhookEdit()
{
	disconnect(lbx_proxy, SIGNAL(highlighted(int)), this, SLOT(qlbx_highlighted(int)));
	disconnect(le_name, SIGNAL(textChanged(const QString &)), this, SLOT(qle_textChanged(const QString &)));
}

void ProxyDlg::doSave()
{
	int x = lbx_proxy->currentItem();
	if(x != -1)
		saveIntoItem(x);
	applyList(d->list, x);
	accept();
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

ProxyChooser::ProxyChooser(ProxyManager *m, QWidget *parent, const char *name)
:QWidget(parent, name)
{
	d = new Private;
	d->m = m;
	connect(m, SIGNAL(settingsChanged()), SLOT(pm_settingsChanged()));
	QHBoxLayout *hb = new QHBoxLayout(this, 0, 4);
	d->cb_proxy = new QComboBox(this);
	QSizePolicy sp = d->cb_proxy->sizePolicy();
	d->cb_proxy->setSizePolicy( QSizePolicy(QSizePolicy::Expanding, sp.verData()) );
	hb->addWidget(d->cb_proxy);
	d->pb_edit = new QPushButton(tr("Edit..."), this);
	connect(d->pb_edit, SIGNAL(clicked()), SLOT(doOpen()));
	hb->addWidget(d->pb_edit);

	buildComboBox();
}

ProxyChooser::~ProxyChooser()
{
	delete d;
}

QString ProxyChooser::currentItem() const
{
	return d->cb_proxy->itemData(d->cb_proxy->currentItem()).toString();
}

void ProxyChooser::setCurrentItem(const QString &id)
{
	d->cb_proxy->setCurrentItem(d->cb_proxy->findData(id));
}

void ProxyChooser::pm_settingsChangedApply()
{
	disconnect(d->m, SIGNAL(settingsChanged()), this, SLOT(pm_settingsChangedApply()));
	QString prev = currentItem();
	buildComboBox();
	prev = d->m->lastEdited();
	int x = d->cb_proxy->findData(prev);
	if(x == -1) {
		d->cb_proxy->setCurrentItem(0);
	} else {
		d->cb_proxy->setCurrentItem(x);
	}
}


void ProxyChooser::pm_settingsChanged()
{
	QString prev = currentItem();
	buildComboBox();
	int x = d->cb_proxy->findData(prev);
	if(x == -1) {
		d->cb_proxy->setCurrentItem(0);
	} else {
		d->cb_proxy->setCurrentItem(x);
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
	QString x = d->cb_proxy->itemData(d->cb_proxy->currentItem()).toString();
	connect(d->m, SIGNAL(settingsChanged()), SLOT(pm_settingsChangedApply()));
	d->m->openDialog(x);
}

void ProxyChooser::fixTabbing(QWidget *a, QWidget *b)
{
	setTabOrder(a, d->cb_proxy);
	setTabOrder(d->cb_proxy, d->pb_edit);
	setTabOrder(d->pb_edit, b);
}


//----------------------------------------------------------------------------
// ProxyManager
//----------------------------------------------------------------------------
class ProxyManager::Private
{
public:
	Private() {}

	QPointer<ProxyDlg> pd;
	QList<int> prevMap;
	QString lastEdited;
	OptionsTree *o;
	
	void itemToOptions(ProxyItem pi) {
		QString base = "proxies." + pi.id;
		pi.settings.toOptions( o, base);
		o->setOption(base + ".name", pi.name);
		o->setOption(base + ".type", pi.type);
	}
};

ProxyManager::ProxyManager(OptionsTree *opt, QObject *parent)
		: QObject(parent)
{
	d = new Private;
	d->o = opt;
}

ProxyManager::~ProxyManager()
{
	delete d;
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
	QString base = "proxies." + x;
	ProxyItem pi;
	pi.settings.fromOptions( d->o, base);
	pi.name = d->o->getOption(base + ".name").toString();
	pi.type = d->o->getOption(base + ".type").toString();
	pi.id = x;
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

QStringList ProxyManager::methodList() const
{
	QStringList list;
	list += "HTTP \"Connect\"";
	list += "SOCKS Version 5";
	list += "HTTP Polling";
	return list;
}

void ProxyManager::openDialog(QString def)
{
	if(d->pd)
		bringToFront(d->pd);
	else {
		d->pd = new ProxyDlg(itemList(), methodList(), def, 0);
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
