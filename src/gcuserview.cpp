/*
 * gcuserview.cpp - groupchat roster
 * Copyright (C) 2001, 2002  Justin Karneges
 * 2011 Khryukin Evgeny
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
#include <QMouseEvent>
#include <QItemDelegate>
#include <QMenu>

#include "capsmanager.h"
#include "psitooltip.h"
#include "psiaccount.h"
#include "userlist.h"
#include "psiiconset.h"
#include "groupchatdlg.h"
#include "common.h"
#include "psioptions.h"
#include "coloropt.h"

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}


//----------------------------------------------------------------------------
// GCUserViewDelegate
//----------------------------------------------------------------------------
class GCUserViewDelegate : public QItemDelegate
{
	Q_OBJECT
public:
	GCUserViewDelegate(QObject* p)
		: QItemDelegate(p)
	{
	}

	void paint(QPainter* mp, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		GCUserView *uv = dynamic_cast<GCUserView*>(parent());
		if(uv) {
			QTreeWidgetItem *i = uv->findEntry(index);
			GCUserViewGroupItem *gi = dynamic_cast<GCUserViewGroupItem*>(i);
			if(gi) {
				paintGroup(mp, option, gi);
			}
			else {
				paintContact(mp, option, index);
			}
		}
	}

	void paintGroup(QPainter* p, const QStyleOptionViewItem& o, GCUserViewGroupItem* gi) const
	{
		QRect rect = o.rect;
		QFont f = o.font;
		f.setPointSize(common_smallFontSize);
		p->setFont(f);
		QColor colorForeground = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-foreground");
		QColor colorBackground = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-background");
		if (!PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool()) {
			p->fillRect(rect, colorBackground);
		}

		p->setPen(QPen(colorForeground));
		p->drawText(rect, gi->text(0));
		if (PsiOptions::instance()->getOption("options.ui.look.contactlist.use-slim-group-headings").toBool()
			&& (o.state & QStyle::State_Selected))
		{
			QFontMetrics fm(f);
			int x = fm.width(gi->text(0)) + 8;
			int width = rect.width();
			if(x < width - 8) {
				int h = (rect.height() / 2) - 1;
				p->setPen(QPen(colorBackground));
				p->drawLine(x, h, width - 8, h);
				h++;
				p->setPen(QPen(colorForeground));
				p->drawLine(x, h, width - 8, h);
			}
		}
	}

	void paintContact(QPainter* mp, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		QItemDelegate::paint(mp, option, index);
	}

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		return QItemDelegate::sizeHint(option, index);
	}
};


//----------------------------------------------------------------------------
// GCUserViewItem
//----------------------------------------------------------------------------

GCUserViewItem::GCUserViewItem(GCUserViewGroupItem *par)
	: QObject()
	, QTreeWidgetItem(par)
{
}

bool GCUserViewItem::operator<(const QTreeWidgetItem& it) const
{	
	GCUserViewItem *item = (GCUserViewItem*)(&it);
	if(PsiOptions::instance()->getOption("options.ui.contactlist.contact-sort-style").toString() == "status") {
		int rank = rankStatus(s.type()) - rankStatus(item->s.type());
		if (rank == 0)
			rank = QString::localeAwareCompare(text(0).toLower(), it.text(0).toLower());
		return rank < 0;
	}
	else {
		return text(0).toLower() < it.text(0).toLower();
	}
}


//----------------------------------------------------------------------------
// GCUserViewGroupItem
//----------------------------------------------------------------------------

GCUserViewGroupItem::GCUserViewGroupItem(GCUserView *par, const QString& t, int k)
	: QTreeWidgetItem(par, QStringList(t))
	, key_(k)
	, baseText_(t)
{
	updateText();
}

void GCUserViewGroupItem::updateText()
{
	int c = childCount();
	setText(0, baseText_ + (c ? QString("  (%1)").arg(c) : ""));
}

//----------------------------------------------------------------------------
// GCUserView
//----------------------------------------------------------------------------

GCUserView::GCUserView(QWidget* parent)
	: QTreeWidget(parent)
	, gcDlg_(0)
{
	header()->hide();
	sortByColumn(0);
	setIndentation(0);
	setContextMenuPolicy(Qt::NoContextMenu);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setItemDelegate(new GCUserViewDelegate(this));
	QTreeWidgetItem* i;
	i = new GCUserViewGroupItem(this, tr("Moderators"), Moderator);
	i->setExpanded(true);
	i = new GCUserViewGroupItem(this, tr("Participants"), Participant);
	i->setExpanded(true);
	i = new GCUserViewGroupItem(this, tr("Visitors"), Visitor);
	i->setExpanded(true);

	connect(this, SIGNAL(doubleClicked(QModelIndex)), SLOT(qlv_doubleClicked(QModelIndex)));
}

GCUserView::~GCUserView()
{
}

void GCUserView::setMainDlg(GCMainDlg* mainDlg)
{
	gcDlg_ = mainDlg;
}

//Q3DragObject* GCUserView::dragObject()
//{
//	Q3ListViewItem* it = currentItem();
//	if (it) {
//		// WARNING: We are assuming that group items can never be dragged
//		GCUserViewItem* u = (GCUserViewItem*) it;
//		if (!u->s.mucItem().jid().isEmpty())
//			return new Q3TextDrag(u->s.mucItem().jid().bare(),this);
//	}
//	return NULL;
//}

void GCUserView::clear()
{
	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		qDeleteAll(j->takeChildren());
	}
}

void GCUserView::updateAll()
{
	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		int count = j->childCount();
		for(int num = 0; num < count; num++) {
			GCUserViewItem *i = (GCUserViewItem*)j->child(num);
			i->setIcon(0, PsiIconset::instance()->status(i->s).icon());
		}
		j->sortChildren(0, Qt::AscendingOrder);
	}
}

QStringList GCUserView::nickList() const
{
	QStringList list;

	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		int count = j->childCount();
		for(int num = 0; num < count; num++) {
			QTreeWidgetItem *lvi = j->child(num);
			list << lvi->text(0);
		}
	}

	qSort(list.begin(), list.end(), caseInsensitiveLessThan);
	return list;
}

bool GCUserView::hasJid(const Jid& jid)
{
	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		int count = j->childCount();
		for(int num = 0; num < count; num++) {
			GCUserViewItem *lvi = (GCUserViewItem*) j->child(num);
			if(!lvi->s.mucItem().jid().isEmpty() && lvi->s.mucItem().jid().compare(jid,false))
				return true;
		}
	}

	return false;
}

QTreeWidgetItem *GCUserView::findEntry(const QString &nick)
{
	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		int count = j->childCount();
		for(int num = 0; num < count; num++) {
			QTreeWidgetItem *lvi = j->child(num);
			if(lvi->text(0) == nick)
				return lvi;
		}
	}

	return 0;
}

QTreeWidgetItem *GCUserView::findEntry(const QModelIndex &index)
{
	return itemFromIndex(index);
}

void GCUserView::updateEntry(const QString &nick, const Status &s)
{
	GCUserViewGroupItem* gr;
	GCUserViewItem *lvi = (GCUserViewItem *)findEntry(nick);
	if (lvi && lvi->s.mucItem().role() != s.mucItem().role()) {
		gr = findGroup(lvi->s.mucItem().role());
		delete lvi;
		gr->updateText();
		lvi = NULL;
	}

	gr = findGroup(s.mucItem().role());
	if(!lvi) {
		lvi = new GCUserViewItem(gr);
		lvi->setText(0, nick);
		gr->updateText();
	}

	lvi->s = s;
	lvi->setIcon(0, PsiIconset::instance()->status(lvi->s).icon());
	gr->sortChildren(0, Qt::AscendingOrder);
}

GCUserViewGroupItem* GCUserView::findGroup(MUCItem::Role a) const
{
	Role r = Visitor;
	if (a == MUCItem::Moderator)
		r = Moderator;
	else if (a == MUCItem::Participant)
		r = Participant;

	int topCount = topLevelItemCount();
	for(int num = 0; num < topCount; num++) {
		GCUserViewGroupItem *j = (GCUserViewGroupItem*)topLevelItem(num);
		if ((Role)j->key() == r)
			return (GCUserViewGroupItem*) j;
	}

	return 0;
}

void GCUserView::removeEntry(const QString &nick)
{
	GCUserViewItem *lvi = (GCUserViewItem *)findEntry(nick);
	if(lvi) {
		GCUserViewGroupItem* gr = findGroup(lvi->s.mucItem().role());
		delete lvi;
		gr->updateText();
	}
}

bool GCUserView::maybeTip(const QPoint &pos)
{
	QTreeWidgetItem *qlvi = itemAt(pos);
	if(!qlvi || !qlvi->parent())
		return false;

	GCUserViewItem *lvi = (GCUserViewItem *) qlvi;
	QRect r(visualItemRect(lvi));

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
	//ur.setClient(client_name,client_version,"");
	ur.setClient(QString(),QString(),"");
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
	return QTreeWidget::event(e);
}

void GCUserView::qlv_doubleClicked(const QModelIndex &index)
{
	if(!index.isValid())
		return;

	GCUserViewItem *lvi = dynamic_cast<GCUserViewItem*>(itemFromIndex(index));
	if(lvi) {
		if(PsiOptions::instance()->getOption("options.messages.default-outgoing-message-type").toString() == "message")
			action(lvi->text(0), lvi->s, 0);
		else
			action(lvi->text(0), lvi->s, 1);
	}
}

void GCUserView::contextMenuRequested(const QPoint &p)
{
	QTreeWidgetItem *i = itemAt(p);

	if(!i || !i->parent() || !gcDlg_)
		return;

	QPointer<GCUserViewItem> lvi = (GCUserViewItem *)i;
	bool self = gcDlg_->nick() == i->text(0);
	GCUserViewItem* c = (GCUserViewItem*) findEntry(gcDlg_->nick());
	if (!c) {
		qWarning() << QString("groupchatdlg.cpp: Self ('%1') not found in contactlist").arg(gcDlg_->nick());
		return;
	}
	QAction* act;
	QMenu *pm = new QMenu();
	act = new QAction(IconsetFactory::icon("psi/sendMessage").icon(), tr("Send &Message"), pm);
	pm->addAction(act);
	act->setData(0);
	act = new QAction(IconsetFactory::icon("psi/start-chat").icon(), tr("Open &Chat Window"), pm);
	pm->addAction(act);
	act->setData(1);
	pm->addSeparator();

	// Kick and Ban submenus
	QStringList reasons = PsiOptions::instance()->getOption("options.muc.reasons").toStringList();
	int cntReasons = reasons.count();
	if (cntReasons > 99)
		cntReasons = 99; // Only first 99 reasons

	QMenu *kickMenu = new QMenu(tr("&Kick"), pm);
	act = new QAction(tr("No reason"), kickMenu);
	kickMenu->addAction(act);
	act->setData(10);
	act = new QAction(tr("Custom reason"), kickMenu);
	kickMenu->addAction(act);
	act->setData(100);
	kickMenu->addSeparator();
	bool canKick = MUCManager::canKick(c->s.mucItem(), lvi->s.mucItem());
	for (int i = 0; i < cntReasons; ++i) {
		act = new QAction(reasons[i], kickMenu);
		kickMenu->addAction(act);
		act->setData(101+i);
	}
	kickMenu->setEnabled(canKick);

	QMenu *banMenu = new QMenu(tr("&Ban"), pm);
	act = new QAction(tr("No reason"), banMenu);
	banMenu->addAction(act);
	act->setData(11);
	act = new QAction(tr("Custom reason"), banMenu);
	banMenu->addAction(act);
	act->setData(200);
	banMenu->addSeparator();
	bool canBan = MUCManager::canBan(c->s.mucItem(), lvi->s.mucItem());
	for (int i = 0; i < cntReasons; ++i) {
		act = new QAction(reasons[i], banMenu);
		banMenu->addAction(act);
		act->setData(201+i);
	}
	banMenu->setEnabled(canBan);

	pm->addMenu(kickMenu);
	kickMenu->menuAction()->setEnabled(canKick);
	pm->addMenu(banMenu);
	banMenu->menuAction()->setEnabled(canBan);

	QMenu* rm = new QMenu(tr("Change Role"), pm);
	act = new QAction(tr("Visitor"), rm);
	rm->addAction(act);
	act->setData(12);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().role() == MUCItem::Visitor);
	act->setEnabled( (!self || lvi->s.mucItem().role() == MUCItem::Visitor) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Visitor) );

	act = new QAction(tr("Participant"), rm);
	rm->addAction(act);
	act->setData(13);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().role() == MUCItem::Participant);
	act->setEnabled( (!self || lvi->s.mucItem().role() == MUCItem::Participant) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Participant));

	act = new QAction(tr("Moderator"), rm);
	rm->addAction(act);
	act->setData(14);
	act->setCheckable(true);
	act->setChecked( lvi->s.mucItem().role() == MUCItem::Moderator);
	act->setEnabled( (!self || lvi->s.mucItem().role() == MUCItem::Moderator) && MUCManager::canSetRole(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Moderator));
	pm->addMenu(rm);

	/*
	QMenu* am = new QMenu(tr("Change Affiliation"), pm);
	act = am->addAction(tr("Unaffiliated"));
	act->setData(15);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().affiliation() == MUCItem::NoAffiliation);
	act->setEnabled((!self || lvi->s.mucItem().affiliation() == MUCItem::NoAffiliation) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::NoAffiliation));

	act = am->addAction(tr("Member"));
	act->setData(16);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().affiliation() == MUCItem::Member);
	act->setEnabled((!self || lvi->s.mucItem().affiliation() == MUCItem::Member) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Member));

	act = am->addAction(tr("Administrator"));
	act->setData(17);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().affiliation() == MUCItem::Admin);
	act->setEnabled((!self || lvi->s.mucItem().affiliation() == MUCItem::Admin) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Admin));

	act = am->addAction(tr("Owner"));
	act->setData(18);
	act->setCheckable(true);
	act->setChecked(lvi->s.mucItem().affiliation() == MUCItem::Owner);
	act->setEnabled((!self || lvi->s.mucItem().affiliation() == MUCItem::Owner) && MUCManager::canSetAffiliation(c->s.mucItem(),lvi->s.mucItem(),MUCItem::Owner));

	pm->addMenu(am);
	*/
	pm->addSeparator();
	//pm->insertItem(tr("Send &File"), 4);
	//pm->insertSeparator();
	//pm->insertItem(tr("Check &Status"), 2);

	act = new QAction(IconsetFactory::icon("psi/vCard").icon(), tr("User &Info"), pm);
	pm->addAction(act);
	act->setData(3);

	int x = -1;
	bool enabled = false;
	act = pm->exec(QCursor::pos());
	if(act) {
		x = act->data().toInt();
		enabled = act->isEnabled();
	}
	delete pm;

	if(x == -1 || !enabled || lvi.isNull())
		return;
	action(lvi->text(0), lvi->s, x);
}

void GCUserView::mousePressEvent(QMouseEvent *event)
{
	QTreeWidget::mousePressEvent(event);
	QTreeWidgetItem *item = itemAt(event->pos());

	if (!item || !item->parent() || !gcDlg_)
		return;
	if (event->button() == Qt::MidButton ||
	    (event->button() == Qt::LeftButton &&
	    qApp->keyboardModifiers() == Qt::ShiftModifier))
	{
		emit insertNick(item->text(0));
	}
	else if (event->button() == Qt::RightButton)
		contextMenuRequested(event->pos());
}

#include "gcuserview.moc"
