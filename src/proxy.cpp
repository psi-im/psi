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
#include "common.h"
#include "iconwidget.h"

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

ProxyDlg::ProxyDlg(const ProxyItemList &list, const QStringList &methods, int def, QWidget *parent)
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

	for(ProxyItemList::ConstIterator it = d->list.begin(); it != d->list.end(); ++it)
		lbx_proxy->insertItem((*it).name);
	if(!list.isEmpty()) {
		if(def < 0)
			def = 0;
		lbx_proxy->setCurrentItem(def);
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
	s.id = -1; // id of -1 means 'new'
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

int ProxyChooser::currentItem() const
{
	return d->cb_proxy->currentItem();
}

void ProxyChooser::setCurrentItem(int x)
{
	d->cb_proxy->setCurrentItem(x);
}

void ProxyChooser::pm_settingsChanged()
{
	int x = d->cb_proxy->currentItem();
	buildComboBox();
	if(x >= 1) {
		x = d->m->findOldIndex(x-1);
		if(x == -1)
			d->cb_proxy->setCurrentItem(0);
		else
			d->cb_proxy->setCurrentItem(x+1);
	}
	else {
		x = d->m->lastEdited();
		if(x != -1)
			d->cb_proxy->setCurrentItem(x+1);
	}
}

void ProxyChooser::buildComboBox()
{
	d->cb_proxy->clear();
	d->cb_proxy->insertItem(tr("None"));
	ProxyItemList list = d->m->itemList();
	for(ProxyItemList::ConstIterator it = list.begin(); it != list.end(); ++it)
		d->cb_proxy->insertItem((*it).name);
}

void ProxyChooser::doOpen()
{
	int x = d->cb_proxy->currentItem();
	if(x < 1)
		x = -1;
	else
		--x;
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

	ProxyItemList list;
	QPointer<ProxyDlg> pd;
	QList<int> prevMap;
	int lastEdited;
};

ProxyManager::ProxyManager(QObject *parent)
:QObject(parent)
{
	d = new Private;
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
	return d->list;
}

const ProxyItem & ProxyManager::getItem(int x) const
{
	return d->list[x];
}

int ProxyManager::lastEdited() const
{
	return d->lastEdited;
}

void ProxyManager::setItemList(const ProxyItemList &list)
{
	d->list = list;
	assignIds();
}

QStringList ProxyManager::methodList() const
{
	QStringList list;
	list += "HTTP \"Connect\"";
	list += "SOCKS Version 5";
	list += "HTTP Polling";
	return list;
}

void ProxyManager::openDialog(int def)
{
	if(d->pd)
		bringToFront(d->pd);
	else {
		d->pd = new ProxyDlg(d->list, methodList(), def, 0);
		connect(d->pd, SIGNAL(applyList(const ProxyItemList &, int)), SLOT(pd_applyList(const ProxyItemList &, int)));
		d->pd->show();
	}
}

void ProxyManager::pd_applyList(const ProxyItemList &list, int x)
{
	d->list = list;
	d->lastEdited = x;

	// grab old id list
	d->prevMap.clear();
	for(ProxyItemList::ConstIterator it = d->list.begin(); it != d->list.end(); ++it)
		d->prevMap += (*it).id;
	assignIds(); // re-assign proper ids

	settingsChanged();
}

void ProxyManager::assignIds()
{
	int n = 0;
	for(ProxyItemList::Iterator it = d->list.begin(); it != d->list.end(); ++it)
		(*it).id = n++;
}

int ProxyManager::findOldIndex(int x) const
{
	int newPos = 0;
	bool found = false;
	for(QList<int>::ConstIterator it = d->prevMap.begin(); it != d->prevMap.end(); ++it) {
		if(*it == x) {
			found = true;
			break;
		}
		++newPos;
	}
	if(found)
		return newPos;
	else
		return -1;
}
