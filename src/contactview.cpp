/*
 * contactview.cpp - contact list widget
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "contactview_b.h"

#include <QFileDialog>
#include <qapplication.h>
#include <q3ptrlist.h>
#include <q3header.h>
#include <qtimer.h>
#include <qpainter.h>
#include <q3popupmenu.h>
#include <qmessagebox.h>
#include <qinputdialog.h>
#include <qicon.h>
#include <q3dragobject.h>
#include <qlayout.h>
#include <QKeyEvent>
#include <QEvent>
#include <QList>
#include <QDropEvent>
#include <QPixmap>
#include <QDesktopWidget>
#include <stdlib.h>
#include "common.h"
#include "userlist.h"
#include "psiaccount_b.h"
#include "psicon.h"
#include "jidutil.h"
#include "psioptions.h"
#include "iconaction.h"
#include "pgputil.h"
#include "alerticon.h"
#include "avatars.h"
#include "psiiconset.h"
#include "serverinfomanager.h"
#include "pepmanager.h"
#include "psitooltip.h"
#include "capsmanager.h"
#include "resourcemenu.h"
#include "shortcutmanager.h"
#include "xmpp_message.h"
#include "xmpp_vcard.h"
#include "vcardfactory.h"
#include "cudaskin.h"
#include "xmpp_client.h"
#include "textutil.h"
#include "bookmarkmanagedlg.h"
#include "bookmarkmanager.h"
#include "simpleprivacymanager.h"

#define FAVNAME "Favorites"

// from mainwin.cpp
QImage makeAvatarImage(const QImage &_in);

static inline int rankStatus(int status) 
{
	switch (status) {
		case STATUS_CHAT : return 0;
		case STATUS_ONLINE : return 1;
		case STATUS_AWAY : return 2;
		case STATUS_XA : return 3;
		case STATUS_DND : return 4;
		case STATUS_INVISIBLE: return 5;
		default:
			return 6;
	}
	return 0;
}

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

//----------------------------------------------------------------------------
// ContactProfile
//----------------------------------------------------------------------------
class ContactProfile::Entry
{
public:
	Entry()
	{
		alerting = false;
		cvi.setAutoDelete(true);
	}
	~Entry()
	{
	}

	UserListItem u;
	Q3PtrList<ContactViewItemB> cvi;
	bool alerting;
	PsiIcon anim;
};

class ContactProfile::Private : public QObject
{
	Q_OBJECT
public:
	Private() {}

	QString name;
	ContactView *cv;
	ContactViewItemB *cvi;
	ContactViewItemB *self;
	UserListItem su;
	Q3PtrList<Entry> roster;
	Q3PtrList<ContactViewItemB> groups;
	int oldstate;
	QTimer *t;
	PsiAccount *pa;
	bool v_enabled;
	ContactViewItemB *rename_item;
	int rename_loops;

	// a transport contact is considered to be a jid whose domain matches
	//   one of the account services domains.  additionally, the jid must
	//   have a node (this is so a transport itself doesn't count as a
	//   transport contact).
	bool isTransportContact(const UserListItem *u) const
	{
		// node required
		if(u->jid().node().isEmpty())
			return false;

		// matching domain?
		QList<Jid> list = pa->servicesList();
		Jid udomain = u->jid().domain();
		foreach(const Jid &jid, list)
		{
			Jid sdomain = jid.domain();
			if(udomain.compare(sdomain))
				return true;
		}

		return false;
	}

public slots:
	/*
	 * \brief This slot is toggled when number of active accounts is changed
	 *
	 * At the moment, it tries to recalculate the roster size.
	 */
	void numAccountsChanged()
	{
		cv->recalculateSize();
	}

	void delayedRename()
	{
		if(rename_item)
		{
			rename_item->doRename();
			cv->setSelected(rename_item, true);
			rename_loops = 2;
			QTimer::singleShot(0, this, SLOT(delayedRename2()));
		}
	}

	void delayedRename2()
	{
		if(rename_loops > 0)
		{
			--rename_loops;
			QTimer::singleShot(0, this, SLOT(delayedRename2()));
			return;
		}

		rename_item = 0;
	}
};

ContactProfile::ContactProfile(PsiAccount *pa, const QString &name, ContactView *cv, bool unique)
{
	d = new Private;
	d->pa = pa;
	d->v_enabled = d->pa->enabled();
	d->name = name;
	d->cv = cv;
	d->cv->link(this);
	d->t = new QTimer;
	connect(d->t, SIGNAL(timeout()), SLOT(updateGroups()));
	connect(pa->psi(), SIGNAL(accountCountChanged()), d, SLOT(numAccountsChanged()));
	d->rename_item = 0;
	d->rename_loops = 0;

	d->roster.setAutoDelete(true);

	d->self = 0;

	if (!unique)
		d->cvi = new ContactViewItemB(name, this, d->cv);
	else
		d->cvi = 0;

	d->oldstate = -2;

	deferredUpdateGroups();
}

ContactProfile::~ContactProfile()
{
	// delete the roster
	clear();

	// clean up
	delete d->self;
	delete d->cvi;

	delete d->t;
	d->cv->unlink(this);

	delete d;
}

void ContactProfile::setEnabled(bool e)
{
	d->v_enabled = e;
	if(d->v_enabled){
		//if(!d->cvi)
		//	d->cvi = new ContactViewItemB(d->name, this, d->cv);
		addAllNeededContactItems();
	}
	else{
		if(d->self)
			removeSelf();

		removeAllUnneededContactItems();
		if(d->cvi)
			delete d->cvi;
		d->cvi = 0;
		d->self = 0;
	}
}

ContactView *ContactProfile::contactView() const
{
	return d->cv;
}

ContactViewItemB *ContactProfile::self() const
{
	return d->self;
}

PsiAccount *ContactProfile::psiAccount() const
{
	return d->pa;
}

const QString & ContactProfile::name() const
{
	return d->name;
}

void ContactProfile::setName(const QString &name)
{
	d->name = name;
	if (d->cvi) {
		d->cvi->setProfileName(name);
	}
}

void ContactProfile::setName(const char *s)
{
	QObject::setName(s);
}

void ContactProfile::setState(int state)
{
	if(state == d->oldstate)
		return;
	d->oldstate = state;

	if(d->cvi) {
		d->cv->resetAnim();
		d->cvi->setProfileState(state);
	}
}

void ContactProfile::setUsingSSL(bool on)
{
	if(d->cvi)
		d->cvi->setProfileSSL(on);
}

ContactViewItemB *ContactProfile::addGroup(int type)
{
	ContactViewItemB *item;

	QString gname;
	if(type == ContactViewItemB::gGeneral)
		gname = tr("General");
	else if(type == ContactViewItemB::gNotInList)
		gname = tr("Not in list");
	else if(type == ContactViewItemB::gAgents)
		gname = tr("Agents/Transports");
	else if(type == ContactViewItemB::gPrivate)
		gname = tr("Private Messages");

	if(d->cvi)
		item = new ContactViewItemB(gname, type, this, d->cvi);
	else
		item = new ContactViewItemB(gname, type, this, d->cv);

	d->groups.append(item);

	return item;
}

ContactViewItemB *ContactProfile::addGroup(const QString &name)
{
	ContactViewItemB *item;
	if(d->cvi)
		item = new ContactViewItemB(name, ContactViewItemB::gUser, this, d->cvi);
	else
		item = new ContactViewItemB(name, ContactViewItemB::gUser, this, d->cv);

	d->groups.append(item);

	return item;
}

// check for special group
ContactViewItemB *ContactProfile::checkGroup(int type)
{
	ContactViewItemB *item;
	if(d->cvi)
		item = (ContactViewItemB *)d->cvi->firstChild();
	else
		item = (ContactViewItemB *)d->cv->firstChild();

	for(; item; item = (ContactViewItemB *)item->nextSibling()) {
		if(item->type() == ContactViewItemB::Group && item->groupType() == type)
			return item;
	}

	return 0;
}

// make a tooltip with account information
QString ContactProfile::makeTip(bool trim, bool doLinkify) const
{
	if (d->cvi)
		return "<qt> <center> <b>" + d->cvi->text(0) + " " + d->cvi->groupInfo() + "</b> </center> " + d->su.makeBareTip(trim,doLinkify) + "</qt>";
	else
		return d->su.makeTip(trim,doLinkify);
}

// check for user group
ContactViewItemB *ContactProfile::checkGroup(const QString &name)
{
	ContactViewItemB *item;
	if(d->cvi)
		item = (ContactViewItemB *)d->cvi->firstChild();
	else
		item = (ContactViewItemB *)d->cv->firstChild();

	for(; item; item = (ContactViewItemB *)item->nextSibling()) {
		if(item->type() == ContactViewItemB::Group && item->groupType() == ContactViewItemB::gUser && item->groupName() == name)
				return item;
	}

	return 0;
}

ContactViewItemB *ContactProfile::ensureGroup(int type)
{
	ContactViewItemB *group_item = checkGroup(type);
	if(!group_item)
		group_item = addGroup(type);

	return group_item;
}

ContactViewItemB *ContactProfile::ensureGroup(const QString &name)
{
	ContactViewItemB *group_item = checkGroup(name);
	if(!group_item)
		group_item = addGroup(name);

	return group_item;
}

void ContactProfile::checkDestroyGroup(const QString &group)
{
	ContactViewItemB *group_item = checkGroup(group);
	if(group_item)
		checkDestroyGroup(group_item);
}

void ContactProfile::checkDestroyGroup(ContactViewItemB *group)
{
	if(group->childCount() == 0) {
		d->groups.remove(group);
		delete group;
	}
}

void ContactProfile::updateEntry(const UserListItem &u)
{
	if (u.isSelf()) {
		// Update the self item
		d->su = u;

		// Show and/or update item if necessary
		if (d->cv->isShowSelf() || d->su.userResourceList().count() > 1) {
			if (d->self) {
				updateSelf();
			}
			else {
				addSelf();
			}
		}
		else {
			removeSelf();
		}
	}
	else {
		Entry *e = findEntry(u.jid());
		if(!e) {
			e = new Entry;
			d->roster.append(e);
			e->u = u;
		}
		else {
			e->u = u;
			removeUnneededContactItems(e);

			// update remaining items
			Q3PtrListIterator<ContactViewItemB> it(e->cvi);
			for(ContactViewItemB *i; (i = it.current()); ++it) {
				i->setContact(&e->u);
				if(!u.isAvailable())
					i->stopAnimateNick();
			}
		}

		deferredUpdateGroups();
		addNeededContactItems(e);
	}
}

void ContactProfile::updateSelf()
{
	if (d->self) {
		d->self->setContact(&d->su);
		if(!d->su.isAvailable())
			d->self->stopAnimateNick();
	}
}

void ContactProfile::addSelf()
{
	if(!d->self) {
		/*if(!d->cvi)
			return;
		d->self = new ContactViewItemB(&d->su, this, d->cvi);*/
		d->self = new ContactViewItemB(&d->su, this, d->cv);
	}
}

void ContactProfile::removeSelf()
{
	if (d->self) {
		delete d->self;
		d->self = 0;
	}
}

ContactViewItemB *ContactProfile::addContactItem(Entry *e, ContactViewItemB *group_item)
{
	ContactViewItemB *i = new ContactViewItemB(&e->u, this, group_item);
	e->cvi.append(i);
	if(e->alerting)
		i->setAlert(&e->anim);
	deferredUpdateGroups();
	//printf("ContactProfile: adding [%s] to group [%s]\n", e->u.jid().full().latin1(), group_item->groupName().latin1());
	return i;
}

/*
 * \brief Ensures that specified Entry is present in contactlist
 *
 * \param e - Entry with the necessary data about item
 * \param group_item - ContactViewItemB that will be the group for this item
 */
ContactViewItemB *ContactProfile::ensureContactItem(Entry *e, ContactViewItemB *group_item)
{
	d->cv->recalculateSize();

	Q3PtrListIterator<ContactViewItemB> it(e->cvi);
	for(ContactViewItemB *i; (i = it.current()); ++it) {
		ContactViewItemB *g = (ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent();
		if(g == group_item)
			return i;
	}
	return addContactItem(e, group_item);
}

/*
 * \brief Removes specified item from ContactView
 *
 * \param e - Entry with item's data
 * \param i - ContactViewItemB corresponding to the e
 */
void ContactProfile::removeContactItem(Entry *e, ContactViewItemB *i)
{
	d->cv->recalculateSize();

	ContactViewItemB *group_item = (ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent();
	//printf("ContactProfile: removing [%s] from group [%s]\n", e->u.jid().full().latin1(), group_item->groupName().latin1());
	e->cvi.removeRef(i);
	deferredUpdateGroups();
	checkDestroyGroup(group_item);
}

void ContactProfile::addNeededContactItems(Entry *e)
{
	if(!d->v_enabled)
		return;

	const UserListItem &u = e->u;

	if(u.inList()) {
		// don't add if we're not supposed to see it
		if(u.isTransport()) {
			if(!d->cv->isShowAgents() && !e->alerting) {
				return;
			}
		}
		else {
			if(!e->alerting) {
				if((!d->cv->isShowOffline() && !u.isAvailable()) || (!d->cv->isShowAway() && u.isAway()) || (!d->cv->isShowHidden() && u.isHidden()))
					return;
			}
		}
	}

	if(u.isPrivate())
		ensureContactItem(e, ensureGroup(ContactViewItemB::gPrivate));
	else if(!u.inList())
		ensureContactItem(e, ensureGroup(ContactViewItemB::gNotInList));
	else if(u.isTransport()) {
		ensureContactItem(e, ensureGroup(ContactViewItemB::gAgents));
	}
	else if(u.groups().isEmpty())
		ensureContactItem(e, ensureGroup(ContactViewItemB::gGeneral));
	else {
		const QStringList &groups = u.groups();
		for(QStringList::ConstIterator git = groups.begin(); git != groups.end(); ++git)
			ensureContactItem(e, ensureGroup(*git));
	}
}

void ContactProfile::removeUnneededContactItems(Entry *e)
{
	const UserListItem &u = e->u;
	
	if(u.inList()) {
		bool delAll = !d->v_enabled;
		if(u.isTransport()) {
			if(!d->cv->isShowAgents() && !e->alerting) {
				delAll = true;
			}
		}
		else {
			if(!e->alerting) {
				if((!d->cv->isShowOffline() && !u.isAvailable()) || (!d->cv->isShowAway() && u.isAway()) || (!d->cv->isShowHidden() && u.isHidden()))
					delAll = true;
			}
		}
		if(delAll) {
			clearContactItems(e);
			return;
		}
	}

	Q3PtrListIterator<ContactViewItemB> it(e->cvi);
	for(ContactViewItemB *i; (i = it.current());) {
		bool del = false;
		ContactViewItemB *g = (ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent();

		if(g->groupType() == ContactViewItemB::gNotInList && u.inList())
			del = true;
		else if(g->groupType() != ContactViewItemB::gNotInList && g->groupType() != ContactViewItemB::gPrivate && !u.inList())
			del = true;
		else if(g->groupType() == ContactViewItemB::gAgents && !u.isTransport())
			del = true;
		else if(g->groupType() != ContactViewItemB::gAgents && u.isTransport())
			del = true;
		else if(g->groupType() == ContactViewItemB::gGeneral && !u.groups().isEmpty())
			del = true;
		else if(g->groupType() != ContactViewItemB::gPrivate && g->groupType() != ContactViewItemB::gGeneral && u.groups().isEmpty() && !u.isTransport() && u.inList())
			del = true;
		else if(g->groupType() == ContactViewItemB::gUser) {
			const QStringList &groups = u.groups();
			if(!groups.isEmpty()) {
				bool found = false;
				for(QStringList::ConstIterator git = groups.begin(); git != groups.end(); ++git) {
					if(g->groupName() == *git) {
						found = true;
						break;
					}
				}
				if(!found)
					del = true;
			}
		}
		else if(PsiOptions::instance()->getOption("options.ui.contactlist.auto-delete-unlisted").toBool() && !e->alerting && (g->groupType() == ContactViewItemB::gPrivate || g->groupType() == ContactViewItemB::gNotInList)) {
			del = true;
		}

		if(del) {
			removeContactItem(e, i);
		}
		else
			++it;
	}
}

void ContactProfile::clearContactItems(Entry *e)
{
	Q3PtrListIterator<ContactViewItemB> it(e->cvi);
	for(ContactViewItemB *i; (i = it.current());)
		removeContactItem(e, i);
}

void ContactProfile::addAllNeededContactItems()
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it)
		addNeededContactItems(e);
}

void ContactProfile::removeAllUnneededContactItems()
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it)
		removeUnneededContactItems(e);
}

