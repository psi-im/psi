/*
 * gcuserview.cpp - groupchat roster
 * Copyright (C) 2001, 2002  Justin Karneges
 * 2011 Evgeny Khryukin
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

#include "gcuserview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QItemDelegate>
#include <QMimeData>
#include <QMenu>

#include "psitooltip.h"
#include "psiaccount.h"
#include "userlist.h"
#include "psiiconset.h"
#include "groupchatdlg.h"
#include "common.h"
#include "psioptions.h"
#include "coloropt.h"
#include "avcall/avcall.h"
#include "xmpp_muc.h"
#include "xmpp_caps.h"
#include "avatars.h"

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
		updateSettings();
	}

	void updateSettings()
	{
		PsiOptions *o = PsiOptions::instance();
		colorForeground_  = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-foreground");
		colorBackground_  = ColorOpt::instance()->color("options.ui.look.colors.contactlist.grouping.header-background");
		colorModerator_   = o->getOption("options.ui.look.colors.muc.role-moderator").value<QColor>();
		colorParticipant_ = o->getOption("options.ui.look.colors.muc.role-participant").value<QColor>();
		colorVisitor_     = o->getOption("options.ui.look.colors.muc.role-visitor").value<QColor>();
		colorNoRole_      = o->getOption("options.ui.look.colors.muc.role-norole").value<QColor>();
		showGroups_       = o->getOption("options.ui.muc.userlist.show-groups").toBool();
		slimGroups_       = o->getOption("options.ui.muc.userlist.use-slim-group-headings").toBool();
		nickColoring_     = o->getOption("options.ui.muc.userlist.nick-coloring").toBool();
		showClients_      = o->getOption("options.ui.muc.userlist.show-client-icons").toBool();
		showAffiliations_ = o->getOption("options.ui.muc.userlist.show-affiliation-icons").toBool();
		showStatusIcons_  = o->getOption("options.ui.muc.userlist.show-status-icons").toBool();
		showAvatar_       = o->getOption("options.ui.muc.userlist.avatars.show").toBool();
		avatarSize_       = o->getOption("options.ui.muc.userlist.avatars.size").toInt();
		avatarAtLeft_     = o->getOption("options.ui.muc.userlist.avatars.avatars-at-left").toBool();
		avatarRadius_     = o->getOption("options.ui.muc.userlist.avatars.radius").toInt();

		QFont font;
		font.fromString(o->getOption("options.ui.look.font.contactlist").toString());
		fontHeight_ = QFontMetrics(font).height()+2;
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
				paintContact(mp, option, index, (GCUserViewItem*)i);
			}
		}
	}

	void paintGroup(QPainter* p, const QStyleOptionViewItem& o, GCUserViewGroupItem* gi) const
	{
		if(!showGroups_)
			return;

		QRect rect = o.rect;
		QFont f = o.font;
		p->setFont(f);
		if (!slimGroups_ || (o.state & QStyle::State_Selected) ) {
			p->fillRect(rect, colorBackground_);
		}

		p->setPen(QPen(colorForeground_));
		rect.translate(2, (rect.height() - o.fontMetrics.height())/2);
		p->drawText(rect, gi->text(0));
		if (slimGroups_	&& !(o.state & QStyle::State_Selected))
		{
			QFontMetrics fm(f);
			int x = fm.width(gi->text(0)) + 8;
			int width = rect.width();
			if(x < width - 8) {
				int h = rect.y() + (rect.height() / 2) - 1;
				p->setPen(QPen(colorBackground_));
				p->drawLine(x, h, width - 8, h);
				h++;
				p->setPen(QPen(colorForeground_));
				p->drawLine(x, h, width - 8, h);
			}
		}
	}

	void paintContact(QPainter* mp, const QStyleOptionViewItem& option, const QModelIndex& index, GCUserViewItem* item) const
	{
		mp->save();
		QStyleOptionViewItem o = option;
		QPalette palette = o.palette;
		MUCItem::Role r = item->s.mucItem().role();
		QRect rect = o.rect;

		if(nickColoring_) {
			if(r == MUCItem::Moderator)
				palette.setColor(QPalette::Text, colorModerator_);
			else if(r == MUCItem::Participant)
				palette.setColor(QPalette::Text, colorParticipant_);
			else if(r == MUCItem::Visitor)
				palette.setColor(QPalette::Text, colorVisitor_);
			else
				palette.setColor(QPalette::Text, colorNoRole_);
		}

		mp->fillRect(rect, (o.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base));

		if(showAvatar_) {
			QPixmap ava = item->avatar();
			if(ava.isNull()) {
				ava = IconsetFactory::iconPixmap("psi/default_avatar");
			}
			ava = AvatarFactory::roundedAvatar(ava, avatarRadius_, avatarSize_);
			QRect avaRect(rect);
			avaRect.setWidth(ava.width());
			avaRect.setHeight(ava.height());
			if(!avatarAtLeft_) {
				avaRect.moveTopRight(rect.topRight());
				avaRect.translate(-1, 1);
				rect.setRight(avaRect.left() - 1);
			}
			else {
				avaRect.translate(1, 1);
				rect.setLeft(avaRect.right() + 1);
			}
			mp->drawPixmap(avaRect, ava);
		}

		QPixmap status = showStatusIcons_ ? item->icon() : QPixmap();
		int h = rect.height();
		int sh = status.isNull() ? 0 : status.height();
		rect.setHeight(qMax(sh, fontHeight_));
		rect.moveTop(rect.top() + (h - rect.height())/2);
		if(!status.isNull()) {
			QRect statusRect(rect);
			statusRect.setWidth(status.width());
			statusRect.setHeight(status.height());
			statusRect.translate(1, 1);
			mp->drawPixmap(statusRect, status);
			rect.setLeft(statusRect.right() + 2);
		}
		else
			rect.setLeft(rect.left() + 2);

		mp->setPen(QPen((o.state & QStyle::State_Selected) ? palette.color(QPalette::HighlightedText) : palette.color(QPalette::Text)));
		mp->setFont(o.font);
		mp->setClipRect(rect);
		QTextOption to;
		to.setWrapMode(QTextOption::NoWrap);
		mp->drawText(rect, index.data(Qt::DisplayRole).toString(), to);

		QList<QPixmap> rightPixs;

		if(showAffiliations_) {
			MUCItem::Affiliation a = item->s.mucItem().affiliation();
			QPixmap pix;
			if(a == MUCItem::Owner)
				pix = IconsetFactory::iconPixmap("affiliation/owner");
			else if(a == MUCItem::Admin)
				pix = IconsetFactory::iconPixmap("affiliation/admin");
			else if(a == MUCItem::Member)
				pix = IconsetFactory::iconPixmap("affiliation/member");
			else if(a == MUCItem::Outcast)
				pix = IconsetFactory::iconPixmap("affiliation/outcast");
			else
				pix = IconsetFactory::iconPixmap("affiliation/noaffiliation");
			if(!pix.isNull())
				rightPixs.push_back(pix);
		}

		if(showClients_) {
			GCUserView *gcuv = (GCUserView*)item->treeWidget();
			GCMainDlg* dlg = gcuv->mainDlg();
			QPixmap clientPix;
			if(dlg) {
				UserListItem u;
				const QString &nick = item->text(0);
				Jid caps_jid(/*s.mucItem().jid().isEmpty() ? */ dlg->jid().withResource(nick) /* : s.mucItem().jid()*/);
				CapsManager *cm = dlg->account()->client()->capsManager();
				QString client_name = cm->clientName(caps_jid);
				QString client_version = (client_name.isEmpty() ? QString() : cm->clientVersion(caps_jid));
				UserResource ur;
				ur.setStatus(item->s);
				ur.setClient(client_name,client_version,"");
				u.userResourceList().append(ur);
				QStringList clients = u.clients();
				if(!clients.isEmpty())
					clientPix = IconsetFactory::iconPixmap("clients/" + clients.takeFirst());
			}
			if(!clientPix.isNull())
				rightPixs.push_back(clientPix);
		}

		mp->restore();

		if(rightPixs.isEmpty())
			return;

		int sumWidth = 0;
		foreach (const QPixmap& pix, rightPixs) {
				sumWidth += pix.width();
		}
		sumWidth += rightPixs.count();

		QColor bgc = (option.state & QStyle::State_Selected) ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
		QColor tbgc = bgc;
		tbgc.setAlpha(0);
		QLinearGradient grad(rect.right() - sumWidth - 20, 0, rect.right() - sumWidth, 0);
		grad.setColorAt(0, tbgc);
		grad.setColorAt(1, bgc);
		QBrush tbakBr(grad);
		QRect gradRect(rect);
		gradRect.setLeft(gradRect.right() - sumWidth - 20);
		mp->fillRect(gradRect, tbakBr);

		QRect iconRect(rect);
		for (int i=0; i<rightPixs.size(); i++) {
			const QPixmap pix = rightPixs[i];
			iconRect.setRight(iconRect.right() - pix.width() -1);
			mp->drawPixmap(iconRect.topRight(), pix);
		}

	}

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		if(!index.isValid())
			return QSize(0,0);

		QSize size = QItemDelegate::sizeHint(option, index);
		GCUserView *uv = static_cast<GCUserView*>(parent());
		if(uv) {
			QTreeWidgetItem *i = uv->findEntry(index);
			GCUserViewItem *vi = dynamic_cast<GCUserViewItem*>(i);
			if(vi) {
				int rowH = qMax(fontHeight_, vi->icon().height() + 2);
				int h = showAvatar_ ? qMax(avatarSize_ + 2, rowH) : rowH;
				size.setHeight(h);
			}
			else {
				size.setHeight(showGroups_ ? fontHeight_ : 0);
			}
		}

		return size;
	}

