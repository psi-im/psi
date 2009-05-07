/*
 * gcuserview.cpp - groupchat roster
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

#include "gcuserview.h"

#include <QPainter>
#include <Q3Header>
#include <Q3TextDrag>
#include <Q3PopupMenu>

#include "capsmanager.h"
#include "psitooltip.h"
#include "psiaccount.h"
#include "userlist.h"
#include "psiiconset.h"
#include "groupchatdlg.h"
#include "common.h"
#include "psioptions.h"

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

//----------------------------------------------------------------------------
// GCUserViewItem
//----------------------------------------------------------------------------

GCUserViewItem::GCUserViewItem(GCUserViewGroupItem *par)
	: QObject()
	, Q3ListViewItem(par)
{
	setDragEnabled(true);
}

void GCUserViewItem::paintFocus(QPainter *, const QColorGroup &, const QRect &)
{
	// re-implimented to do nothing.  selection is enough of a focus
}

void GCUserViewItem::paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h)
{
	// paint a square of nothing
	p->fillRect(0, 0, w, h, cg.base());
}

//----------------------------------------------------------------------------
// GCUserViewGroupItem
//----------------------------------------------------------------------------

GCUserViewGroupItem::GCUserViewGroupItem(GCUserView *par, const QString& t, int k)
:Q3ListViewItem(par,t), key_(k)
{
	setDragEnabled(false);
}

void GCUserViewGroupItem::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment)
{
	QColorGroup xcg = cg;
	QFont f = p->font();
	f.setPointSize(common_smallFontSize);
	p->setFont(f);
	xcg.setColor(QColorGroup::Text, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>());
	if (!PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool()) {
		#if QT_VERSION < 0x040301
			xcg.setColor(QColorGroup::Background, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
		#else
			xcg.setColor(QColorGroup::Base, PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>());
		#endif
	}
	Q3ListViewItem::paintCell(p, xcg, column, width, alignment);
	if (PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool() && !isSelected()) {
		QFontMetrics fm(p->font());
		int x = fm.width(text(column)) + 8;
		if(x < width - 8) {
			int h = (height() / 2) - 1;
			p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-background").value<QColor>()));
			p->drawLine(x, h, width - 8, h);
			h++;
			p->setPen(QPen(PsiOptions::instance()->getOption("options.ui.look.colors.contactlist.grouping.header-foreground").value<QColor>()));
			p->drawLine(x, h, width - 8, h);
		}
	}
}

void GCUserViewGroupItem::paintFocus(QPainter *, const QColorGroup &, const QRect &)
{
	// re-implimented to do nothing.  selection is enough of a focus
}

void GCUserViewGroupItem::paintBranches(QPainter *p, const QColorGroup &cg, int w, int, int h)
{
	// paint a square of nothing
	p->fillRect(0, 0, w, h, cg.base());
}

int GCUserViewGroupItem::compare(Q3ListViewItem *i, int col, bool ascending) const
{
	Q_UNUSED(ascending);	// Qt docs say: "your code can safely ignore it"

	if (col == 0)
		// groups are never compared to users, so static_cast is safe
		return this->key_ - static_cast<GCUserViewGroupItem*>(i)->key_;
	else
		return Q3ListViewItem::compare(i, col, ascending);
}

//----------------------------------------------------------------------------
// GCUserView
//----------------------------------------------------------------------------

GCUserView::GCUserView(QWidget* parent)
	: Q3ListView(parent)
	, gcDlg_(0)
{
	setResizeMode(Q3ListView::AllColumns);
	setTreeStepSize(0);
	setShowToolTips(false);
	header()->hide();
	addColumn("");
	setSortColumn(0);
	Q3ListViewItem* i;
	i = new GCUserViewGroupItem(this, tr("Visitors"), 3);
	i->setOpen(true);
	i = new GCUserViewGroupItem(this, tr("Participants"), 2);
	i->setOpen(true);
	i = new GCUserViewGroupItem(this, tr("Moderators"), 1);
	i->setOpen(true);

	connect(this, SIGNAL(doubleClicked(Q3ListViewItem *)), SLOT(qlv_doubleClicked(Q3ListViewItem *)));
	connect(this, SIGNAL(contextMenuRequested(Q3ListViewItem *, const QPoint &, int)), SLOT(qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int)));
	connect(this, SIGNAL(mouseButtonClicked(int, Q3ListViewItem*, const QPoint&, int)), SLOT(qlv_mouseButtonClicked(int, Q3ListViewItem*, const QPoint&, int)));
}

GCUserView::~GCUserView()
{
}

void GCUserView::setMainDlg(GCMainDlg* mainDlg)
{
	gcDlg_ = mainDlg;
}

Q3DragObject* GCUserView::dragObject()
{
	Q3ListViewItem* it = currentItem();
	if (it) {
		// WARNING: We are assuming that group items can never be dragged
		GCUserViewItem* u = (GCUserViewItem*) it;
		if (!u->s.mucItem().jid().isEmpty())
			return new Q3TextDrag(u->s.mucItem().jid().bare(),this);
	}
	return NULL;
}

void GCUserView::clear()
{
	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling())
		while (GCUserViewItem* i = (GCUserViewItem*) j->firstChild()) {
			delete i;
		}
}

void GCUserView::updateAll()
{
	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling())
		for(GCUserViewItem *i = (GCUserViewItem *)j->firstChild(); i; i = (GCUserViewItem *)i->nextSibling())
			i->setPixmap(0, PsiIconset::instance()->status(i->s).impix());
}

QStringList GCUserView::nickList() const
{
	QStringList list;

	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling())
		for(Q3ListViewItem *lvi = j->firstChild(); lvi; lvi = lvi->nextSibling())
			list << lvi->text(0);

	qSort(list.begin(), list.end(), caseInsensitiveLessThan);
	return list;
}

bool GCUserView::hasJid(const Jid& jid)
{
	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling())
		for(GCUserViewItem *lvi = (GCUserViewItem*) j->firstChild(); lvi; lvi = (GCUserViewItem*) lvi->nextSibling()) {
			if(!lvi->s.mucItem().jid().isEmpty() && lvi->s.mucItem().jid().compare(jid,false))
				return true;
		}
	return false;
}

Q3ListViewItem *GCUserView::findEntry(const QString &nick)
{
	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling())
		for(Q3ListViewItem *lvi = j->firstChild(); lvi; lvi = lvi->nextSibling()) {
			if(lvi->text(0) == nick)
				return lvi;
		}
	return 0;
}

void GCUserView::updateEntry(const QString &nick, const Status &s)
{
	GCUserViewItem *lvi = (GCUserViewItem *)findEntry(nick);
	if (lvi && lvi->s.mucItem().role() != s.mucItem().role()) {
		delete lvi;
		lvi = NULL;
	}

	if(!lvi) {
		lvi = new GCUserViewItem(findGroup(s.mucItem().role()));
		lvi->setText(0, nick);
	}

	lvi->s = s;
	lvi->setPixmap(0, PsiIconset::instance()->status(lvi->s).impix());
}

GCUserViewGroupItem* GCUserView::findGroup(MUCItem::Role a) const
{
	Role r = Visitor;
	if (a == MUCItem::Moderator)
		r = Moderator;
	else if (a == MUCItem::Participant)
		r = Participant;

	int i = 0;
	for (Q3ListViewItem *j = firstChild(); j; j = j->nextSibling()) {
		if (i == (int) r)
			return (GCUserViewGroupItem*) j;
		i++;
	}

	return NULL;
}

void GCUserView::removeEntry(const QString &nick)
{
	Q3ListViewItem *lvi = findEntry(nick);
	if(lvi)
		delete lvi;
}

bool GCUserView::maybeTip(const QPoint &pos)
{
	Q3ListViewItem *qlvi = itemAt(pos);
	if(!qlvi || !qlvi->parent())
		return false;

	GCUserViewItem *lvi = (GCUserViewItem *) qlvi;
	QRect r(itemRect(lvi));

	const QString &nick = lvi->text(0);
	const Status &s = lvi->s;
	UserListItem u;
	// SICK SICK SICK SICK
	GCMainDlg* dlg = gcDlg_;
	if (!dlg) {
		qDebug("Calling maybetip on an entity without an owning dialog");
		return false;
	}
	u.setJid(dlg->jid().withResource(nick));
	u.setName(nick);

	// Find out capabilities info
	Jid caps_jid(s.mucItem().jid().isEmpty() ? dlg->jid().withResource(nick) : s.mucItem().jid());
	QString client_name = dlg->account()->capsManager()->clientName(caps_jid);
	QString client_version = (client_name.isEmpty() ? QString() : dlg->account()->capsManager()->clientVersion(caps_jid));

	// make a resource so the contact appears online
	UserResource ur;
	ur.setName(nick);
	ur.setStatus(s);
	ur.setClient(client_name,client_version,"");
	u.userResourceList().append(ur);

	PsiToolTip::showText(mapToGlobal(pos), u.makeTip(), this);
	return true;
}

bool GCUserView::event(QEvent* e)
{
	if (e->type() == QEvent::ToolTip) {
		QPoint pos = ((QHelpEvent*) e)->pos();
		e->setAccepted(maybeTip(pos));
		return true;
	}
	return Q3ListView::event(e);
}

void GCUserView::qlv_doubleClicked(Q3ListViewItem *i)
{
	if(!i || !i->parent())
		return;

	GCUserViewItem *lvi = (GCUserViewItem *)i;
	if(PsiOptions::instance()->getOption("options.messages.default-outgoing-message-type").toString() == "message")
		action(lvi->text(0), lvi->s, 0);
	else
		action(lvi->text(0), lvi->s, 1);
}

void GCUserView::qlv_contextMenuRequested(Q3ListViewItem *i, const QPoint &pos, int)
{
	if(!i || !i->parent() || !gcDlg_)
		return;

	QPointer<GCUserViewItem> lvi = (GCUserViewItem *)i;
	bool self = gcDlg_->nick() == i->text(0);
	GCUserViewItem* c = (GCUserViewItem*) findEntry(gcDlg_->nick());
	if (!c) {
		qWarning(QString("groupchatdlg.cpp: Self ('%1') not found in contactlist").arg(gcDlg_->nick()));
		return;
	}
	Q3PopupMenu *pm = new Q3PopupMenu;
	pm->insertItem(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &Message"), 0);
	pm->insertItem(IconsetFactory::icon("psi/start-chat").icon(), tr("Open &Chat Window"), 1);
	pm->insertSeparator();
	pm->insertItem(tr("&Kick"),10);
	pm->setItemEnabled(10, MUCManager::canKick(c->s.mucItem(),lvi->s.mucItem()));
	pm->insertItem(tr("&Ban"),11);
	pm->setItemEnabled(11, MUCManager::canBan(c->s.mucItem(),lvi->s.mucItem()));

	Q3PopupMenu* rm = new Q3PopupMenu(pm);
	rm->insertItem(tr("Visitor"),12);
	rm->setItemChecked(12, lvi->s.mucItem().role() == MUCItem::Visitor);
	rm->setItemEnabled(12, (!self || lvi->s.mucItem().role() == MUCItem::Visitor) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Visitor));
	rm->insertItem(tr("Participant"),13);
	rm->setItemChecked(13, lvi->s.mucItem().role() == MUCItem::Participant);
	rm->setItemEnabled(13, (!self || lvi->s.mucItem().role() == MUCItem::Participant) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Participant));
	rm->insertItem(tr("Moderator"),14);
	rm->setItemChecked(14, lvi->s.mucItem().role() == MUCItem::Moderator);
	rm->setItemEnabled(14, (!self || lvi->s.mucItem().role() == MUCItem::Moderator) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Moderator));
	pm->insertItem(tr("Change Role"),rm);

	/*Q3PopupMenu* am = new Q3PopupMenu(pm);
	am->insertItem(tr("Unaffiliated"),15);
	am->setItemChecked(15, lvi->s.mucItem().affiliation() == MUCItem::NoAffiliation);
	am->setItemEnabled(15, (!self || lvi->s.mucItem().affiliation() == MUCItem::NoAffiliation) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::NoAffiliation));
	am->insertItem(tr("Member"),16);
	am->setItemChecked(16, lvi->s.mucItem().affiliation() == MUCItem::Member);
	am->setItemEnabled(16,  (!self || lvi->s.mucItem().affiliation() == MUCItem::Member) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Member));
	am->insertItem(tr("Administrator"),17);
	am->setItemChecked(17, lvi->s.mucItem().affiliation() == MUCItem::Admin);
	am->setItemEnabled(17,  (!self || lvi->s.mucItem().affiliation() == MUCItem::Admin) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Admin));
	am->insertItem(tr("Owner"),18);
	am->setItemChecked(18, lvi->s.mucItem().affiliation() == MUCItem::Owner);
	am->setItemEnabled(18,  (!self || lvi->s.mucItem().affiliation() == MUCItem::Owner) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Owner));
	pm->insertItem(tr("Change Affiliation"),am);*/
	pm->insertSeparator();
	//pm->insertItem(tr("Send &File"), 4);
	//pm->insertSeparator();
	pm->insertItem(tr("Check &Status"), 2);
	pm->insertItem(IconsetFactory::icon("psi/vCard").icon(), tr("User &Info"), 3);
	int x = pm->exec(pos);
	bool enabled = pm->isItemEnabled(x) || rm->isItemEnabled(x);
	delete pm;

	if(x == -1 || !enabled || lvi.isNull())
		return;
	action(lvi->text(0), lvi->s, x);
}

void GCUserView::qlv_mouseButtonClicked(int button, Q3ListViewItem* item, const QPoint& pos, int c)
{
	Q_UNUSED(pos);
	Q_UNUSED(c);
	if (!item || !item->parent() || !gcDlg_)
		return;
	if (button != Qt::MidButton)
		return;

	emit insertNick(item->text(0));
}