void ContactProfile::resetAllContactItemNames()
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		Q3PtrListIterator<ContactViewItemB> cvi_it(e->cvi);
		for(ContactViewItemB *i; (i = cvi_it.current()); ++cvi_it) {
			i->resetName();
			contactView()->filterContact(i);
		}
	}
}

void ContactProfile::removeEntry(const Jid &j)
{
	Entry *e = findEntry(j);
	if(e)
		removeEntry(e);
}

void ContactProfile::removeEntry(Entry *e)
{
	e->alerting = false;
	clearContactItems(e);
	d->roster.remove(e);
}

void ContactProfile::setAlert(const Jid &j, const PsiIcon *anim)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->setAlert(anim);
	}
	else {
		Entry *e = findEntry(j);
		if(!e)
			return;

		e->alerting = true;
		e->anim = *anim;
		addNeededContactItems(e);
		Q3PtrListIterator<ContactViewItemB> it(e->cvi);
		for(ContactViewItemB *i; (i = it.current()); ++it)
			i->setAlert(anim);

		if(PsiOptions::instance()->getOption("options.ui.contactlist.ensure-contact-visible-on-event").toBool())
			ensureVisible(e);
	}
}

void ContactProfile::clearAlert(const Jid &j)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->clearAlert();
	}
	else {
		Entry *e = findEntry(j);
		if(!e)
			return;

		e->alerting = false;
		Q3PtrListIterator<ContactViewItemB> it(e->cvi);
		for(ContactViewItemB *i; (i = it.current()); ++it)
			i->clearAlert();
		removeUnneededContactItems(e);
	}
}

void ContactProfile::clear()
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current());)
		removeEntry(e);
}

ContactProfile::Entry *ContactProfile::findEntry(const Jid &jid) const
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		if(e->u.jid().compare(jid))
			return e;
	}
	return 0;
}

ContactProfile::Entry *ContactProfile::findEntry(ContactViewItemB *i) const
{
	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		Q3PtrListIterator<ContactViewItemB> ci(e->cvi);
		for(ContactViewItemB *cvi; (cvi = ci.current()); ++ci) {
			if(cvi == i)
				return e;
		}
	}
	return 0;
}

// return a list of contacts from a CVI group
QList<XMPP::Jid> ContactProfile::contactListFromCVGroup(ContactViewItemB *group) const
{
	QList<XMPP::Jid> list;

	for(ContactViewItemB *item = (ContactViewItemB *)group->firstChild(); item ; item = (ContactViewItemB *)item->nextSibling()) {
		if(item->type() != ContactViewItemB::Contact)
			continue;

		list.append(item->u()->jid());
	}

	return list;
}

// return the number of contacts from a CVI group
int ContactProfile::contactSizeFromCVGroup(ContactViewItemB *group) const
{
	int total = 0;

	for(ContactViewItemB *item = (ContactViewItemB *)group->firstChild(); item ; item = (ContactViewItemB *)item->nextSibling()) {
		if(item->type() != ContactViewItemB::Contact)
			continue;

		++total;
	}

	return total;
}

// return the number of contacts from a CVI group
int ContactProfile::contactsOnlineFromCVGroup(ContactViewItemB *group) const
{
	int total = 0;

	for(ContactViewItemB *item = (ContactViewItemB *)group->firstChild(); item ; item = (ContactViewItemB *)item->nextSibling()) {
		if(item->type() == ContactViewItemB::Contact && item->u()->isAvailable())
			++total;
	}

	return total;
}

// return a list of contacts associated with "groupName"
QList<XMPP::Jid> ContactProfile::contactListFromGroup(const QString &groupName) const
{
	QList<XMPP::Jid> list;

	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		const UserListItem &u = e->u;
		if(u.isTransport())
			continue;
		const QStringList &g = u.groups();
		if(g.isEmpty()) {
			if(groupName.isEmpty())
				list.append(u.jid());
		}
		else {
			for(QStringList::ConstIterator git = g.begin(); git != g.end(); ++git) {
				if(*git == groupName) {
					list.append(u.jid());
					break;
				}
			}
		}
	}

	return list;
}

// return the number of contacts associated with "groupName"
int ContactProfile::contactSizeFromGroup(const QString &groupName) const
{
	int total = 0;

	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		const UserListItem &u = e->u;
		if(u.isTransport())
			continue;
		const QStringList &g = u.groups();
		if(g.isEmpty()) {
			if(groupName.isEmpty())
				++total;
		}
		else {
			for(QStringList::ConstIterator git = g.begin(); git != g.end(); ++git) {
				if(*git == groupName) {
					++total;
					break;
				}
			}
		}
	}

	return total;
}

void ContactProfile::updateGroupInfo(ContactViewItemB *group)
{
	int type = group->groupType();
	if(type == ContactViewItemB::gGeneral || type == ContactViewItemB::gAgents || type == ContactViewItemB::gPrivate || type == ContactViewItemB::gUser) {
		int online = contactsOnlineFromCVGroup(group);
		int total;
		if(type == ContactViewItemB::gGeneral || type == ContactViewItemB::gUser) {
			QString gname;
			if(type == ContactViewItemB::gUser)
				gname = group->groupName();
			else
				gname = "";
			total = contactSizeFromGroup(gname);
		}
		else {
			total = group->childCount();
		}
		if (PsiOptions::instance()->getOption("options.ui.contactlist.show-group-counts").toBool())
			group->setGroupInfo(QString("(%1/%2)").arg(online).arg(total));
	}
	else if (PsiOptions::instance()->getOption("options.ui.contactlist.show-group-counts").toBool()) {
		int inGroup = contactSizeFromCVGroup(group);
		group->setGroupInfo(QString("(%1)").arg(inGroup));
	}
}

QStringList ContactProfile::groupList() const
{
	QStringList groupList;

	Q3PtrListIterator<Entry> it(d->roster);
	for(Entry *e; (e = it.current()); ++it) {
		foreach(QString group, e->u.groups()) {
			if (!groupList.contains(group))
				groupList.append(group);
		}
	}

	groupList.sort();
	return groupList;
}

void ContactProfile::animateNick(const Jid &j)
{
	if(d->su.jid().compare(j)) {
		if(d->self)
			d->self->setAnimateNick();
	}

	Entry *e = findEntry(j);
	if(!e)
		return;
	Q3PtrListIterator<ContactViewItemB> it(e->cvi);
	for(ContactViewItemB *i; (i = it.current()); ++it)
		i->setAnimateNick();
}

void ContactProfile::deferredUpdateGroups()
{
	d->t->start(250, true);
}

void ContactProfile::updateGroups()
{
	int totalOnline = 0;
	{
		Q3PtrListIterator<Entry> it(d->roster);
		for(Entry *e; (e = it.current()); ++it) {
			if(e->u.isAvailable())
				++totalOnline;
		}
		if(d->cvi && PsiOptions::instance()->getOption("options.ui.contactlist.show-group-counts").toBool())
			d->cvi->setGroupInfo(QString("(%1/%2)").arg(totalOnline).arg(d->roster.count()));
	}

	{
		Q3PtrListIterator<ContactViewItemB> it(d->groups);
		for(ContactViewItemB *g; (g = it.current()); ++it)
		{
			updateGroupInfo(g);
			contactView()->filterGroup(g);
		}
	}
}

void ContactProfile::ensureVisible(const Jid &j)
{
	Entry *e = findEntry(j);
	if(!e)
		return;
	ensureVisible(e);
}

void ContactProfile::ensureVisible(Entry *e)
{
	if(!e->alerting) {
		if(!d->cv->isShowAgents() && e->u.isTransport())
			d->cv->setShowAgents(true);
		if(!d->cv->isShowOffline() && !e->u.isAvailable())
			d->cv->setShowOffline(true);
		if(!d->cv->isShowAway() && e->u.isAway())
			d->cv->setShowAway(true);
		if(!d->cv->isShowHidden() && e->u.isHidden())
			d->cv->setShowHidden(true);
	}

	ContactViewItemB *i = e->cvi.first();
	if(!i)
		return;
	d->cv->ensureItemVisible(i);
}

