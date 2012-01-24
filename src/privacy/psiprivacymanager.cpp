/*
 * Copyright (C) 2006  Remko Troncon
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
 
#include <QObject>
#include <QDebug>

#include "xmpp_xmlcommon.h"
#include "xmpp_task.h"
#include "xmpp_jid.h"
#include "psiprivacymanager.h"
#include "privacymanager.h"
#include "privacylist.h"

#define PRIVACY_NS "jabber:iq:privacy"

using namespace XMPP;

// -----------------------------------------------------------------------------
// 
class PrivacyListListener : public Task
{
	Q_OBJECT

public:
	PrivacyListListener(Task* parent) : Task(parent) {
	}
	
	bool take(const QDomElement &e) {
		if(e.tagName() != "iq" || e.attribute("type") != "set")
			return false;
		
		QString ns = queryNS(e);
		if(ns == "jabber:iq:privacy") {
			// TODO: Do something with update
			
			// Confirm receipt
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			send(iq);
			return true;
		}

		return false;
	}
};

// -----------------------------------------------------------------------------

class GetPrivacyListsTask : public Task
{
	Q_OBJECT

private:
	QDomElement iq_;
	QStringList lists_;
	QString default_, active_;

public:
	GetPrivacyListsTask(Task* parent) : Task(parent) { 
		iq_ = createIQ(doc(), "get", "", id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns",PRIVACY_NS);
		iq_.appendChild(query);	
	}

	void onGo() {
		send(iq_);
	}
	
	bool take(const QDomElement &x) {
		if(!iqVerify(x, "", id()))
			return false;

		//qDebug("privacy.cpp: Got reply for privacy lists.");
		if (x.attribute("type") == "result") {
			QDomElement tag, q = queryTag(x);
			
			for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (e.tagName() == "active") 
					active_ = e.attribute("name");
				else if (e.tagName() == "default")
					default_ = e.attribute("name");
				else if (e.tagName() == "list") 
					lists_.append(e.attribute("name"));
				else
					qWarning("privacy.cpp: Unknown tag in privacy lists.");
				
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

	const QStringList& lists() { 
		return lists_; 
	}

	const QString& defaultList() { 
		return default_;
	}

	const QString& activeList() {
		return active_;
	}
};


class SetPrivacyListsTask : public Task
{
	Q_OBJECT

private:
	bool changeDefault_, changeActive_, changeList_;
	PrivacyList list_;
	QString value_;

public:
	SetPrivacyListsTask(Task* parent) : Task(parent), changeDefault_(false), changeActive_(false), changeList_(false), list_("") { 
	}

	void onGo() {
		QDomElement iq_ = createIQ(doc(), "set", "", id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns",PRIVACY_NS);
		iq_.appendChild(query);	

		QDomElement e;
		if (changeDefault_) {
			//qDebug("privacy.cpp: Changing default privacy list.");
			e = doc()->createElement("default");
			if (!value_.isEmpty())
				e.setAttribute("name",value_);
		}
		else if (changeActive_) {
			//qDebug("privacy.cpp: Changing active privacy list.");
			e = doc()->createElement("active");
			if (!value_.isEmpty()) 
				e.setAttribute("name",value_);
		}
		else if (changeList_) {
			//qDebug("privacy.cpp: Changing privacy list.");
			e = list_.toXml(*doc());
		}
		else {
			qWarning("privacy.cpp: Empty active/default list change request.");
			return;
		}
		
		query.appendChild(e);
		send(iq_);
	}

	void setActive(const QString& active) {
		value_ = active;
		changeDefault_ = false;
		changeActive_ = true;
		changeList_ = false;
	}
	
	void setDefault(const QString& d) {
		value_ = d;
		changeDefault_ = true;
		changeActive_ = false;
		changeList_ = true;
	}
	
	void setList(const PrivacyList& list) {
		//qDebug() << "setList: " << list.toString();
		list_ = list;
		changeDefault_ = false;
		changeActive_ = false;
		changeList_ = true;
	}

	bool take(const QDomElement &x) {
		if(!iqVerify(x, "", id()))
			return false;

		if (x.attribute("type") == "result") {
			//qDebug("privacy.cpp: Got succesful reply for list change.");
			setSuccess();
		}
		else {
			qWarning("privacy.cpp: Got error reply for list change.");
			setError(x);
		}
		return true;
	}
};

class GetPrivacyListTask : public Task
{
	Q_OBJECT

private:
	QDomElement iq_;
	QString name_;
	PrivacyList list_;

public:
	GetPrivacyListTask(Task* parent, const QString& name) : Task(parent), name_(name), list_(PrivacyList("")) { 
		iq_ = createIQ(doc(), "get", "", id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns",PRIVACY_NS);
		iq_.appendChild(query);	
		QDomElement list = doc()->createElement("list");
		list.setAttribute("name",name);
		query.appendChild(list);
	}

	void onGo() {
		//qDebug() << "privacy.cpp: Requesting privacy list %1." << name_;
		send(iq_);
	}
	
	bool take(const QDomElement &x) {
		if(!iqVerify(x, "", id()))
			return false;

		//qDebug() << QString("privacy.cpp: Got privacy list %1 reply.").arg(name_);
		if (x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			bool found;
			QDomElement listTag = findSubTag(q,"list",&found);
			if (found) {
				list_ = PrivacyList(listTag);
			}
			else {
				qWarning("privacy.cpp: No valid list found.");
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

	const PrivacyList& list() { 
		return list_; 
	}
};

// -----------------------------------------------------------------------------

PsiPrivacyManager::PsiPrivacyManager(XMPP::Task* rootTask) : rootTask_(rootTask), getDefault_waiting_(false), block_waiting_(false)
{
   listener_ = new PrivacyListListener(rootTask_);	
}

PsiPrivacyManager::~PsiPrivacyManager()
{
	delete listener_;
}

void PsiPrivacyManager::requestListNames()
{
	GetPrivacyListsTask* t = new GetPrivacyListsTask(rootTask_);
	connect(t,SIGNAL(finished()),SLOT(receiveLists()));
	t->go(true);
}

void PsiPrivacyManager::requestList(const QString& name)
{
	GetPrivacyListTask* t = new GetPrivacyListTask(rootTask_, name);
	connect(t,SIGNAL(finished()),SLOT(receiveList()));
	t->go(true);
}

void PsiPrivacyManager::block(const QString& target)
{
	block_targets_.push_back(target);
	if (!block_waiting_) {
		block_waiting_ = true;
		connect(this,SIGNAL(defaultListAvailable(const PrivacyList&)),SLOT(block_getDefaultList_success(const PrivacyList&)));
		connect(this,SIGNAL(defaultListError()),SLOT(block_getDefaultList_error()));
		getDefaultList();
	}
}

void PsiPrivacyManager::block_getDefaultList_success(const PrivacyList& l_)
{
	PrivacyList l = l_;
	disconnect(this,SIGNAL(defaultListAvailable(const PrivacyList&)),this,SLOT(block_getDefault_success(const PrivacyList&)));
	disconnect(this,SIGNAL(defaultListError()),this,SLOT(block_getDefault_error()));
	block_waiting_ = false;
	while (!block_targets_.isEmpty()) 
		l.insertItem(0,PrivacyListItem::blockItem(block_targets_.takeFirst()));
	changeList(l);
}

void PsiPrivacyManager::block_getDefaultList_error()
{
	disconnect(this,SIGNAL(defaultListAvailable(const PrivacyList&)),this,SLOT(block_getDefault_success(const PrivacyList&)));
	disconnect(this,SIGNAL(defaultListError()),this,SLOT(block_getDefault_error()));
	block_waiting_ = false;
	block_targets_.clear();
}

void PsiPrivacyManager::getDefaultList()
{
	connect(this,SIGNAL(listsReceived(const QString&, const QString&, const QStringList&)),SLOT(getDefault_listsReceived(const QString&, const QString&, const QStringList&)));
	connect(this,SIGNAL(listsError()),SLOT(getDefault_listsError()));
	requestListNames();
}

void PsiPrivacyManager::getDefault_listsReceived(const QString& defaultList, const QString&, const QStringList&)
{
	disconnect(this,SIGNAL(listsReceived(const QString&, const QString&, const QStringList&)),this,SLOT(getDefault_listsReceived(const QString&, const QString&, const QStringList&)));
	disconnect(this,SIGNAL(listsError()),this,SLOT(getDefault_listsError()));
	
	getDefault_default_ = defaultList;
	if (!defaultList.isEmpty()) {
		getDefault_waiting_ = true;
		connect(this,SIGNAL(listReceived(const PrivacyList&)),SLOT(getDefault_listReceived(const PrivacyList&)));
		connect(this,SIGNAL(listError()),SLOT(getDefault_listError()));
		requestList(defaultList);
	}
	else {
		emit defaultListAvailable(PrivacyList(""));
	}
}

void PsiPrivacyManager::getDefault_listsError()
{
	disconnect(this,SIGNAL(listsReceived(const QString&, const QString&, const QStringList&)),this,SLOT(getDefault_listsReceived(const QString&, const QString&, const QStringList&)));
	disconnect(this,SIGNAL(listsError()),this,SLOT(getDefault_listsError()));
	emit defaultListError();
}

void PsiPrivacyManager::getDefault_listReceived(const PrivacyList& l)
{
	if (l.name() == getDefault_default_ && getDefault_waiting_) {
		disconnect(this,SIGNAL(listReceived(const PrivacyList&)),this,SLOT(getDefault_listReceived(const PrivacyList&)));
		disconnect(this,SIGNAL(listError()),this,SLOT(getDefault_listError()));
		getDefault_waiting_ = false;
		emit defaultListAvailable(l);
	}
}

void PsiPrivacyManager::getDefault_listError()
{
	emit defaultListError();
}

void PsiPrivacyManager::changeDefaultList(const QString& name)
{
	SetPrivacyListsTask* t = new SetPrivacyListsTask(rootTask_);
	t->setDefault(name);
	connect(t,SIGNAL(finished()),SLOT(changeDefaultList_finished()));
	t->go(true);
}

void PsiPrivacyManager::changeDefaultList_finished()
{
	SetPrivacyListsTask *t = (SetPrivacyListsTask*)sender();
	if (!t) {
		qWarning("privacy.cpp:changeDefault_finished(): Unexpected sender.");
		return;
	}

	if (t->success()) {
		emit changeDefaultList_success();
	}
	else {
		emit changeDefaultList_error();
	}
}

void PsiPrivacyManager::changeActiveList(const QString& name)
{
	SetPrivacyListsTask* t = new SetPrivacyListsTask(rootTask_);
	t->setActive(name);
	connect(t,SIGNAL(finished()),SLOT(changeActiveList_finished()));
	t->go(true);
}

void PsiPrivacyManager::changeActiveList_finished()
{
	SetPrivacyListsTask *t = (SetPrivacyListsTask*)sender();
	if (!t) {
		qWarning("privacy.cpp:changeActive_finished(): Unexpected sender.");
		return;
	}

	if (t->success()) {
		emit changeActiveList_success();
	}
	else {
		emit changeActiveList_error();
	}
}

void PsiPrivacyManager::changeList(const PrivacyList& list)
{
	SetPrivacyListsTask* t = new SetPrivacyListsTask(rootTask_);
	t->setList(list);
	connect(t,SIGNAL(finished()),SLOT(changeList_finished()));
	t->go(true);
}

void PsiPrivacyManager::changeList_finished()
{
	SetPrivacyListsTask *t = (SetPrivacyListsTask*)sender();
	if (!t) {
		qWarning("privacy.cpp:changeList_finished(): Unexpected sender.");
		return;
	}

	if (t->success()) {
		emit changeList_success();
	}
	else {
		emit changeList_error();
	}
}

void PsiPrivacyManager::receiveLists() 
{
	GetPrivacyListsTask *t = (GetPrivacyListsTask*)sender();
	if (!t) {
		qWarning("privacy.cpp:receiveLists(): Unexpected sender.");
		return;
	}
	
	if (t->success()) {
		emit listsReceived(t->defaultList(),t->activeList(),t->lists());
	}
	else {
		qDebug("privacy.cpp: Error in lists receiving.");
		emit listsError();
	}
}

void PsiPrivacyManager::receiveList() 
{
	GetPrivacyListTask *t = (GetPrivacyListTask*)sender();
	if (!t) {
		qWarning("privacy.cpp:receiveList(): Unexpected sender.");
		return;
	}
	
	if (t->success()) {
		emit listReceived(t->list());
	}
	else {
		qDebug("privacy.cpp: Error in list receiving.");
		emit listError();
	}
}

#include "psiprivacymanager.moc"