private:
	QColor colorForeground_, colorBackground_, colorModerator_, colorParticipant_, colorVisitor_, colorNoRole_;
	bool showGroups_, slimGroups_, nickColoring_, showClients_, showAffiliations_, showStatusIcons_, showAvatar_, avatarAtLeft_;
	int avatarSize_, fontHeight_, avatarRadius_;
};


//----------------------------------------------------------------------------
// GCUserViewItem
//----------------------------------------------------------------------------

GCUserViewItem::GCUserViewItem(GCUserViewGroupItem *par)
	: QObject()
	, QTreeWidgetItem(par)
{
}

void GCUserViewItem::setAvatar(const QPixmap &pix)
{
	avatar_ = pix;
	treeWidget()->viewport()->update();
}

void GCUserViewItem::setIcon(const QPixmap &icon)
{
	icon_ = icon;
}

bool GCUserViewItem::operator<(const QTreeWidgetItem& it) const
{
	GCUserViewItem *item = (GCUserViewItem*)(&it);
	if(PsiOptions::instance()->getOption("options.ui.muc.userlist.contact-sort-style").toString() == "status") {
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
	setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
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
	setRootIsDecorated(false);
	sortByColumn(0);
	setIndentation(0);
	setContextMenuPolicy(Qt::DefaultContextMenu);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setDragDropMode(QAbstractItemView::DragOnly);

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

QMimeData* GCUserView::mimeData(QList<QTreeWidgetItem *>items) const
{
	QMimeData* data = 0;
	if(!items.isEmpty()) {
		data = new QMimeData();
		data->setText(items.first()->text(0));
	}

	return data;
}

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
			i->setIcon(PsiIconset::instance()->status(i->s).pixmap());
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
		lvi->setAvatar(gcDlg_->account()->avatarFactory()->getMucAvatar(gcDlg_->jid().withResource(nick)));
		lvi->setText(0, nick);
		gr->updateText();
	}

	lvi->s = s;
	lvi->setIcon(PsiIconset::instance()->status(lvi->s).pixmap());
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
//	QRect r(visualItemRect(lvi));

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
	Jid caps_jid(dlg->jid().withResource(nick));
	CapsManager *cm = dlg->account()->client()->capsManager();
	QString client_name = cm->clientName(caps_jid);
	QString client_version = (client_name.isEmpty() ? QString() : cm->clientVersion(caps_jid));

	// make a resource so the contact appears online
	UserResource ur;
	ur.setName(nick);
	ur.setStatus(s);
	ur.setClient(client_name,client_version,"");
	//ur.setClient(QString(),QString(),"");
	u.userResourceList().append(ur);
	u.setPrivate(true);
	u.setAvatarFactory(dlg->account()->avatarFactory());

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

void GCUserView::mousePressEvent(QMouseEvent *event)
{
	QTreeWidgetItem *item = itemAt(event->pos());
	if (item && item->parent() && gcDlg_) {
		if (event->button() == Qt::MidButton ||
			(event->button() == Qt::LeftButton &&
			qApp->keyboardModifiers() == Qt::ShiftModifier))
		{
			emit insertNick(item->text(0));
			return;
		}
	}
	QTreeWidget::mousePressEvent(event);
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

void GCUserView::doContextMenu(QTreeWidgetItem *i)
{
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
	if (AvCallManager::isSupported()) {
		act = new QAction(IconsetFactory::icon("psi/avcall").icon(), tr("Voice Call"), pm);
		pm->addAction(act);
		act->setData(5);
	}

	act = new QAction(tr("E&xecute Command"), pm);
	pm->addAction(act);
	act->setData(6);

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
	pm->addSeparator();
	//pm->insertItem(tr("Send &File"), 4);
	//pm->insertSeparator();
	//pm->insertItem(tr("Check &Status"), 2);

	act = new QAction(IconsetFactory::icon("psi/vCard").icon(), tr("User &Info"), pm);
	pm->addAction(act);
	act->setData(3);

	const QString css = PsiOptions::instance()->getOption("options.ui.chat.css").toString();
	if (!css.isEmpty()) {
		pm->setStyleSheet(css);
	}
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

void GCUserView::contextMenuEvent(QContextMenuEvent *cm)
{
	QTreeWidgetItem *i = 0;
	i = currentItem();
	if (i && i->parent() && gcDlg_) {
		doContextMenu(i);
		return;
	}
	QTreeWidget::contextMenuEvent(cm);
}

void GCUserView::setLooks()
{
	((GCUserViewDelegate*)itemDelegate())->updateSettings();
	viewport()->update();
}

#include "gcuserview.moc"