void ContactProfile::doContextMenu(ContactViewItemB *i, const QPoint &pos)
{
	bool online = d->pa->loggedIn();

	if(i->type() == ContactViewItemB::Profile) {
		Q3PopupMenu pm;

		Q3PopupMenu *am = new Q3PopupMenu(&pm);
		am->insertItem(IconsetFactory::icon("psi/disco").icon(), tr("Online Users"), 5);
		am->insertItem(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send server message"), 1);
		am->insertSeparator();
		am->insertItem(/*IconsetFactory::iconPixmap("psi/edit"),*/ tr("Set MOTD"), 2);
		am->insertItem(/*IconsetFactory::iconPixmap("psi/edit/clear"),*/ tr("Update MOTD"), 3);
		am->insertItem(IconsetFactory::icon("psi/remove").icon(), tr("Delete MOTD"), 4);

		const int status_start = 15;
		Q3PopupMenu *sm = new Q3PopupMenu(&pm);
		sm->insertItem(PsiIconset::instance()->status(STATUS_ONLINE).icon(),	status2txt(STATUS_ONLINE),	STATUS_ONLINE		+ status_start);
		if (PsiOptions::instance()->getOption("options.ui.menu.status.chat").toBool())
			sm->insertItem(PsiIconset::instance()->status(STATUS_CHAT).icon(),		status2txt(STATUS_CHAT),	STATUS_CHAT		+ status_start);
		sm->insertSeparator();
		sm->insertItem(PsiIconset::instance()->status(STATUS_AWAY).icon(),		status2txt(STATUS_AWAY),	STATUS_AWAY		+ status_start);
		if (PsiOptions::instance()->getOption("options.ui.menu.status.xa").toBool())
			sm->insertItem(PsiIconset::instance()->status(STATUS_XA).icon(),		status2txt(STATUS_XA),		STATUS_XA		+ status_start);
		sm->insertItem(PsiIconset::instance()->status(STATUS_DND).icon(),		status2txt(STATUS_DND),		STATUS_DND		+ status_start);
		if (PsiOptions::instance()->getOption("options.ui.menu.status.invisible").toBool()) {
			sm->insertSeparator();
			sm->insertItem(PsiIconset::instance()->status(STATUS_INVISIBLE).icon(),	status2txt(STATUS_INVISIBLE),	STATUS_INVISIBLE	+ status_start);
		}
		sm->insertSeparator();
		sm->insertItem(PsiIconset::instance()->status(STATUS_OFFLINE).icon(),	status2txt(STATUS_OFFLINE),	STATUS_OFFLINE		+ status_start);
		pm.insertItem(tr("&Status"), sm);
#ifdef USE_PEP
		pm.insertItem(tr("Mood"), 11);
		pm.setItemEnabled(11, d->pa->serverInfoManager()->hasPEP());

		Q3PopupMenu *avatarm = new Q3PopupMenu (&pm);
		avatarm->insertItem(tr("Set Avatar"), 12);
		avatarm->insertItem(tr("Unset Avatar"), 13);
		pm.insertItem(tr("Avatar"), avatarm, 14);
		pm.setItemEnabled(14, d->pa->serverInfoManager()->hasPEP());
#endif
		const int bookmarks_start = STATUS_CHAT + status_start + 1; // STATUS_CHAT is the highest value of the states
		QMenu *bookmarks = new QMenu(&pm);
		bookmarks->insertItem(tr("Manage..."), bookmarks_start);
		if (d->pa->bookmarkManager()->isAvailable()) {
			int idx = 1;
			bookmarks->insertSeparator();
			foreach(ConferenceBookmark c, psiAccount()->bookmarkManager()->conferences()) {
				bookmarks->insertItem(QString(tr("Join %1")).arg(c.name()), bookmarks_start + idx);
				idx++;
			}
		}
		else {
			bookmarks->setEnabled(false);
		}

		pm.insertItem(tr("Bookmarks"), bookmarks);

		pm.insertSeparator();
		pm.insertItem(IconsetFactory::icon("psi/addContact").icon(), tr("&Add a contact"), 7);
		pm.insertItem(IconsetFactory::icon("psi/disco").icon(), tr("Service &Discovery"), 9);
		if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
			pm.insertItem(IconsetFactory::icon("psi/sendMessage").icon(), tr("New &blank message"), 6);
		pm.insertSeparator();
		pm.insertItem(IconsetFactory::icon("psi/xml").icon(), tr("&XML Console"), 10);
		pm.insertSeparator();
		pm.insertItem(IconsetFactory::icon("psi/account").icon(), tr("&Modify Account..."), 0);
		
		if (PsiOptions::instance()->getOption("options.ui.menu.account.admin").toBool()) {
			pm.insertSeparator();
			pm.setItemEnabled(pm.insertItem(tr("&Admin"), am), online);
		}

		int x = pm.exec(pos);

		if(x == -1)
			return;

		if(x == 0)
			d->pa->modify();
		else if(x == 1) {
			Jid j = d->pa->jid().host() + '/' + "announce/online";
			actionSendMessage(j);
		}
		else if(x == 2) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd";
			actionSendMessage(j);
		}
		else if(x == 3) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd/update";
			actionSendMessage(j);
		}
		else if(x == 4) {
			Jid j = d->pa->jid().host() + '/' + "announce/motd/delete";
			Message m;
			m.setTo(j);
			d->pa->dj_sendMessage(m, false);
		}
		else if(x == 5) {
			// FIXME: will it still work on XMPP servers?
			Jid j = d->pa->jid().host() + '/' + "admin";
			actionDisco(j, "");
		}
		else if(x == 6) {
			actionSendMessage("");
		}
		else if(x == 7) {
			d->pa->openAddUserDlg();
		}
		else if(x == 9) {
			Jid j = d->pa->jid().host();
			actionDisco(j, "");
		}
		else if(x == 10) {
			d->pa->showXmlConsole();
		}
		else if(x == 11 && pm.isItemEnabled(11)) {
			emit actionSetMood();
		}
		else if(x == 12  && pm.isItemEnabled(14)) {
			emit actionSetAvatar();
		}
		else if(x == 13  && pm.isItemEnabled(14)) {
			emit actionUnsetAvatar();
		}
		else if(x >= status_start && x <= STATUS_CHAT + status_start) { // STATUS_CHAT is the highest value of the states
			int status = x - status_start;
			d->pa->changeStatus(status);
		}
		else if(x == bookmarks_start) {
			BookmarkManageDlg *dlg = d->pa->findDialog<BookmarkManageDlg*>();
			if(dlg) {
				bringToFront(dlg);
			} else {
				dlg = new BookmarkManageDlg(d->pa);
				dlg->show();
			}
		}
		else if (x > bookmarks_start) {
			ConferenceBookmark c = psiAccount()->bookmarkManager()->conferences()[x - bookmarks_start - 1];
			psiAccount()->actionJoin(c, true);
		}
	}
	else if(i->type() == ContactViewItemB::Group) {
		QString gname = i->groupName();
		Q3PopupMenu pm;

		if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
			//pm.insertItem(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send message to group"), 0);
			pm.insertItem(tr("Send message to group"), 0);
		if(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
			// disable if it's not a user group
			if(!online || i->groupType() != ContactViewItemB::gUser || gname == ContactView::tr("Hidden")) {
				d->cv->qa_ren->setEnabled(false);
				pm.setItemEnabled(2, false);
				pm.setItemEnabled(3, false);
			}
			else
				d->cv->qa_ren->setEnabled(true);

			d->cv->qa_ren->addTo(&pm);
			pm.insertSeparator();
			//pm.insertItem(IconsetFactory::icon("psi/remove").icon(), tr("Remove group"), 2);
			pm.insertItem(tr("Remove group"), 2);
			//pm.insertItem(IconsetFactory::icon("psi/remove").icon(), tr("Remove group and contacts"), 3);
			pm.insertItem(tr("Remove group and contacts"), 3);
		}

		if(i->groupType() == ContactViewItemB::gAgents) {
			pm.insertSeparator();
			pm.insertItem(tr("Hide"), 4);
		}

		int x = pm.exec(pos);

		// restore actions
		if(PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool())
			d->cv->qa_ren->setEnabled(false);
		else
			d->cv->qa_ren->setEnabled(true);

		if(x == -1)
			return;

		if(x == 0) {
			QList<XMPP::Jid> list = contactListFromCVGroup(i);

			// send multi
			actionSendMessage(list);
		}
		else if(x == 2 && online) {
			int n = QMessageBox::information(d->cv, tr("Remove Group"),tr(
			"This will cause all contacts in this group to be disassociated with it.\n"
			"\n"
			"Proceed?"), tr("&Yes"), tr("&No"));

			if(n == 0) {
				QList<XMPP::Jid> list = contactListFromGroup(i->groupName());
				for(QList<Jid>::Iterator it = list.begin(); it != list.end(); ++it)
					actionGroupRemove(*it, gname);
			}
		}
		else if(x == 3 && online) {
			int n = QMessageBox::information(d->cv, tr("Remove Group and Contacts"),tr(
			"WARNING!  This will remove all contacts associated with this group!\n"
			"\n"
			"Proceed?"), tr("&Yes"), tr("&No"));

			if(n == 0) {
				QList<XMPP::Jid> list = contactListFromGroup(i->groupName());
				for(QList<Jid>::Iterator it = list.begin(); it != list.end(); ++it) {
					removeEntry(*it);
					actionRemove(*it);
				}
			}
		}
		else if(x == 4) {
			if(i->groupType() == ContactViewItemB::gAgents)
				d->cv->setShowAgents(false);
		}
	}
	else if(i->type() == ContactViewItemB::Contact) {
		bool self = false;
		UserListItem *u;
		Entry *e = 0;
		if(i == d->self) {
			self = true;
			u = &d->su;
		}
		else {
			e = findEntry(i);
			if(!e)
				return;
			u = &e->u;
		}

		QStringList gl = groupList();
		qSort(gl.begin(), gl.end(), caseInsensitiveLessThan);

		bool inList = e ? e->u.inList() : false;
		bool isPrivate = e ? e->u.isPrivate(): false;
		bool isAgent = e ? e->u.isTransport() : false;
		bool avail = e ? e->u.isAvailable() : false;
		QString groupNameCache;
		if(static_cast<Q3ListViewItem *>(i)->parent())
			groupNameCache = ((ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent())->groupName();

		bool asMoreMenu = true;

		Q3PopupMenu pm;

		if(!self && !inList && !isPrivate && !PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
			//pm.insertItem(IconsetFactory::icon("psi/addContact").icon(), tr("Add/Authorize to contact list"), 10);
			pm.insertItem(tr("Add/Authorize to contact list"), 10);
			if(!online)
				pm.setItemEnabled(10, false);
			pm.insertSeparator();
		}

		if ( (self  && i->isAlerting()) ||
		     (!self && e->alerting) ) {
			d->cv->qa_recv->addTo(&pm);
			pm.insertSeparator();
		}

		//if (PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
		//	d->cv->qa_send->addTo(&pm);

		//pm.insertItem(QIconSet(PsiIconset::instance()->url), tr("Send &URL"), 2);

		const UserResourceList &rl = u->userResourceList();

		int base_sendto = 32;
		int at_sendto = 0;
		ResourceMenu *s2m  = new ResourceMenu(&pm);
		ResourceMenu *c2m  = new ResourceMenu(&pm);
#ifdef WHITEBOARDING
		ResourceMenu *wb2m = new ResourceMenu(&pm);
#endif
		ResourceMenu *rc2m = new ResourceMenu(&pm);

		if(!rl.isEmpty()) {
			for(UserResourceList::ConstIterator it = rl.begin(); it != rl.end(); ++it) {
				const UserResource &r = *it;
				s2m->addResource(r,  base_sendto+at_sendto++);
				c2m->addResource(r,  base_sendto+at_sendto++);
#ifdef WHITEBOARDING
				wb2m->addResource(r,  base_sendto+at_sendto++);
#endif
				rc2m->addResource(r, base_sendto+at_sendto++);
			}
		}

		//if(!isPrivate && PsiOptions::instance()->getOption("options.ui.message.enabled").toBool())
		//	pm.insertItem(tr("Send message to"), s2m, 17);

		if(!asMoreMenu)
		{
			//d->cv->qa_chat->setIconSet(IconsetFactory::iconPixmap("psi/start-chat"));
			d->cv->qa_chat->addTo(&pm);
		}

		//if(!isPrivate)
		//	pm.insertItem(tr("Open chat to"), c2m, 18);

#ifdef WHITEBOARDING
		//d->cv->qa_wb->setIconSet(IconsetFactory::iconPixmap("psi/whiteboard"));
		d->cv->qa_wb->addTo(&pm);

		if(!isPrivate)
			pm.insertItem(tr("Open a whiteboard to"), wb2m, 19);
#endif
		
		if(!isPrivate) {
			if(rl.isEmpty()) {
				pm.setItemEnabled(17, false);
				pm.setItemEnabled(18, false);
#ifdef WHITEBOARDING
				pm.setItemEnabled(19, false);
#endif
			}
		}
		
		// TODO: Add executeCommand() thing
		//if(!isPrivate) {
		//	pm.insertItem(tr("E&xecute command"), rc2m, 25);
		//	pm.setItemEnabled(25, !rl.isEmpty());
		//}

		int base_hidden = base_sendto + at_sendto;
		int at_hidden = 0;
		QStringList hc;
		/*if(!isPrivate && PsiOptions::instance()->getOption("options.ui.menu.contact.active-chats").toBool()) {
			hc = d->pa->hiddenChats(u->jid());
			ResourceMenu *cm = new ResourceMenu(&pm);
			for(QStringList::ConstIterator it = hc.begin(); it != hc.end(); ++it) {
				// determine status
				int status;
				const UserResourceList &rl = u->userResourceList();
				UserResourceList::ConstIterator uit = rl.find(*it);
				if(uit != rl.end() || (uit = rl.priority()) != rl.end())
					status = makeSTATUS((*uit).status());
				else
					status = STATUS_OFFLINE;
				cm->addResource(status, *it, base_hidden+at_hidden++);
			}
			pm.insertItem(tr("Active chats"), cm, 7);
			if(hc.isEmpty())
				pm.setItemEnabled(7, false);
		}*/

		// Voice call
#ifdef QUICKVOIP
		if(!isAgent) {
#else
		if(d->pa->voiceCaller() && !isAgent) {
#endif
			//pm.insertItem(IconsetFactory::icon("psi/voice").icon(), tr("Voice Call"), 24);
			pm.insertItem(tr("Voice Call"), 24);
			if(!online) {
				pm.setItemEnabled(24, false);
			}
			/*else {
				bool hasVoice = false;
				const UserResourceList &rl = u->userResourceList();
				for (UserResourceList::ConstIterator it = rl.begin(); it != rl.end() && !hasVoice; ++it) {
					hasVoice = psiAccount()->capsManager()->features(u->jid().withResource((*it).name())).canVoice();
				}
				pm.setItemEnabled(24,!psiAccount()->capsManager()->isEnabled() || hasVoice);
			}*/
		}
		
		if(!asMoreMenu) {
		if(!isAgent) {
			pm.insertSeparator();
			pm.insertItem(IconsetFactory::icon("psi/upload").icon(), tr("Send &file"), 23);
			if(!online)
				pm.setItemEnabled(23, false);
		}
		}

		// invites
		int base_gc = base_hidden + at_hidden;
		int at_gc = 0;
		/*QStringList groupchats;
		if(!isPrivate && !isAgent) {
			Q3PopupMenu *gm = new Q3PopupMenu(&pm);
			groupchats = d->pa->groupchats();
			for(QStringList::ConstIterator it = groupchats.begin(); it != groupchats.end(); ++it) {
				int id = gm->insertItem(*it, base_gc+at_gc++);
				if(!online)
					gm->setItemEnabled(id, false);
			}
			//pm.insertItem(IconsetFactory::iconPixmap("psi/groupChat"), tr("Invite to"), gm, 14);
			pm.insertItem(tr("Invite to"), gm, 14);
			if(groupchats.isEmpty())
				pm.setItemEnabled(14, false);
		}

		// weird?
		if(inList || !isAgent)
			pm.insertSeparator();*/

		int base_group = base_gc + at_gc;

		QString qa_rem_origText;
		if(!self) {
			if(inList) {
				if(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
					d->cv->qa_ren->setEnabled(online);
					d->cv->qa_ren->addTo(&pm);
				}
			}

			if(!isAgent) {
				if(inList) {
					if(!u->groups().contains(FAVNAME)) {
						pm.insertItem(tr("Add to &Favorites"), 26);
						if(!online)
							pm.setItemEnabled(26, false);
					}
					else if(groupNameCache == FAVNAME) {
						qa_rem_origText = d->cv->qa_rem->text();
						d->cv->qa_rem->setText("Rem&ove from Favorites");
						d->cv->qa_rem->addTo(&pm);
						if(online)
							d->cv->qa_rem->setEnabled(true);
						else
							d->cv->qa_rem->setEnabled(false);
					}
				}

				if(inList && !PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
					Q3PopupMenu *gm = new Q3PopupMenu(&pm);

					gm->setCheckable(true);
					gm->insertItem(tr("&None"), 8);
					gm->insertSeparator();

					QString g;
					if(e->u.groups().isEmpty())
						gm->setItemChecked(8, true);
					else
						g = groupNameCache;

					int n = 0;
					gl.remove(ContactView::tr("Hidden"));
					for(QStringList::ConstIterator it = gl.begin(); it != gl.end(); ++it) {
						QString str;
						if(n < 9)
							str = "&";
						str += QString("%1. %2").arg(n+1).arg(*it);
						gm->insertItem(str, n+base_group);

						if(*it == g)
							gm->setItemChecked(n+base_group, true);
						++n;
					}
					if(n > 0)
						gm->insertSeparator();

					gm->insertItem(ContactView::tr("Hidden"),n+base_group);
					if(g == ContactView::tr("Hidden"))
						gm->setItemChecked(n+base_group, true);
					gm->insertSeparator();
					gm->insertItem(/*IconsetFactory::iconPixmap("psi/edit/clear"),*/ tr("&Create new..."), 9);

					if(groupNameCache != FAVNAME) {
						pm.insertItem(tr("&Group"), gm, 5);

						if(!online)
							pm.setItemEnabled(5, false);
					}
				}
			}
			else {
				pm.insertSeparator();

				d->cv->qa_logon->setEnabled( !avail && online );

				d->cv->qa_logon->setIcon(PsiIconset::instance()->status(e->u.jid(), STATUS_ONLINE).icon());
				d->cv->qa_logon->addTo(&pm);

				pm.insertItem(PsiIconset::instance()->status(e->u.jid(), STATUS_OFFLINE).icon(), tr("Log off"), 16);
				if(!avail || !online)
					pm.setItemEnabled(16, false);
				pm.insertSeparator();
			}
		}

		SimplePrivacyManager *spm = (SimplePrivacyManager *)d->pa->privacyManager();
		if(!spm->isContactBlocked(u->jid())) {
			pm.insertItem (tr("Block"), 27);
			if(!online)
				pm.setItemEnabled(27, false);
		} else {
			pm.insertItem (tr("Unblock"), 28);
			if(!online)
				pm.setItemEnabled(28, false);
		}

		if(inList && !PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
			Q3PopupMenu *authm = new Q3PopupMenu (&pm);

			authm->insertItem(tr("Resend authorization to"), 6);
			authm->insertItem(tr("Rerequest authorization from"), 11);
			authm->insertItem(/*IconsetFactory::iconPixmap("psi/edit/delete"),*/ tr("Remove authorization from"), 15);

			//pm.insertItem (IconsetFactory::iconPixmap("psi/register"), tr("Authorization"), authm, 20);
			pm.insertItem (tr("Authorization"), authm, 20);
			if(!online)
				pm.setItemEnabled(20, false);
		}

		if(!self) {
			if(groupNameCache != FAVNAME) {
			if(!PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
				if(online || !inList)
					d->cv->qa_rem->setEnabled(true);
				else
					d->cv->qa_rem->setEnabled(false);

				d->cv->qa_rem->addTo(&pm);
			}
			}
			pm.insertSeparator();
		}

		// Avatars
		if (PsiOptions::instance()->getOption("options.ui.menu.contact.custom-picture").toBool()) {
			Q3PopupMenu *avpm = new Q3PopupMenu(&pm);
			d->cv->qa_assignAvatar->addTo(avpm);
			d->cv->qa_clearAvatar->setEnabled(d->pa->avatarFactory()->hasManualAvatar(u->jid()));
			d->cv->qa_clearAvatar->addTo(avpm);
			pm.insertItem(tr("&Picture"), avpm);
		}

		/*if(PGPUtil::instance().pgpAvailable() && PsiOptions::instance()->getOption("options.ui.menu.contact.custom-pgp-key").toBool()) {
			if(u->publicKeyID().isEmpty())
				//pm.insertItem(IconsetFactory::icon("psi/gpg-yes").icon(), tr("Assign Open&PGP key"), 21);
				pm.insertItem(tr("Assign Open&PGP key"), 21);
			else
				//pm.insertItem(IconsetFactory::icon("psi/gpg-no").icon(), tr("Unassign Open&PGP key"), 22);
				pm.insertItem(tr("Unassign Open&PGP key"), 22);
		}*/

		d->cv->qa_vcard->addTo( &pm );

		if(!isPrivate) {
			d->cv->qa_hist->addTo(&pm);
		}

		QString name = u->jid().full();
		QString show = JIDUtil::nickOrJid(u->name(), u->jid().full());
		if(name != show)
			name += QString(" (%1)").arg(u->name());

		int x = pm.exec(pos);

		// restore actions
		d->cv->qa_logon->setEnabled(true);
		if(PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
			d->cv->qa_ren->setEnabled(false);
			d->cv->qa_rem->setEnabled(false);
		}
		else {
			d->cv->qa_ren->setEnabled(true);
			d->cv->qa_rem->setEnabled(true);
		}

		if(!qa_rem_origText.isEmpty())
			d->cv->qa_rem->setText(qa_rem_origText);

		if(x == -1)
			return;

		if(x == 0) {
			actionRecvEvent(u->jid());
		}
		else if(x == 1) {
			actionSendMessage(u->jid());
		}
		else if(x == 2) {
			actionSendUrl(u->jid());
		}
		else if(x == 6) {
			if(online) {
				actionAuth(u->jid());
				QMessageBox::information(d->cv, tr("Authorize"),
				tr("Sent authorization to <b>%1</b>.").arg(name));
			}
		}
		else if(x == 8) {
			if(online) {
				// if we have groups, but we are setting to 'none', then remove that particular group
				if(!u->groups().isEmpty()) {
					QString gname = groupNameCache;
					actionGroupRemove(u->jid(), gname);
				}
			}
		}
		else if(x == 9) {
			if(online) {
				while(1) {
					bool ok = false;
					QString newgroup = QInputDialog::getText(tr("Create New Group"), tr("Enter the new Group name:"), QLineEdit::Normal, QString::null, &ok, d->cv);
					if(!ok)
						break;
					if(newgroup.isEmpty())
						continue;

					// make sure we don't have it already
					bool found = false;
					const QStringList &groups = u->groups();
					for(QStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
						if(*it == newgroup) {
							found = true;
							break;
						}
					}
					if(!found) {
						QString gname = groupNameCache;
						actionGroupRemove(u->jid(), gname);
						actionGroupAdd(u->jid(), newgroup);
						break;
					}
				}
			}
		}
		else if(x == 10) {
			if(online) {
				actionAdd(u->jid());
				actionAuth(u->jid());

				QMessageBox::information(d->cv, tr("Add"),
				tr("Added/Authorized <b>%1</b> to the contact list.").arg(name));
			}
		}
		else if(x == 11) {
			if(online) {
				actionAuthRequest(u->jid());
				QMessageBox::information(d->cv, tr("Authorize"),
				tr("Rerequested authorization from <b>%1</b>.").arg(name));
			}
		}
		else if(x == 15) {
			if(online) {
				int n = QMessageBox::information(d->cv, tr("Remove"),
				tr("Are you sure you want to remove authorization from <b>%1</b>?").arg(name),
				tr("&Yes"), tr("&No"));

				if(n == 0)
					actionAuthRemove(u->jid());
			}
		}
		else if(x == 16) {
			if(online) {
				Status s=makeStatus(STATUS_OFFLINE,"");
				actionAgentSetStatus(u->jid(), s);
			}
		}
		else if(x == 21) {
			actionAssignKey(u->jid());
		}
		else if(x == 22) {
			actionUnassignKey(u->jid());
		}
		else if(x == 23) {
			if(online)
				actionSendFile(u->jid());
		}
		else if (x == 25) {
			if(online)
				actionExecuteCommand(u->jid(),"");
		}
		else if (x == 24) {
			if(online)
				actionVoice(u->jid());
		}
		else if (x == 26) {
			if(online) {
				QString newgroup = FAVNAME;

				// make sure we don't have it already
				bool found = false;
				const QStringList &groups = u->groups();
				for(QStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
					if(*it == newgroup) {
						found = true;
						break;
					}
				}
				if(!found) {
					actionGroupAdd(u->jid(), newgroup);
				}
			}
		}
		else if (x == 27) {
			// block
			spm->setContactBlocked(u->jid(), true);
		}
		else if (x == 28) {
			// unblock
			spm->setContactBlocked(u->jid(), false);
		}
		else if(x >= base_sendto && x < base_hidden) {
			int n = x - base_sendto;
#ifndef WHITEBOARDING
			int res = n / 3;
			int type = n % 3;
#else
			int res = n / 4;
			int type = n % 4;
#endif
			QString rname = "";
			//if(res > 0) {
				const UserResource &r = rl[res];
				rname = r.name();
			//}
			QString s = u->jid().userHost();
			if(!rname.isEmpty()) {
				s += '/';
				s += rname;
			}
			Jid j(s);

			if(type == 0)
				actionSendMessage(j);
			else if(type == 1)
				actionOpenChatSpecific(j);
#ifndef WHITEBOARDING
			else if (type == 2)
				actionExecuteCommandSpecific(j,"");
#else
			else if (type == 2)
				actionOpenWhiteboardSpecific(j);
			else if (type == 3)
				actionExecuteCommandSpecific(j,"");
#endif
		}
		else if(x >= base_hidden && x < base_gc) {
			int n = 0;
			int n2 = x - base_hidden;

			QString rname;
			for(QStringList::ConstIterator it = hc.begin(); it != hc.end(); ++it) {
				if(n == n2) {
					rname = *it;
					break;
				}
				++n;
			}

			QString s = u->jid().userHost();
			if(!rname.isEmpty()) {
				s += '/';
				s += rname;
			}
			Jid j(s);

			actionOpenChatSpecific(j);
		}
		else if(x >= base_gc && x < base_group) {
			/*if(online) {
				QString gc = groupchats[x - base_gc];
				actionInvite(u->jid(), gc);

				QMessageBox::information(d->cv, tr("Invitation"),
				tr("Sent groupchat invitation to <b>%1</b>.").arg(name));
			}*/
		}
		else if(x >= base_group) {
			if(online) {
				int n = 0;
				int n2 = x - base_group;

				QString newgroup;
				for(QStringList::Iterator it = gl.begin(); it != gl.end(); ++it) {
					if(n == n2) {
						newgroup = *it;
						break;
					}
					++n;
				}

				if(newgroup.isEmpty())
					newgroup = ContactView::tr("Hidden");

				if(n == n2) {
					// remove the group of this cvi if there is one
					if(!u->groups().isEmpty()) {
						//QString gname = ((ContactViewItemB *)static_cast<QListViewItem *>(i)->parent())->groupName();
						QString gname = groupNameCache;
						actionGroupRemove(u->jid(), gname);
					}
					// add the group
					actionGroupAdd(u->jid(), newgroup);
				}
			}
		}
	}
}

void ContactProfile::scActionDefault(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionDefault(i->u()->jid());
}

void ContactProfile::scRecvEvent(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionRecvEvent(i->u()->jid());
}

void ContactProfile::scSendMessage(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionSendMessage(i->u()->jid());
}

void ContactProfile::scRename(ContactViewItemB *i)
{
	if(!d->pa->loggedIn())
		return;

	if((i->type() == ContactViewItemB::Contact && i->u()->inList()) ||
		(i->type() == ContactViewItemB::Group && i->groupType() == ContactViewItemB::gUser && i->groupName() != ContactView::tr("Hidden"))) {
		d->rename_item = i;
		d->cv->setSelected(d->rename_item, false);
		QTimer::singleShot(0, d, SLOT(delayedRename()));
	}
}

void ContactProfile::scVCard(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionInfo(i->u()->jid());
}

void ContactProfile::scHistory(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionHistory(i->u()->jid());
}

void ContactProfile::scOpenChat(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionOpenChat(i->u()->jid());
}

#ifdef WHITEBOARDING
void ContactProfile::scOpenWhiteboard(ContactViewItemB *i)
{
	if(i->type() == ContactViewItemB::Contact)
		actionOpenWhiteboard(i->u()->jid());
}
#endif

void ContactProfile::scAgentSetStatus(ContactViewItemB *i, Status &s)
{
	if(i->type() != ContactViewItemB::Contact)
		return;
	if(!i->isAgent())
		return;

	if(i->u()->isAvailable() || !d->pa->loggedIn())
		return;

	actionAgentSetStatus(i->u()->jid(), s);
}

void ContactProfile::scRemove(ContactViewItemB *i)
{
	if(i->type() != ContactViewItemB::Contact)
		return;

	Entry *e = findEntry(i);
	if(!e)
		return;

	bool ok = true;
	if(!d->pa->loggedIn())
		ok = false;
	if(!i->u()->inList())
		ok = true;

	if(ok) {
		QString groupNameCache;
		if(static_cast<Q3ListViewItem *>(i)->parent())
			groupNameCache = ((ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent())->groupName();

		if(groupNameCache == FAVNAME) {
			UserListItem *u = &e->u;
			actionGroupRemove(u->jid(), groupNameCache);
			return;
		}
	}

	if(ok) {
		QString name = e->u.jid().full();
		QString show = JIDUtil::nickOrJid(e->u.name(), e->u.jid().full());
		if(name != show)
			name += QString(" (%1)").arg(e->u.name());

		int n = 0;
		int gt = i->parentGroupType();
		if(gt != ContactViewItemB::gNotInList && gt != ContactViewItemB::gPrivate) {
			n = QMessageBox::information(d->cv, tr("Remove"),
			tr("Are you sure you want to remove <b>%1</b> from your contact list?").arg(name),
			tr("&Yes"), tr("&No"));
		}
		else
			n = 0;

		if(n == 0) {
			Jid j = e->u.jid();
			removeEntry(e);
			actionRemove(j);
		}
	}
}

void ContactProfile::doItemRenamed(ContactViewItemB *i, const QString &text)
{
	if(i->type() == ContactViewItemB::Contact) {
		Entry *e = findEntry(i);
		if(!e)
			return;

		// no change?
		//if(text == i->text(0))
		//	return;
		if(text.isEmpty()) {
			i->resetName();
			QMessageBox::information(d->cv, tr("Error"), tr("You can't set a blank name."));
			return;
		}

		//e->u.setName(text);
		//i->setContact(&e->u);
		actionRename(e->u.jid(), text);
		i->resetName(); // to put the status message in if needed
	}
	else {
		// no change?
		if(text == i->groupName()) {
			i->resetGroupName();
			return;
		}
		if(text.isEmpty()) {
			i->resetGroupName();
			QMessageBox::information(d->cv, tr("Error"), tr("You can't set a blank group name."));
			return;
		}

		// make sure we don't have it already
		QStringList g = groupList();
		bool found = false;
		for(QStringList::ConstIterator it = g.begin(); it != g.end(); ++it) {
			if(*it == text) {
				found = true;
				break;
			}
		}
		if(found) {
			i->resetGroupName();
			QMessageBox::information(d->cv, tr("Error"), tr("You already have a group with that name."));
			return;
		}

		QString oldName = i->groupName();

		// set group name
		i->setGroupName(text);

		// send signal
		actionGroupRename(oldName, text);
	}
}

void ContactProfile::dragDrop(const QString &text, ContactViewItemB *i)
{
	if(!d->pa->loggedIn())
		return;

	// get group
	ContactViewItemB *gr;
	if(i->type() == ContactViewItemB::Group)
		gr = i;
	else
		gr = (ContactViewItemB *)static_cast<Q3ListViewItem *>(i)->parent();

	Jid j(text);
	if(!j.isValid())
		return;
	Entry *e = findEntry(j);
	if(!e)
		return;
	const UserListItem &u = e->u;
	QStringList gl = u.groups();

	// already in the general group
	if(gr->groupType() == ContactViewItemB::gGeneral && gl.isEmpty())
		return;
	// already in this user group
	if(gr->groupType() == ContactViewItemB::gUser && u.inGroup(gr->groupName()))
		return;

	//printf("putting [%s] into group [%s]\n", u.jid().full().latin1(), gr->groupName().latin1());

	// remove all other groups from this contact
	for(QStringList::ConstIterator it = gl.begin(); it != gl.end(); ++it) {
		actionGroupRemove(u.jid(), *it);
	}
	if(gr->groupType() == ContactViewItemB::gUser) {
		// add the new group
		actionGroupAdd(u.jid(), gr->groupName());
	}
}

void ContactProfile::dragDropFiles(const QStringList &files, ContactViewItemB *i)
{
	if(files.isEmpty() || !d->pa->loggedIn() || i->type() != ContactViewItemB::Contact)
		return;

	Entry *e = findEntry(i);
	if(!e)
		return;

	actionSendFiles(e->u.jid(),files);
}

//----------------------------------------------------------------------------
// ContactView
//----------------------------------------------------------------------------
class ContactView::Private : public QObject
{
	Q_OBJECT
public:
	Private(ContactView *_cv)
		: QObject(_cv)
	{
		cv = _cv;
		autoRosterResizeInProgress = false;
		selected = 0;

		// type ahead
		typeAheadTimer = new QTimer(cv);
		connect(typeAheadTimer, SIGNAL(timeout()), SLOT(resetTypeAhead()));
	}

	ContactView *cv;
	QTimer *animTimer, *recalculateSizeTimer;
	Q3PtrList<ContactProfile> profiles;
	QString typeAhead;
	QTimer *typeAheadTimer;
	QSize lastSize;
	bool autoRosterResizeInProgress;
	Q3ListViewItem *selected;

	// ###cuda
	QList<Jid> checkedVCardJids;

	bool doTypeAhead(QKeyEvent *e)
	{
		typeAheadTimer->start(3000, TRUE); // reset type ahead in 3 seconds of inactivity

		QString text = e->text().lower();
		if ( text.isEmpty() ) {
			// Shift key could be used for entering '@', '#' and other extended chars
			// and other modifier keys could be used for layout changing

			//if ( e->key() != Qt::Key_Shift )
			//	typeAhead = QString::null;
			return false;
		}

		bool searchFromStart = true;
		if ( typeAhead.isEmpty() )
			searchFromStart = false;

		bool found = false;
		typeAhead += text;

		Q3ListViewItemIterator it(cv);
		ContactViewItemB *item;

		if ( !searchFromStart && cv->currentItem() ) {
			for ( ; (item = (ContactViewItemB *)it.current()); ++it) {
				if ( item == cv->currentItem() ) {
					++it; // we want the search to start from next contact
					break;
				}
			}
		}

		for ( ; (item = (ContactViewItemB *)it.current()); ++it) {
			//qWarning("->%s | %s | %s", text.latin1(), item->text(0).lower().latin1(), typeAhead.latin1());
			if ( item->text(0).lower().startsWith(typeAhead) ) {
				found = true;
				cv->setSelected(item, true);
				cv->ensureItemVisible(item);
				item->optionsUpdate();
				item->repaint();
				break;
			}
		}

		//qWarning("---------------\n");
		if ( !found ) {
			typeAhead = text;
			return false;
		}

		return true;
	}

	void doSearch(const QString &text)
	{
		QString typeAhead = text.toLower();
		bool found = false;

		Q3ListViewItemIterator it(cv);
		ContactViewItemB *item;

		for ( ; (item = (ContactViewItemB *)it.current()); ++it) {
			//qWarning("->%s | %s | %s", text.latin1(), item->text(0).lower().latin1(), typeAhead.latin1());
			if ( item->text(0).toLower().startsWith(typeAhead) ) {
				found = true;
				cv->setSelected(item, true);
				cv->ensureItemVisible(item);
				item->optionsUpdate();
				item->repaint();
				break;
			}
		}
	}

public slots:
	/*
	 * \brief Recalculates the size of ContactView and resizes it accordingly
	 */
	void recalculateSize()
	{
		// save some CPU
		if ( !cv->allowResize() )
			return;

		if ( !cv->isUpdatesEnabled() )
			return;

		QSize oldSize = cv->size();
		QSize newSize = cv->sizeHint();

		if ( newSize.height() != oldSize.height() ) {
			lastSize = newSize;
			QWidget *topParent = cv->window();

			if ( cv->allowResize() ) {
				topParent->layout()->setEnabled( false ); // try to reduce some flicker

				int dh = newSize.height() - oldSize.height();
				topParent->resize( topParent->width(),
						   topParent->height() + dh );

				autoRosterResizeInProgress = true;

				QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
				if ( PsiOptions::instance()->getOption("options.ui.contactlist.grow-roster-upwards").toBool() ) {
					int newy = topParent->y() - dh;

					// check, if we need to move roster lower
					if ( dh > 0 && ( topParent->frameGeometry().top() <= desktop.top() ) ) {
						newy = desktop.top();
					}

					topParent->move( topParent->x(), newy );
				}

				// check, if we need to move roster upper
				if ( dh > 0 && ( topParent->frameGeometry().bottom() >= desktop.bottom() ) ) {
					int newy = desktop.bottom() - topParent->frameGeometry().height();
					topParent->move( topParent->x(), newy );
				}

				QTimer::singleShot( 0, this, SLOT( resetAutoRosterResize() ) );

				topParent->layout()->setEnabled( true );
			}

			// issue a layout update
			cv->parentWidget()->layout()->update();
		}
	}

	/*
	 * \brief Determine in which direction to grow Psi roster window when autoRosterSize is enabled
	 */
	void determineAutoRosterSizeGrowSide()
	{
		if ( autoRosterResizeInProgress )
			return;

		QWidget *topParent = cv->window();
		QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );

		int top_offs    = abs( desktop.top()    - topParent->frameGeometry().top() );
		int bottom_offs = abs( desktop.bottom() - topParent->frameGeometry().bottom() );

		PsiOptions::instance()->setOption("options.ui.contactlist.grow-roster-upwards", (bool) (bottom_offs < top_offs));
		//qWarning("growTop = %d", PsiOptions::instance()->getOption("options.ui.contactlist.automatically-resize-roster").toBool()GrowTop);
	}
	
	/*
	 * \brief Display tool tip for item under cursor.
	 */
	bool doToolTip(QPoint pos)
	{
		ContactViewItemB *i = (ContactViewItemB *)cv->itemAt(pos);
		if(i) {
			QRect r(cv->itemRect(i));
			QPoint globalPos = cv->mapToGlobal(pos);
			if(i->type() == ContactViewItemB::Contact)
				PsiToolTip::showText(globalPos, i->u()->makeTip(true, false), cv);
			else if(i->type() == ContactViewItemB::Profile)
				PsiToolTip::showText(globalPos, i->contactProfile()->makeTip(true, false), cv);
			else
				PsiToolTip::showText(globalPos, i->groupName() + " " + i->groupInfo(), cv);
			return true;
		}
		return false;
	}

	// ###cuda
	void ensureVCard(PsiAccount *pa, const Jid &_j)
	{
		Jid j = _j.bare();

		bool found = false;
		foreach(const Jid &i, checkedVCardJids) {
			if(i.compare(j)) {
				found = true;
				break;
			}
		}

		if(!found) {
			checkedVCardJids += j;
			VCardFactory::instance()->getVCard(j, pa->client()->rootTask(), this, SLOT(vcardFinished()), true);
		}
	}

private slots:
	void resetAutoRosterResize()
	{
		//qWarning("resetAutoRosterResize");
		autoRosterResizeInProgress = false;
	}

	void resetTypeAhead()
	{
		typeAhead = QString::null;
	}

	// ###cuda
	void vcardFinished()
	{
		// nothing
	}
};

ContactView::ContactView(QWidget *parent, const char *name)
:Q3ListView(parent, name, Qt::WNoAutoErase | Qt::WResizeNoErase)
{
	d = new Private(this);

	// setup the QListView
	setAllColumnsShowFocus(true);
	setShowToolTips(false);
	setHScrollBarMode(AlwaysOff);
	setMinimumSize(96,32);
	setTreeStepSize(0);
	setSorting(0,true);

	window()->installEventFilter( this );

	// create the column and hide the header
	addColumn("");
	header()->hide();

	setResizeMode(Q3ListView::LastColumn);
	setDefaultRenameAction(Q3ListView::Accept);

	// catch signals
	lcto_active = false;
	connect(this, SIGNAL(itemRenamed(Q3ListViewItem *, int, const QString &)), SLOT(qlv_itemRenamed(Q3ListViewItem *, int, const QString &)));
	connect(this, SIGNAL(mouseButtonPressed(int, Q3ListViewItem *, const QPoint &, int)),SLOT(qlv_singleclick(int, Q3ListViewItem *, const QPoint &, int)) );
	connect(this, SIGNAL(doubleClicked(Q3ListViewItem *)),SLOT(qlv_doubleclick(Q3ListViewItem *)) );
	connect(this, SIGNAL(contextMenuRequested(Q3ListViewItem *, const QPoint &, int)), SLOT(qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int)));
	connect(this, SIGNAL(selectionChanged()), SLOT(qlv_selectionChanged()));

	v_showOffline = PsiOptions::instance()->getOption("options.ui.contactlist.show.offline-contacts").toBool();
	v_showAway = PsiOptions::instance()->getOption("options.ui.contactlist.show.away-contacts").toBool();
	v_showHidden = PsiOptions::instance()->getOption("options.ui.contactlist.show.hidden-contacts-group").toBool();
	v_showAgents = PsiOptions::instance()->getOption("options.ui.contactlist.show.agent-contacts").toBool();
	v_showSelf = PsiOptions::instance()->getOption("options.ui.contactlist.show.self-contact").toBool();
	v_showStatusMsg = PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.show").toBool();


	d->lastSize = QSize( 0, 0 );

	// animation timer
	d->animTimer = new QTimer(this);
	d->animTimer->start(120 * 5);

	d->recalculateSizeTimer = new QTimer(this);
	connect(d->recalculateSizeTimer, SIGNAL(timeout()), d, SLOT(recalculateSize()));

	// actions
	//qa_send = new IconAction("", "psi/sendMessage", tr("Send &message"), 0, this);
	qa_send = new IconAction("", tr("Send &message"), 0, this);
	connect(qa_send, SIGNAL(activated()), SLOT(doSendMessage()));
	qa_ren = new IconAction("", /*"psi/edit/clear",*/ tr("Re&name"), 0, this);
	connect(qa_ren, SIGNAL(activated()), SLOT(doRename()));
	qa_assignAvatar = new IconAction("", tr("&Assign Custom Picture"), 0, this);
	connect(qa_assignAvatar, SIGNAL(activated()), SLOT(doAssignAvatar()));
	qa_clearAvatar = new IconAction("", tr("&Clear Custom Picture"), 0, this);
	connect(qa_clearAvatar, SIGNAL(activated()), SLOT(doClearAvatar()));
	//qa_chat = new IconAction("", "psi/start-chat", tr("Open &chat window"), 0, this);
	qa_chat = new IconAction("", tr("Open &chat window"), 0, this);
	connect(qa_chat, SIGNAL(activated()), SLOT(doOpenChat()));
#ifdef WHITEBOARDING
	//qa_wb = new IconAction("", "psi/whiteboard", tr("Open a &whiteboard"), Qt::CTRL+Qt::Key_W, this);
	qa_wb = new IconAction("", tr("Open a &whiteboard"), Qt::CTRL+Qt::Key_W, this);
	connect(qa_wb, SIGNAL(activated()), SLOT(doOpenWhiteboard()));
#endif
	//qa_hist = new IconAction("", "psi/history", tr("&History"), 0, this);
	qa_hist = new IconAction("", tr("&History"), 0, this);
	connect(qa_hist, SIGNAL(activated()), SLOT(doHistory()));
	qa_logon = new IconAction("", tr("&Log on"), 0, this);
	connect(qa_logon, SIGNAL(activated()), SLOT(doLogon()));
	qa_recv = new IconAction("", tr("&Receive incoming event"), 0, this);
	connect(qa_recv, SIGNAL(activated()), SLOT(doRecvEvent()));
	//qa_rem = new IconAction("", "psi/remove", tr("Rem&ove"), 0, this);
	qa_rem = new IconAction("", tr("Rem&ove"), 0, this);
	connect(qa_rem, SIGNAL(activated()), SLOT(doRemove()));
	//qa_vcard = new IconAction("", "psi/vCard", tr("User &Info"), 0, this);
	qa_vcard = new IconAction("", tr("User &Info"), 0, this);
	connect(qa_vcard, SIGNAL(activated()), SLOT(doVCard()));

	if(PsiOptions::instance()->getOption("options.ui.contactlist.lockdown-roster").toBool()) {
		qa_ren->setEnabled(false);
		qa_rem->setEnabled(false);
	}

	optionsUpdate();
	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);
	
	filterString_ = QString();
}

ContactView::~ContactView()
{
	clear();
	delete d;
}

bool ContactView::filterContact(ContactViewItemB *item, bool refineSearch)
{
	if (!item) {
		return false;
	}
	if (item->type() != ContactViewItemB::Contact) {
		return false;
	}
	if (filterString_.isNull()) {
		return true;	
	}
	//if refineSearch, only search still visible items
	if (refineSearch && !item->isVisible()) {
		return false;		
	}
	if (TextUtil::rich2plain(item->text(0)).contains(filterString_,Qt::CaseInsensitive) || item->u()->jid().node().contains(filterString_,Qt::CaseInsensitive))
	{
		ensureItemVisible(item);
		item->setVisible(true);
		item->optionsUpdate();			
	}
	else
	{
		item->setVisible(false);
	}
	item->repaint();	
	return item->isVisible();
}

bool ContactView::filterGroup(ContactViewItemB *group, bool refineSearch)
{
	if (!group) {
		return false;	
	} else if (group->type() != ContactViewItemB::Group) {
		return false;
	} else if (filterString_.isNull()) {
		return true;
	}
	//if refine_search, only search still visible items
	if (refineSearch && !group->isVisible()) {
		return false;	
	}
	group->setVisible(true); //if not refined search
	
	//iterate over children
	Q3ListViewItemIterator it(group);
	bool groupContainsAFinding = false;
	ContactViewItemB *item = static_cast<ContactViewItemB*>(group->firstChild());
	while(item) {
		if (filterContact(item,refineSearch))
			groupContainsAFinding = true;
        item = static_cast<ContactViewItemB*>(item->nextSibling());
	}
	if (groupContainsAFinding == false) {
		group->setVisible(false);		
	}
	group->repaint();
	return groupContainsAFinding;
}

void ContactView::setFilter(QString const &text)
{
	bool refineSearch = text.startsWith(filterString_);
	filterString_ = text;
	
	Q3ListViewItemIterator it(d->cv);
	for (ContactViewItemB *item; (item = (ContactViewItemB *)it.current()); ++it)
	{	
		if (item->type() == ContactViewItemB::Group) {
			filterGroup(item, refineSearch);
		}
	}
}

void ContactView::clearFilter()
{
	// no filter set?  do nothing then
	if(filterString_.isEmpty())
		return;

	filterString_=QString();
	Q3ListViewItemIterator it(d->cv);
	for (ContactViewItemB *item; (item = (ContactViewItemB *)it.current()); ++it) 
	{
		if (item->type() != ContactViewItemB::Contact && item->type() != ContactViewItemB::Group) {
			continue;
		}
		item->setVisible(true);
		item->optionsUpdate();
		item->repaint();
	}	
}


QTimer *ContactView::animTimer() const
{
	return d->animTimer;
}

void ContactView::clear()
{
	d->profiles.setAutoDelete(true);
	d->profiles.clear();
	d->profiles.setAutoDelete(false);
}

void ContactView::link(ContactProfile *cp)
{
	d->profiles.append(cp);
}

void ContactView::unlink(ContactProfile *cp)
{
	d->profiles.removeRef(cp);
}

void ContactView::keyPressEvent(QKeyEvent *e)
{
	int key = e->key();
	if(key == Qt::Key_Enter || key == Qt::Key_Return) {
		doEnter();
	} else if(key == Qt::Key_Space /*&& d->typeAhead.isEmpty()*/) {
		doContext();
	} else if (key == Qt::Key_Home   || key == Qt::Key_End      ||
		 key == Qt::Key_PageUp || key == Qt::Key_PageDown ||
		 key == Qt::Key_Up     || key == Qt::Key_Down     ||
		 key == Qt::Key_Left   || key == Qt::Key_Right) {

		d->typeAhead = QString::null;
		Q3ListView::keyPressEvent(e);
#ifdef Q_WS_MAC
	} else if (e->modifiers() == Qt::ControlModifier) {
		Q3ListView::keyPressEvent(e);
#endif 
	} else {
		if (!d->doTypeAhead(e))
			Q3ListView::keyPressEvent(e);
		/*QString text = e->text().lower();
		if (text.isEmpty()) {
			Q3ListView::keyPressEvent(e);
		}
		else {
			bool printable = true;
			foreach (QChar ch, text) {
				if (!ch.isPrint()) {
					Q3ListView::keyPressEvent(e);
					printable = false;
					break;
				}
			}
			if (printable) {
				emit searchInput(text);
			}
		}*/
	}
}

void ContactView::setShowOffline(bool x)
{
	bool oldstate = v_showOffline;
	v_showOffline = x;
	PsiOptions::instance()->setOption("options.ui.contactlist.show.offline-contacts", x);


	if(v_showOffline != oldstate) {
		showOffline(v_showOffline);

		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			if(!v_showOffline)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowAway(bool x)
{
	bool oldstate = v_showAway;
	v_showAway = x;
	PsiOptions::instance()->setOption("options.ui.contactlist.show.away-contacts", x);
		
	if(v_showAway != oldstate) {
		showAway(v_showAway);

		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			if(!v_showAway)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowHidden(bool x)
{
	bool oldstate = v_showHidden;
	v_showHidden = x;
	PsiOptions::instance()->setOption("options.ui.contactlist.show.hidden-contacts-group", x);

	if(v_showHidden != oldstate) {
		showHidden(v_showHidden);

		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			if(!v_showHidden)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

void ContactView::setShowStatusMsg(bool x)
{
	if (v_showStatusMsg != x) {
		v_showStatusMsg = x;
		PsiOptions::instance()->setOption("options.ui.contactlist.status-messages.show",x);
		emit showStatusMsg(v_showStatusMsg);
		
		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			cp->resetAllContactItemNames();
		}
		
		recalculateSize();
	}
}

/*
 * \brief Shows/hides the self contact in roster
 *
 * \param x - boolean variable specifies whether to show self-contact or not
 */
void ContactView::setShowSelf(bool x)
{
	if (v_showSelf != x) {
		v_showSelf = x;
		PsiOptions::instance()->setOption("options.ui.contactlist.show.self-contact", x);
		showSelf(v_showSelf);

		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			if (v_showSelf && ! cp->self()) {
				cp->addSelf();
			}
			else if (!v_showSelf && cp->self() && cp->self()->u()->userResourceList().count() <= 1) {
				cp->removeSelf();
			}
		}

		recalculateSize();
	}
}

/*
 * \brief Event filter. Nuff said.
 */
bool ContactView::eventFilter( QObject *obj, QEvent *event )
{
	if ( event->type() == QEvent::Move )
		d->determineAutoRosterSizeGrowSide();
	/*else if (event->type() == QEvent::ToolTip && obj == viewport()) {
		if (d->doToolTip(((QHelpEvent*)event)->pos())) {
			event->accept();
			return true;
		}
	}*/

	return Q3ListView::eventFilter( obj, event );
}


void ContactView::setShowAgents(bool x)
{
	bool oldstate = v_showAgents;
	v_showAgents = x;
	PsiOptions::instance()->setOption("options.ui.contactlist.show.agent-contacts", x);

	if(v_showAgents != oldstate) {
		showAgents(v_showAgents);

		Q3PtrListIterator<ContactProfile> it(d->profiles);
		for(ContactProfile *cp; (cp = it.current()); ++it) {
			if(!v_showAgents)
				cp->removeAllUnneededContactItems();
			else
				cp->addAllNeededContactItems();
		}
	}
}

// right-click context menu
void ContactView::qlv_contextMenuRequested(Q3ListViewItem *lvi, const QPoint &pos, int c)
{
	if(PsiOptions::instance()->getOption("options.ui.contactlist.use-left-click").toBool())
		return;

	qlv_contextPopup(lvi, pos, c);
}

void ContactView::qlv_contextPopup(Q3ListViewItem *lvi, const QPoint &pos, int)
{
	ContactViewItemB *i = (ContactViewItemB *)lvi;
	if(!i)
		return;

	// ###cuda this used to be here to disable right-click
	//if(i->type() == ContactViewItemB::Contact)
	//	return;

	i->contactProfile()->doContextMenu(i, pos);
}

void ContactView::qlv_singleclick(int button, Q3ListViewItem *i, const QPoint &pos, int c)
{
	lcto_active = false;

	if(!i)
		return;

	ContactViewItemB *item = (ContactViewItemB *)i;
	setSelected(item, true);

	if(button == Qt::MidButton) {
		if(item->type() == ContactViewItemB::Contact)
			item->contactProfile()->scActionDefault(item);
	}
	else {
		const QPixmap * pix = item->pixmap(0);			
		if (button == Qt::LeftButton && item->type() == ContactViewItemB::Group && pix && viewport()->mapFromGlobal(pos).x() <= pix->width() + treeStepSize()) {
			setOpen(item, !item->isOpen());
		}
		else if(PsiOptions::instance()->getOption("options.ui.contactlist.use-left-click").toBool()) {
			if(button == Qt::LeftButton) {
				if(PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool()) {
					qlv_contextPopup(i, pos, c);
				}
				else {
					lcto_active = true;
					lcto_pos = pos;
					lcto_item = i;
					QTimer::singleShot(QApplication::doubleClickInterval()/2, this, SLOT(leftClickTimeOut()));
				}
			}
			else if(PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool() && button == Qt::RightButton) {
				if(item->type() == ContactViewItemB::Contact)
					item->contactProfile()->scActionDefault(item);
			}
		}
		else {
			//if(button == QListView::RightButton) {
			//	qlv_contextPopup(i, pos, c);
			//}
			/*else*/if(button == Qt::LeftButton && PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool()) {
				if(item->type() == ContactViewItemB::Contact)
					item->contactProfile()->scActionDefault(item);
			}
			else if(button == Qt::LeftButton)
			{
				QPoint p = viewport()->mapFromGlobal(pos);
				p = viewportToContents(p);
				item->clicked(QPoint(p.x(), p.y() - item->itemPos()), pos);
			}
		}
	}

	d->typeAhead = "";
}

void ContactView::qlv_doubleclick(Q3ListViewItem *i)
{
	lcto_active = false;

	if(!i)
		return;

	if(PsiOptions::instance()->getOption("options.ui.contactlist.use-single-click").toBool())
		return;

	ContactViewItemB *item = (ContactViewItemB *)i;
	item->contactProfile()->scActionDefault(item);

	d->typeAhead = "";
}

void ContactView::qlv_itemRenamed(Q3ListViewItem *lvi, int, const QString &text)
{
	ContactViewItemB *i = (ContactViewItemB *)lvi;
	i->contactProfile()->doItemRenamed(i, text);
}

void ContactView::qlv_selectionChanged()
{
	Q3ListViewItem *i = selectedItem();
	if(d->selected == i)
		return;
	if(d->selected)
		closeSubArea((ContactViewItemB *)d->selected);
	d->selected = i;
	if(d->selected) {
		ContactViewItemB *cvi = (ContactViewItemB *)d->selected;
		openSubArea(cvi);
		if(cvi->type() == ContactViewItemB::Contact)
			d->ensureVCard(cvi->contactProfile()->psiAccount(), cvi->u()->jid());
	}
}

void ContactView::leftClickTimeOut()
{
	if(lcto_active) {
		qlv_contextPopup(lcto_item, lcto_pos, 0);
		lcto_active = false;
	}
}

void ContactView::optionsUpdate()
{
	// set the font
	QFont f;
	f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.contactlist").toString());
	Q3ListView::setFont(f);

	// set the text and background colors
	QPalette mypal = Q3ListView::palette();
	mypal.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.online").value<QColor>());
	mypal.setColor(QColorGroup::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.background").value<QColor>());
	Q3ListView::setPalette(mypal);

	// reload the icons
	Q3ListViewItemIterator it(this);
	ContactViewItemB *item;
	for(; it.current() ; ++it) {
		item = (ContactViewItemB *)it.current();
		item->optionsUpdate();
	}

	// shortcuts
	setShortcuts();

	// resize if necessary
	if (PsiOptions::instance()->getOption("options.ui.contactlist.automatically-resize-roster").toBool())
		recalculateSize();

	update();
}

void ContactView::setShortcuts()
{
	qa_send->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.message"));
	qa_ren->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.rename"));
	qa_assignAvatar->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.assign-custom-avatar"));
	qa_clearAvatar->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.clear-custom-avatar"));
	qa_chat->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.chat"));
	qa_hist->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));
	qa_logon->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.login-transport"));
	qa_recv->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.event"));
	qa_rem->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
	qa_vcard->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
}

void ContactView::resetAnim()
{
	for(Q3ListViewItemIterator it(this); it.current() ; ++it) {
		ContactViewItemB *item = (ContactViewItemB *)it.current();
		if(item->isAlerting())
			item->resetAnim();
	}
}

void ContactView::doRecvEvent()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scRecvEvent(i);
}

void ContactView::doRename()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scRename(i);
}

void ContactView::doAssignAvatar()
{
	// FIXME: Should check the supported filetypes dynamically
	QString file = QFileDialog::getOpenFileName(this, tr("Choose an image"), "", tr("All files (*.png *.jpg *.gif)"));
	if (!file.isNull()) {
		ContactViewItemB *i = (ContactViewItemB *)selectedItem();
		i->contactProfile()->psiAccount()->avatarFactory()->importManualAvatar(i->u()->jid(),file);
	}
}

void ContactView::doClearAvatar()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	i->contactProfile()->psiAccount()->avatarFactory()->removeManualAvatar(i->u()->jid());
}

void ContactView::doEnter()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scActionDefault(i);
}

void ContactView::doContext()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	ensureItemVisible(i);

	if(i->type() == ContactViewItemB::Group)
		setOpen(i, !i->isOpen());
	else
		qlv_contextPopup(i, viewport()->mapToGlobal(QPoint(32, itemPos(i))), 0);
}

void ContactView::doSendMessage()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scSendMessage(i);
}

void ContactView::doOpenChat()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scOpenChat(i);
}

#ifdef WHITEBOARDING
void ContactView::doOpenWhiteboard()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scOpenWhiteboard(i);
}
#endif

void ContactView::doHistory()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scHistory(i);
}

void ContactView::doVCard()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scVCard(i);
}

void ContactView::doLogon()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	Status s=i->contactProfile()->psiAccount()->status();
	i->contactProfile()->scAgentSetStatus(i, s);
}

void ContactView::doRemove()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return;
	i->contactProfile()->scRemove(i);
}

Q3DragObject *ContactView::dragObject()
{
	ContactViewItemB *i = (ContactViewItemB *)selectedItem();
	if(!i)
		return 0;
	if(i->type() != ContactViewItemB::Contact)
		return 0;

	Q3DragObject *d = new Q3TextDrag(i->u()->jid().full(), this);
	d->setPixmap(IconsetFactory::iconPixmap("status/online"), QPoint(8,8));
	return d;
}

bool ContactView::allowResize() const
{
	if ( !PsiOptions::instance()->getOption("options.ui.contactlist.automatically-resize-roster").toBool() )
		return false;

	if ( window()->isMaximized() )
		return false;

	return true;
}

QSize ContactView::minimumSizeHint() const
{
	return QSize( minimumWidth(), minimumHeight() );
}

QSize ContactView::sizeHint() const
{
	// save some CPU
	if ( !allowResize() )
		return minimumSizeHint();

	QSize s( Q3ListView::sizeHint().width(), 0 );
	int border = 5;
	int h = border;

	Q3ListView *listView = (Q3ListView *)this;
	Q3ListViewItemIterator it( listView );
	while ( it.current() ) {
		if ( it.current()->isVisible() ) {
			// also we need to check whether the group is open or closed
			bool show = true;
			Q3ListViewItem *item = it.current()->parent();
			while ( item ) {
				if ( !item->isOpen() ) {
					show = false;
					break;
				}
				item = item->parent();
			}

			if ( show )
				h += it.current()->height();
		}

		++it;
	}

	QWidget *topParent = window();
	QRect desktop = qApp->desktop()->availableGeometry( (QWidget *)topParent );
	int dh = h - d->lastSize.height();

	// check that our dialog's height doesn't exceed the desktop's
	if ( allowResize() && dh > 0 && (topParent->frameGeometry().height() + dh) >= desktop.height() ) {
		h = desktop.height() - ( topParent->frameGeometry().height() - d->lastSize.height() );
	}

	int minH = minimumSizeHint().height();
	if ( h < minH )
		h = minH + border;
	s.setHeight( h );
	return s;
}

/*
 * \brief Adds the request to recalculate the ContactView size to the event queue
 */
void ContactView::recalculateSize()
{
	d->recalculateSizeTimer->start( 0, true );
}

void ContactView::openSubArea(ContactViewItemB *i)
{
	i->expandArea();
}

void ContactView::closeSubArea(ContactViewItemB *i)
{
	i->closeArea();
}

void ContactView::setSearch(const QString &str)
{
	if(!str.isEmpty())
		d->doSearch(str);
}

//------------------------------------------------------------------------------
// RichListViewItemB: A RichText listview item
//------------------------------------------------------------------------------

#include <q3simplerichtext.h>
#include <qpainter.h>

static const int icon_vpadding = 2;

RichListViewStyleSheet::RichListViewStyleSheet(QObject* parent, const char * name) : Q3StyleSheet(parent, name)
{
}


RichListViewStyleSheet* RichListViewStyleSheet::instance()
{
	if (!instance_)
		instance_ = new RichListViewStyleSheet();
	return instance_;
}

void RichListViewStyleSheet::scaleFont(QFont& font, int logicalSize) const
{
	int size = font.pointSize() + (logicalSize - 3)*2;
	if (size > 0)
		font.setPointSize(size);
	else
		font.setPointSize(1);
}

RichListViewStyleSheet* RichListViewStyleSheet::instance_ = 0;


RichListViewItemB::RichListViewItemB( Q3ListView * parent ) : Q3ListViewItem(parent)
{
	indent = 6;
	v_rt = 0;
	v_active = v_selected = false;
	v_rich = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();
}

RichListViewItemB::RichListViewItemB( Q3ListViewItem * parent ) : Q3ListViewItem(parent)
{
	indent = 6;
	v_rt = 0;
	v_active = v_selected = false;
	v_rich = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();
}

RichListViewItemB::~RichListViewItemB()
{
	delete v_rt;
}
	
void RichListViewItemB::setText(int column, const QString& text)
{
	Q3ListViewItem::setText(column, text);
	setup();
}

void RichListViewItemB::setup()
{
	Q3ListViewItem::setup();

	// ###cuda
	const Q3ListView* lv = listView();
	v_active = lv->isActiveWindow();
	v_selected = isSelected();

	/*if (v_rich) {
		int h = height();
		QString txt = text(0);
		if( txt.isEmpty() ){
			delete v_rt;
			v_rt = 0;
			return;
		}
		
		const Q3ListView* lv = listView();
		const QPixmap* px = pixmap(0);
		int left =  lv->itemMargin() + ((px)?(px->width() + lv->itemMargin()):0);

		v_active = lv->isActiveWindow();
		v_selected = isSelected();

		if ( v_selected  ) {
			txt = QString("<font color=\"%1\">").arg(listView()->colorGroup().color( QColorGroup::HighlightedText ).name()) + txt + "</font>";
		}
		
		if(v_rt)
			delete v_rt;
		v_rt = new Q3SimpleRichText(txt, lv->font(), QString::null, RichListViewStyleSheet::instance());
		
		v_rt->setWidth(lv->columnWidth(0) - left - depth() * lv->treeStepSize());

		v_widthUsed = v_rt->widthUsed() + left;

		h = QMAX( h, v_rt->height() );

		if ( h % 2 > 0 )
			h++;

		setHeight( h );
	}*/
}

void RichListViewItemB::paintCell(QPainter *p, const QColorGroup &cg, int column, int width, int align)
{
	//if(!v_rt){
	//	Q3ListViewItem::paintCell(p, cg, column, width, align);
	//	return;
	//}

	p->save();

	Q3ListView* lv = listView();

	if ( isSelected() != v_selected || lv->isActiveWindow() != v_active) 
		setup();
	
	int r = indent; //int r = lv->itemMargin();
	
	const QBrush *paper;
	// setup (colors, sizes, ...)
	if ( isSelected() ) {
		paper = new QBrush(cg.brush( QColorGroup::Highlight ));
	}
	else{
#if QT_VERSION >= 0x040103 
		paper = new QBrush(cg.background(), Qt::NoBrush);
#else
		paper = new QBrush(cg.brush( QColorGroup::WindowText ));
#endif
	}
	
	const QPixmap * px = pixmap( column );
	QRect pxrect;
	int pxw = 0;
	int pxh = 0;
	if(px) {
		pxw = px->width();
		pxh = px->height();
		pxrect = QRect(r, icon_vpadding, pxw, pxh);
		//r += pxw + lv->itemMargin();
		r += pxw + 10;
	}

	if(px)
		pxrect.moveTop((height() - pxh)/2);

	// start drawing
	/*QRect rtrect(r, (height() - v_rt->height())/2, v_widthUsed, v_rt->height());
	v_rt->draw(p, rtrect.left(), rtrect.top(), rtrect, cg, paper);
	
	QRegion clip(0, 0, width, height());
	clip -= rtrect;
	p->setClipRegion(clip);*/

	QFontMetrics fm(p->font());
	int baseline = fm.height() - fm.descent();

	if(isSelected())
	{
		p->setPen(cg.highlightedText());
		QFont f = p->font();
		f.setBold(true);
		p->setFont(f);
	}
	else
		p->setPen(cg.text());

	p->fillRect( 0, 0, width, height(), *paper );

	p->drawText(r, (height() - fm.height())/2 + baseline, text(0));

	if(px)
		p->drawPixmap(pxrect, *px);

	p->restore();
	delete paper;
}

int RichListViewItemB::widthUsed()
{
	return v_widthUsed;
}

//----------------------------------------------------------------------------
// ContactViewItemB
//----------------------------------------------------------------------------
class ContactViewItemB::Private
{
private:
	ContactViewItemB *cvi;

public:
	Private(ContactViewItemB *parent, ContactProfile *_cp) {
		cvi = parent;
		cp = _cp;
		u = 0;
		animateNickColor = false;

		icon = lastIcon = 0;
	}

	~Private() {
	}

	void initGroupState() {
		UserAccount::GroupData gd = groupData();
		cvi->setOpen(gd.open);
	}

	QString getGroupName() {
		QString group;
		if ( cvi->type() == Profile )
			group = "/\\/" + profileName + "\\/\\";
		else
			group = groupName;

		return group;
	}

	QMap<QString, UserAccount::GroupData> *groupState() {
		return (QMap<QString, UserAccount::GroupData> *)&cp->psiAccount()->userAccount().groupState;
	}

	UserAccount::GroupData groupData() {
		QMap<QString, UserAccount::GroupData> groupState = (QMap<QString, UserAccount::GroupData>)cp->psiAccount()->userAccount().groupState;
		QMap<QString, UserAccount::GroupData>::Iterator it = groupState.find(getGroupName());

		UserAccount::GroupData gd;
		gd.open = true;
		gd.rank = 0;

		if ( it != groupState.end() )
			gd = it.data();

		return gd;
	}

	ContactProfile *cp;
	int status;

	// profiles
	QString profileName;
	bool ssl;

	// groups
	int groupType;
	QString groupName;
	QString groupInfo;

	// contact
	UserListItem *u;
	bool isAgent;
	bool alerting;
	bool animatingNick;
	bool status_single;

	PsiIcon *icon, *lastIcon;
	int animateNickX, animateNickColor; // nick animation

	QRect chatRect, fileRect, moreRect;
};

ContactViewItemB::ContactViewItemB(const QString &profileName, ContactProfile *cp, ContactView *parent)
:RichListViewItemB(parent)
{
	type_ = Profile;
	d = new Private(this, cp);
	d->profileName = profileName;
	d->alerting = false;
	d->ssl = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	setProfileState(STATUS_OFFLINE);
	if (!PsiOptions::instance()->getOption("options.ui.account.single").toBool())
		setText(0, profileName);

	d->initGroupState();
}

ContactViewItemB::ContactViewItemB(const QString &groupName, int groupType, ContactProfile *cp, ContactView *parent)
:RichListViewItemB(parent)
{
	type_ = Group;
	d = new Private(this, cp);
	d->groupName = groupName;
	d->groupType = groupType;
	d->alerting = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	drawGroupIcon();
	resetGroupName();
	setDropEnabled(true);

	d->initGroupState();
}

ContactViewItemB::ContactViewItemB(const QString &groupName, int groupType, ContactProfile *cp, ContactViewItemB *parent)
:RichListViewItemB(parent)
{
	type_ = Group;
	d = new Private(this, cp);
	d->groupName = groupName;
	d->groupType = groupType;
	d->alerting = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	drawGroupIcon();
	resetGroupName();
	setDropEnabled(true);

	if(!parent->isVisible())
		setVisible(false);

	d->initGroupState();
}

ContactViewItemB::ContactViewItemB(UserListItem *u, ContactProfile *cp, ContactViewItemB *parent)
:RichListViewItemB(parent)
{
	type_ = Contact;
	d = new Private(this, cp);
	d->cp = cp;
	d->u = u;
	d->alerting = false;
	d->animatingNick = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	cacheValues();

	resetStatus();
	resetName();

	setDragEnabled(true);
	setDropEnabled(true);

	if(!parent->isVisible())
		setVisible(false);
	else
		setup();
}

// ###cuda warning, mostly just a copy of above!
ContactViewItemB::ContactViewItemB(UserListItem *u, ContactProfile *cp, ContactView *parent)
:RichListViewItemB(parent)
{
	//indent = 14;

	type_ = Contact;
	d = new Private(this, cp);
	d->cp = cp;
	d->u = u;
	d->alerting = false;
	d->animatingNick = false;
	d->status_single = !PsiOptions::instance()->getOption("options.ui.contactlist.status-messages.single-line").toBool();

	cacheValues();

	resetStatus();
	resetName();

	setDragEnabled(true);
	setDropEnabled(true);

	setup();
}

ContactViewItemB::~ContactViewItemB()
{
	setIcon( 0 );
	delete d;
}

void ContactViewItemB::setup()
{
	RichListViewItemB::setup();

	if(isSelected() && type_ == Contact && d->cp->d->rename_item != this)
		setHeight(148);
	else
		setHeight(25);
}

void ContactViewItemB::cacheValues()
{
	if ( d->u ) {
		if( !d->u->isAvailable() )
			d->status = STATUS_OFFLINE;
		else
			d->status = makeSTATUS((*d->u->priority()).status());
		d->isAgent = d->u->isTransport();
	}
}

ContactProfile *ContactViewItemB::contactProfile() const
{
	return d->cp;
}

int ContactViewItemB::type() const
{
	return type_;
}

const QString & ContactViewItemB::groupName() const
{
	return d->groupName;
}

const QString & ContactViewItemB::groupInfo() const
{
	return d->groupInfo;
}

int ContactViewItemB::groupType() const
{
	return d->groupType;
}

UserListItem *ContactViewItemB::u() const
{
	return d->u;
}

int ContactViewItemB::status() const
{
	return d->status;
}

bool ContactViewItemB::isAgent() const
{
	return d->isAgent;
}

bool ContactViewItemB::isAlerting() const
{
	return d->alerting;
}

bool ContactViewItemB::isAnimatingNick() const
{
	return d->animatingNick;
}

int ContactViewItemB::parentGroupType() const
{
	ContactViewItemB *item = (ContactViewItemB *)Q3ListViewItem::parent();
	return item->groupType();
}

void ContactViewItemB::drawGroupIcon()
{
	if ( type_ == Group ) {
		if ( childCount() == 0 )
			setIcon(IconsetFactory::iconPtr("psi/groupEmpty"));
		else if ( isOpen() )
			setIcon(IconsetFactory::iconPtr("psi/groupOpen"));
		else
			setIcon(IconsetFactory::iconPtr("psi/groupClosed"));
	}
	else if ( type_ == Profile ) {
		if ( !d->alerting )
			setProfileState(d->status);
	}
}

void ContactViewItemB::paintFocus(QPainter *, const QColorGroup &, const QRect &)
{
	// re-implimented to do nothing.  selection is enough of a focus
}

void ContactViewItemB::paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h)
{
	// paint a square of nothing
	p->fillRect(0, 0, w, h, cg.base());
}

void ContactViewItemB::paintCellContact(QPainter *p, const QColorGroup &cg, int column, int width, int align)
{
	p->save();

	Q3ListView* lv = listView();

	if ( isSelected() != v_selected || lv->isActiveWindow() != v_active) 
		setup();

	int h = height();
	if(isSelected() && type_ == Contact && d->cp->d->rename_item != this)
	{
		p->fillRect(0, 0, width, h, get_selectBg());
		h = 25;
	}

	int r = indent; //lv->itemMargin();

	const QBrush *paper;
	// setup (colors, sizes, ...)
	if ( isSelected() ) {
		paper = new QBrush(cg.brush( QColorGroup::Highlight ));
	}
	else{
#if QT_VERSION >= 0x040103 
		paper = new QBrush(cg.background(), Qt::NoBrush);
#else
		paper = new QBrush(cg.brush( QColorGroup::WindowText ));
#endif
	}
	
	const QPixmap * px = pixmap( column );
	QRect pxrect;
	int pxw = 0;
	int pxh = 0;
	if(px) {
		pxw = px->width();
		pxh = px->height();
		pxrect = QRect(r, icon_vpadding, pxw, pxh);
		//r += pxw + lv->itemMargin();
		r += pxw + 10;
	}

	if(px)
		pxrect.moveTop((h - pxh)/2);

	// start drawing
	/*QRect rtrect(r, (h - v_rt->height())/2, v_widthUsed, v_rt->height());
	v_rt->draw(p, rtrect.left(), rtrect.top(), rtrect, cg, paper);
	
	QRegion clip(0, 0, width, h);
	clip -= rtrect;
	p->setClipRegion(clip);*/

	QFontMetrics fm(p->font());
	int baseline = fm.height() - fm.descent();

	if(isSelected())
	{
		p->setPen(cg.highlightedText());
		QFont f = p->font();
		f.setBold(true);
		p->setFont(f);
	}
	else
		p->setPen(cg.text());

	p->fillRect( 0, 0, width, h, *paper );

	p->drawText(r, (h - fm.height())/2 + baseline, text(0));

	if(px)
		p->drawPixmap(pxrect, *px);

	p->restore();
	delete paper;
}

void ContactViewItemB::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment)
{
	if ( type_ == Contact ) {
		QColorGroup xcg = cg;

		/*if(d->status == STATUS_AWAY || d->status == STATUS_XA)
			xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.away").value<QColor>());
		else if(d->status == STATUS_DND)
			xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.do-not-disturb").value<QColor>());
		else if(d->status == STATUS_OFFLINE)
			xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status.offline").value<QColor>());*/
		if(d->status == STATUS_OFFLINE)
			xcg.setColor(QColorGroup::Text, Qt::gray);
		else
			xcg.setColor(QColorGroup::Text, get_contactOnline());

		xcg.setColor(QColorGroup::HighlightedText, Qt::white);

		if(d->animatingNick) {
			xcg.setColor(QColorGroup::Text, d->animateNickColor ? PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color1").value<QColor>() : PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color2").value<QColor>());
			xcg.setColor(QColorGroup::HighlightedText, d->animateNickColor ? PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color1").value<QColor>() : PsiOptions::instance()->getOption("options.ui.look.contactlist.status-change-animation.color2").value<QColor>());
		}

		paintCellContact(p, xcg, column, width, alignment);

		int x;
		if (d->status_single)
			x = widthUsed();
		else {
			QFontMetrics fm(p->font());
			const QPixmap *pix = pixmap(column);
			x = fm.width(text(column)) + (pix ? pix->width() : 0) + 8;
		}

		int h = 25;
		if ( d->u ) {
			UserResourceList::ConstIterator it = d->u->priority();
			if(it != d->u->userResourceList().end()) {
				if(d->u->isSecure((*it).name())) {
					const QPixmap &pix = IconsetFactory::iconPixmap("psi/cryptoYes");
					int y = (h - pix.height()) / 2;
					p->drawPixmap(x, y, pix);
					x += 24;
				}
			}
		}

		if(!isSelected() || d->cp->d->rename_item == this || height() <= 25)
			return;

		x = 0;
		int y = h;

		PsiAccount *pa = d->cp->psiAccount();
		QImage img;
		QPixmap avatarPixmap = pa->avatarFactory()->getAvatar(d->u->jid());
		if(!avatarPixmap.isNull())
			img = avatarPixmap.toImage();

		img = makeAvatarImage(img);
		p->drawImage(x + 8, y + 8, img);
		x += 8 + img.width() + 10;
		y += 8;

		QString phone; //= "<Unknown>";
		QString email; //= "<Unknown>";
		QString dept; //= "<Unknown>";
		const VCard *vcard = VCardFactory::instance()->vcard(d->u->jid());
		if(vcard)
		{
			if(!vcard->phoneList().isEmpty())
				phone = vcard->phoneList().first().number;
			if(!vcard->emailList().isEmpty())
				email = vcard->emailList().first().userid;
			if(!vcard->org().unit.isEmpty())
				dept = vcard->org().unit.first();
		}

		p->save();
		QFont f = p->font();
		f.setBold(true);
		p->setFont(f);
		QFontMetrics fm(f);
		p->setPen(get_windowText());
		p->drawText(x, y + fm.ascent(), status2txt(d->status));
		y += fm.height();
		f.setBold(false);
		p->setFont(f);
		fm = QFontMetrics(f);
		p->drawText(x, y + fm.ascent(), d->u->jid().full());
		y += fm.height();
		y += 2;
		p->setPen(get_expansionLine());
		p->drawLine(x, y, width - 8 - 16, y);
		y += 4;

		p->setPen(get_expansionText());

		if(!phone.isEmpty())
		{
			QString name = "Phone";
			QString val = phone;
			f.setBold(true);
			p->setFont(f);
			fm = QFontMetrics(f);
			p->drawText(x, y + fm.ascent(), name);
			f.setBold(false);
			fm = QFontMetrics(f);
			p->setFont(f);
			p->drawText(x + 40, y + fm.ascent(), val);
		}
		y += fm.height();

		if(!email.isEmpty())
		{
			QString name = "Email";
			QString val = email;
			f.setBold(true);
			p->setFont(f);
			fm = QFontMetrics(f);
			p->drawText(x, y + fm.ascent(), name);
			f.setBold(false);
			fm = QFontMetrics(f);
			p->setFont(f);
			p->drawText(x + 40, y + fm.ascent(), val);
		}
		y += fm.height();

		if(!dept.isEmpty())
		{
			QString name = "Dept";
			QString val = dept;
			f.setBold(true);
			p->setFont(f);
			fm = QFontMetrics(f);
			p->drawText(x, y + fm.ascent(), name);
			f.setBold(false);
			fm = QFontMetrics(f);
			p->setFont(f);
			p->drawText(x + 40, y + fm.ascent(), val);
		}
		y += fm.height();

		y += 4;

		p->setPen(Qt::white);

		QPixmap icon = get_icon_im();
		QImage bg = get_button_up().scaled(icon.width() + 16, 20);
		{
			QPainter p2(&bg);
			p2.drawPixmap(8, (bg.height() - icon.height()) / 2, icon);
		}
		p->drawImage(x, y, bg);
		d->chatRect = QRect(x, y, bg.width(), bg.height());
		x += bg.width() + 2;

		d->fileRect = QRect();
		if(!d->u->isTransport() && !d->cp->d->isTransportContact(d->u))
		{
		icon = get_icon_file();
		bg = get_button_up().scaled(icon.width() + 16, 20);
		{
			QPainter p2(&bg);
			p2.drawPixmap(8, (bg.height() - icon.height()) / 2, icon);
		}
		p->drawImage(x, y, bg);
		d->fileRect = QRect(x, y, bg.width(), bg.height());
		}
		x += bg.width() + 2;

		f = p->font();
		f.setBold(false);
		p->setFont(f);
		fm = QFontMetrics(f);
		bg = get_button_up().scaled(fm.width("More...") + 16, 20);
		{
			QPainter p2(&bg);
			p2.setPen(p->pen());
			p2.setFont(p->font());
			p2.drawText(8, (bg.height() - fm.height())/2 + fm.ascent(), "More...");
		}
		p->drawImage(x, y, bg);
		d->moreRect = QRect(x, y, bg.width(), bg.height());

		p->restore();
	}
	else if ( type_ == Group || type_ == Profile ) {
		QColorGroup xcg = cg;

		if(type_ == Profile) {
 			xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-foreground").value<QColor>());
			#if QT_VERSION < 0x040301
				xcg.setColor(QColorGroup::Background, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-background").value<QColor>());
			#else
 				xcg.setColor(QColorGroup::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.profile.header-background").value<QColor>());
 			#endif
			
		}
		else if(type_ == Group) {
			QFont f = p->font();
			f.setBold(true);
			//f.setPointSize(common_smallFontSize);
			p->setFont(f);
			/*xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>());
			if (!PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool()) {
 				#if QT_VERSION < 0x040301
					xcg.setColor(QColorGroup::Background, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
				#else
 					xcg.setColor(QColorGroup::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
				#endif
			}*/
			xcg.setColor(QColorGroup::Background, get_selectBg());
		} 

		Q3ListViewItem::paintCell(p, xcg, column, width, alignment);

		QFontMetrics fm(p->font());
		const QPixmap *pix = pixmap(column);
		int x = fm.width(text(column)) + (pix ? pix->width() : 0) + 8;

		if(type_ == Profile) {
			const QPixmap &pix = d->ssl ? IconsetFactory::iconPixmap("psi/cryptoYes") : IconsetFactory::iconPixmap("psi/cryptoNo");
			int y = (height() - pix.height()) / 2;
			p->drawPixmap(x, y, pix);
			x += 24;
		}

		if(isSelected())
			p->setPen(xcg.highlightedText());
		else
			p->setPen(xcg.text());

		QFont f_info = p->font();
		f_info.setPointSize(common_smallFontSize);
		p->setFont(f_info);
		QFontMetrics fm_info(p->font());
		//int info_x = width - fm_info.width(d->groupInfo) - 8;
		int info_x = x;
		int info_y = ((height() - fm_info.height()) / 2) + fm_info.ascent();
		p->drawText((info_x > x ? info_x : x), info_y, d->groupInfo);

		if(type_ == Group && PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool() && !isSelected()) {
			x += fm.width(d->groupInfo) + 8;
			if(x < width - 8) {
				int h = (height() / 2) - 1;
				p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>()));
				p->drawLine(x, h, width - 8, h);
				h++;
				p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>()));
				/*int h = height() / 2;

				p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>(), 2));*/
				p->drawLine(x, h, width - 8, h);
			}
		} 
		else {
			if (PsiOptions::instance()->getOption("options.ui.look.contactlist.use-outlined-group-headings").toBool()) {
				p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>()));
				p->drawRect(0, 0, width, height());
			}
		}
	}
}

/*
 * \brief "Opens" or "closes the ContactViewItemB
 *
 * When the item is in "open" state, all it's children items are visible.
 *
 * \param o - if true, the item will be "open"
 */
void ContactViewItemB::setOpen(bool o)
{
	((ContactView *)listView())->recalculateSize();

	Q3ListViewItem::setOpen(o);
	drawGroupIcon();

	// save state
	UserAccount::GroupData gd = d->groupData();
	gd.open = o;
	d->groupState()->insert(d->getGroupName(), gd);
}

void ContactViewItemB::insertItem(Q3ListViewItem *i)
{
	Q3ListViewItem::insertItem(i);
	drawGroupIcon();
}

void ContactViewItemB::startRename(int col)
{
	Q3ListViewItem::startRename(col);
}

void ContactViewItemB::takeItem(Q3ListViewItem *i)
{
	Q3ListViewItem::takeItem(i);
	drawGroupIcon();
}

int ContactViewItemB::rankGroup(int groupType) const
{
	static int rankgroups[5] = {
		gGeneral,
		gUser,
		gPrivate,
		gAgents,
		gNotInList,
	};

	int n;
	for(n = 0; n < (int)sizeof(rankgroups); ++n) {
		if(rankgroups[n] == groupType)
			break;
	}
	if(n == sizeof(rankgroups))
		return sizeof(rankgroups)-1;

	return n;
}

int ContactViewItemB::compare(Q3ListViewItem *lvi, int, bool) const
{
	ContactViewItemB *i = (ContactViewItemB *)lvi;
	int ret = 0;

	if(type_ == Contact) {
		// contacts always go before groups
		if(i->type() == Group)
			ret = -1;
		else {
			if ( PsiOptions::instance()->getOption("options.ui.contactlist.contact-sort-style").toString() == "status" ) {
				ret = rankStatus(d->status) - rankStatus(i->status());
				if(ret == 0)
					ret = text(0).lower().localeAwareCompare(i->text(0).lower());
			}
			else { // ContactSortStyle_Alpha
				ret = text(0).lower().localeAwareCompare(i->text(0).lower());
			}
		}
	}
	else if(type_ == Group || type_ == Profile) {
		// contacts always go before groups
		if(i->type() == Contact)
			ret = 1;
		else if(i->type() == Group) {
			if ( PsiOptions::instance()->getOption("options.ui.contactlist.group-sort-style").toString() == "rank") {
				int ourRank   = d->groupData().rank;
				int theirRank = i->d->groupData().rank;

				ret = ourRank - theirRank;
			}
			else { // GroupSortStyle_Alpha
				if(text(0) == FAVNAME) {
					ret = -1;
				}
				else if(i->text(0) == FAVNAME) {
					ret = 1;
				}
				else {
					ret = rankGroup(d->groupType) - rankGroup(i->groupType());
					if(ret == 0)
						ret = text(0).lower().localeAwareCompare(i->text(0).lower());
				}
			}
		}
		else if(i->type() == Profile) {
			if ( PsiOptions::instance()->getOption("options.ui.contactlist.account-sort-style").toString() == "rank" ) {
				int ourRank = d->groupData().rank;
				int theirRank = i->d->groupData().rank;

				ret = ourRank - theirRank;
			}
			else // AccountSortStyle_Alpha
				ret = text(0).lower().localeAwareCompare(i->text(0).lower());
		}
	}

	return ret;
}

void ContactViewItemB::setProfileName(const QString &name)
{
	d->profileName = name;
	if (!PsiOptions::instance()->getOption("options.ui.account.single").toBool())
		setText(0, d->profileName);
	else
		setText(0, "");
}

void ContactViewItemB::setProfileState(int status)
{
	if ( status == -1 ) {
		setAlert( IconsetFactory::iconPtr("psi/connect") );
	}
	else {
		d->status = status;

		clearAlert();
		setIcon(PsiIconset::instance()->statusPtr(status));
	}
}

void ContactViewItemB::setProfileSSL(bool on)
{
	d->ssl = on;
	repaint();
}

void ContactViewItemB::setGroupName(const QString &name)
{
	d->groupName = name;
	resetGroupName();

	updatePosition();
}

void ContactViewItemB::setGroupInfo(const QString &info)
{
	d->groupInfo = info;
	repaint();
}

void ContactViewItemB::resetStatus()
{
	if ( !d->alerting && d->u ) {
		setIcon(PsiIconset::instance()->statusPtr(d->u));
	}

	// If the status is shown, update the text of the item too
	if (static_cast<ContactView*>(Q3ListViewItem::listView())->isShowStatusMsg())
		resetName();
}

void ContactViewItemB::resetName(bool forceNoStatusMsg)
{
	if ( d->u ) {
		QString s = JIDUtil::nickOrJid(d->u->name(), d->u->jid().full());
			
		if (d->status_single && !forceNoStatusMsg) {
			s = "<nobr>" + s + "</nobr>";
		}

		// Add the status message if wanted 
		if (!forceNoStatusMsg && static_cast<ContactView*>(Q3ListViewItem::listView())->isShowStatusMsg()) {
			QString statusMsg;
			if (d->u->priority() != d->u->userResourceList().end()) 
				statusMsg = (*d->u->priority()).status().status();
			else 
				statusMsg = d->u->lastUnavailableStatus().status();

			if (d->status_single) {
				statusMsg.replace("<","&lt;");
				statusMsg.replace(">","&gt;");
				statusMsg = statusMsg.simplifyWhiteSpace();
				if (!statusMsg.isEmpty())
					s += "<br><font size=-1 color='" + PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.status-messages").value<QColor>().name() + "'><nobr>" + statusMsg + "</nobr></font>";
			}
			else {
				statusMsg.replace('\n'," ");
				if (!statusMsg.isEmpty())
					s += " (" + statusMsg + ")";
			}
		}

		/*SimplePrivacyManager *spm = (SimplePrivacyManager *)d->cp->d->pa->privacyManager();
		if (spm->isContactBlocked(d->u->jid()))
			s += " (blocked)";*/

		if ( s != text(0) ) {
			setText(0, s);
		}
	}
}

void ContactViewItemB::resetGroupName()
{
	if ( d->groupName != text(0) )
		setText(0, d->groupName);
}

void ContactViewItemB::resetAnim()
{
	if ( d->alerting ) {
		// TODO: think of how to reset animation frame
	}
}

void ContactViewItemB::setAlert(const PsiIcon *icon)
{
	bool reset = false;

	if ( !d->alerting ) {
		d->alerting = true;
		reset = true;
	}
	else {
		if ( d->lastIcon != icon )
			reset = true;
	}

	if ( reset )
		setIcon(icon, true);
}

void ContactViewItemB::clearAlert()
{
	if ( d->alerting ) {
		d->alerting = false;
		//disconnect(static_cast<ContactView*>(QListViewItem::listView())->animTimer(), SIGNAL(timeout()), this, SLOT(animate()));
		resetStatus();
	}
}

void ContactViewItemB::setIcon(const PsiIcon *icon, bool alert)
{
	if ( d->lastIcon == icon ) {
		return; // cause less flicker. but still have to run calltree valgring skin on psi while online (mblsha).
	}
	else
		d->lastIcon = (PsiIcon *)icon;

	if ( d->icon ) {
		disconnect(d->icon, 0, this, 0 );
		d->icon->stop();

		delete d->icon;
		d->icon = 0;
	}

	QPixmap pix;
	if ( icon ) {
		if ( !alert )
			d->icon = new PsiIcon(*icon);
		else
			d->icon = new AlertIcon(icon);

		if (!PsiOptions::instance()->getOption("options.ui.contactlist.temp-no-roster-animation").toBool()) {
			connect(d->icon, SIGNAL(pixmapChanged()), SLOT(iconUpdated()));
		}
		d->icon->activated();

		pix = d->icon->pixmap();
	}

	setPixmap(0, pix);
}

void ContactViewItemB::iconUpdated()
{
	setPixmap(0, d->icon ? d->icon->pixmap() : QPixmap());
}

void ContactViewItemB::animateNick()
{
	d->animateNickColor = !d->animateNickColor;
	repaint();

	if(++d->animateNickX >= 16)
		stopAnimateNick();
}

void ContactViewItemB::stopAnimateNick()
{
	if ( !d->animatingNick )
		return;

	disconnect(static_cast<ContactView*>(Q3ListViewItem::listView())->animTimer(), SIGNAL(timeout()), this, SLOT(animateNick()));

	d->animatingNick = false;
	repaint();
}

void ContactViewItemB::setAnimateNick()
{
	stopAnimateNick();

	connect(static_cast<ContactView*>(Q3ListViewItem::listView())->animTimer(), SIGNAL(timeout()), SLOT(animateNick()));

	d->animatingNick = true;
	d->animateNickX = 0;
	animateNick();
}

void ContactViewItemB::updatePosition()
{
	ContactViewItemB *par = (ContactViewItemB *)Q3ListViewItem::parent();
	if(!par)
		return;

	ContactViewItemB *after = 0;
	for(Q3ListViewItem *i = par->firstChild(); i; i = i->nextSibling()) {
		ContactViewItemB *item = (ContactViewItemB *)i;
		// skip self
		if(item == this)
			continue;
		int x = compare(item, 0, true);
		if(x == 0)
			continue;
		if(x < 0)
			break;
		after = item;
	}

	if(after)
		moveItem(after);
	else {
		Q3ListViewItem *i = par->firstChild();
		moveItem(i);
		i->moveItem(this);
	}
}

void ContactViewItemB::optionsUpdate()
{
	if(type_ == Group || type_ == Profile) {
		drawGroupIcon();
	}
	else if(type_ == Contact) {
		if(!d->alerting)
			resetStatus();
		else
			resetAnim();
	}
}

void ContactViewItemB::setContact(UserListItem *u)
{
	int oldStatus = d->status;
	QString oldName = text(0);
	bool wasAgent = d->isAgent;

	//QString newName = JIDUtil::nickOrJid(u->name(),u->jid().full());

	d->u = u;
	cacheValues();

	bool needUpdate = false;
	if(d->status != oldStatus || d->isAgent != wasAgent || !u->presenceError().isEmpty()) {
		resetStatus();
		needUpdate = true;
	}

	// Hack, but that's the safest way.
	resetName();
	QString newName = text(0);
	if(newName != oldName) {
		needUpdate = true;
	}

	if(needUpdate)
		updatePosition();

	repaint();
	setup();
}

bool ContactViewItemB::acceptDrop(const QMimeSource *m) const
{
	if ( type_ == Profile )
		return false;
	else if ( type_ == Group ) {
		if(d->groupType != gGeneral && d->groupType != gUser)
			return false;
	}
	else if ( type_ == Contact ) {
		if ( d->u && d->u->isSelf() )
			return false;
		ContactViewItemB *par = (ContactViewItemB *)Q3ListViewItem::parent();
		if(par->groupType() != gGeneral && par->groupType() != gUser)
			return false;
	}

	// Files. Note that the QTextDrag test has to come after QUriDrag.
	if (type_ == Contact && Q3UriDrag::canDecode(m)) {
		QStringList l;
		if (Q3UriDrag::decodeLocalFiles(m,l) && !l.isEmpty())
			return true;
	}

	if(!Q3TextDrag::canDecode(m))
		return false;

	QString str;
	if(!Q3TextDrag::decode(m, str))
		return false;

	return true;
}

void ContactViewItemB::dragEntered()
{
	//printf("entered\n");
}

void ContactViewItemB::dragLeft()
{
	//printf("left\n");
}

void ContactViewItemB::dropped(QDropEvent *i)
{
	if(!acceptDrop(i))
		return;

	// Files
	if (Q3UriDrag::canDecode(i)) {
		QStringList l;
		if (Q3UriDrag::decodeLocalFiles(i,l) && !l.isEmpty()) {
			d->cp->dragDropFiles(l, this);
			return;
		}
	}

	// Text
	if(Q3TextDrag::canDecode(i)) {
		QString text;
		if(Q3TextDrag::decode(i, text))
			d->cp->dragDrop(text, this);
	}
}

void ContactViewItemB::cancelRename (int i)
{
	Q3ListViewItem::cancelRename(i);
	resetName();
}

int ContactViewItemB::rtti() const
{
	return 5103;
}

void ContactViewItemB::expandArea()
{
//	setHeight(125);
}

void ContactViewItemB::closeArea()
{
//	setHeight(25);
}

void ContactViewItemB::clicked(const QPoint &p, const QPoint &glob)
{
	//printf("clicked: (%d,%d)\n", p.x(), p.y());
	if(d->chatRect.contains(p))
	{
		//printf("  *chat\n");
		d->cp->actionOpenChat(d->u->jid());
	}
	else if(d->fileRect.contains(p))
	{
		//printf("  *file\n");
		d->cp->actionSendFile(d->u->jid());
	}
	else if(d->moreRect.contains(p))
	{
		//printf("  *more\n");
		d->cp->doContextMenu(this, glob);
	}
}

void ContactViewItemB::doRename()
{
	resetName(true);
	setRenameEnabled(0, true);
	startRename(0);
	setRenameEnabled(0, false);
}

#include "contactview.moc"
