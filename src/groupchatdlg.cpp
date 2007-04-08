/*
 * groupchatdlg.cpp - dialogs for handling groupchat
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

// TODO: Move all the 'logic' of groupchats into MUCManager. See MUCManager
// for more details.

#include "groupchatdlg.h"

#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <QToolBar>
#include <qmessagebox.h>
#include <QPainter>
#include <QColorGroup>
#include <qsplitter.h>
#include <qtimer.h>
#include <q3header.h>
#include <qtoolbutton.h>
#include <qinputdialog.h>
#include <qpointer.h>
#include <qaction.h>
#include <qobject.h>
#include <q3popupmenu.h>
#include <qcursor.h>
#include <QCloseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QFrame>
#include <QList>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QTextCursor>
#include <QTextDocument> // for Qt::escape()

#include "psicon.h"
#include "psiaccount.h"
#include "capsmanager.h"
#include "userlist.h"
#include "mucconfigdlg.h"
#include "textutil.h"
#include "statusdlg.h"
#include "psiiconset.h"
#include "stretchwidget.h"
#include "mucmanager.h"
#include "busywidget.h"
#include "common.h"
#include "msgmle.h"
#include "iconwidget.h"
#include "iconselect.h"
#include "psitoolbar.h"
#include "iconaction.h"
#include "psitooltip.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "psicontactlist.h"
#include "accountlabel.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

//----------------------------------------------------------------------------
// GCUserViewItem
//----------------------------------------------------------------------------

GCUserViewItem::GCUserViewItem(GCUserViewGroupItem *par)
:Q3ListViewItem(par)
{
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

GCUserViewGroupItem::GCUserViewGroupItem(GCUserView *par, const QString& t)
:Q3ListViewItem(par,t)
{
}

void GCUserViewGroupItem::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment)
{
	QColorGroup xcg = cg;
	QFont f = p->font();
	f.setPointSize(option.smallFontSize);
	p->setFont(f);
	xcg.setColor(QColorGroup::Text, option.color[cGroupFore]);
	if (!option.clNewHeadings) {
		#if QT_VERSION >= 0x040103 
			xcg.setColor(QColorGroup::Background, option.color[cGroupBack]);
		#else
			xcg.setColor(QColorGroup::WindowText, option.color[cGroupBack]);
		#endif
	}
	Q3ListViewItem::paintCell(p, xcg, column, width, alignment);
	if (option.clNewHeadings && !isSelected()) {
		QFontMetrics fm(p->font());
		int x = fm.width(text(column)) + 8;
		if(x < width - 8) {
			int h = (height() / 2) - 1;
			p->setPen(QPen(option.color[cGroupBack]));
			p->drawLine(x, h, width - 8, h);
			h++;
			p->setPen(QPen(option.color[cGroupFore]));
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


//----------------------------------------------------------------------------
// GCUserView
//----------------------------------------------------------------------------

GCUserView::GCUserView(GCMainDlg* dlg, QWidget *parent, const char *name)
:Q3ListView(parent, name)
{
	gcDlg_ = dlg;
	setResizeMode(Q3ListView::AllColumns);
	setTreeStepSize(0);
	setShowToolTips(false);
	header()->hide();
	addColumn("");
	setSortColumn(-1);
	Q3ListViewItem* i;
	i = new GCUserViewGroupItem(this, tr("Visitors"));
	i->setOpen(true);
	i = new GCUserViewGroupItem(this, tr("Participants"));
	i->setOpen(true);
	i = new GCUserViewGroupItem(this, tr("Moderators"));
	i->setOpen(true);

	connect(this, SIGNAL(doubleClicked(Q3ListViewItem *)), SLOT(qlv_doubleClicked(Q3ListViewItem *)));
	connect(this, SIGNAL(contextMenuRequested(Q3ListViewItem *, const QPoint &, int)), SLOT(qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int)));
}

GCUserView::~GCUserView()
{
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
			i->setPixmap(0, PsiIconset::instance()->status(i->s));
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
	lvi->setPixmap(0, PsiIconset::instance()->status(lvi->s));
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
	GCMainDlg* dlg = (GCMainDlg*) topLevelWidget();
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
	if(option.defaultAction == 0)
		action(lvi->text(0), lvi->s, 0);
	else
		action(lvi->text(0), lvi->s, 1);
}

void GCUserView::qlv_contextMenuRequested(Q3ListViewItem *i, const QPoint &pos, int)
{
	if(!i || !i->parent())
		return;

	GCUserViewItem *lvi = (GCUserViewItem *)i;
	bool self = gcDlg_->nick() == i->text(0);
	GCUserViewItem* c = (GCUserViewItem*) findEntry(gcDlg_->nick());
	if (!c) {
		qWarning(QString("groupchatdlg.cpp: Self ('%1') not found in contactlist").arg(gcDlg_->nick()));
		return;
	}
	Q3PopupMenu *pm = new Q3PopupMenu;
	pm->insertItem(IconsetFactory::icon("psi/sendMessage"), tr("Send &message"), 0);
	pm->insertItem(IconsetFactory::icon("psi/start-chat"), tr("Open &chat window"), 1);
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
	pm->insertItem(tr("Change role"),rm);

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
	pm->insertItem(tr("Change affiliation"),am);*/
	pm->insertSeparator();
	//pm->insertItem(tr("Send &file"), 4);
	//pm->insertSeparator();
	pm->insertItem(tr("Check &Status"), 2);
	pm->insertItem(IconsetFactory::icon("psi/vCard"), tr("User &Info"), 3);
	int x = pm->exec(pos);
	bool enabled = pm->isItemEnabled(x) || rm->isItemEnabled(x);
	delete pm;

	if(x == -1 || !enabled)
		return;
	action(lvi->text(0), lvi->s, x);
}


//----------------------------------------------------------------------------
// GCMainDlg
//----------------------------------------------------------------------------
class GCMainDlg::Private : public QObject
{
	Q_OBJECT
public:
	enum { Connecting, Connected, Idle };
	Private(GCMainDlg *d) {
		dlg = d;
		nickSeparator = ":";
		typingStatus = Typing_Normal;
		
		trackBar = false;
		oldTrackBarPosition = 0;
	}

	GCMainDlg *dlg;
	int state;
	PsiAccount *pa;
	MUCManager *mucManager;
	Jid jid;
	QString self;
	QString password;
	ChatView *te_log;
	ChatEdit *mle;
	QLineEdit *le_topic;
	GCUserView *lv_users;
	QPushButton *pb_topic;
	QToolBar *toolbar;
	IconAction *act_find, *act_clear, *act_icon, *act_configure;
	QAction *act_send, *act_scrollup, *act_scrolldown, *act_close;
	QLabel* lb_ident;
	Q3PopupMenu *pm_settings;
	bool smallChat;
	int pending;
	bool connecting;

	QTimer *flashTimer;
	int flashCount;

	QStringList hist;
	int histAt;

	QPointer<GCFindDlg> findDlg;
	QString lastSearch;

	QPointer<MUCConfigDlg> configDlg;
	
public:
	bool trackBar;
protected:
	int  oldTrackBarPosition;

public slots:
	void addEmoticon(const Icon *icon) {
		if ( !dlg->isActiveWindow() )
		     return;

		QString text;

		QHash<QString,QString> itext = icon->text();
		for ( QHash<QString,QString>::ConstIterator it = itext.begin(); it != itext.end(); ++it) {
			if (  !it->isEmpty() ) {
				text = (*it) + " ";
				break;
			}
		}

		if ( !text.isEmpty() )
			mle->insert( text );
	}

	void addEmoticon(QString text) {
		if ( !dlg->isActiveWindow() )
		     return;

		mle->insert( text + " " );
	}

	void deferredScroll() {
		//QTimer::singleShot(250, this, SLOT(slotScroll()));
		te_log->scrollToBottom();
	}

protected slots:
	void slotScroll() {
		te_log->scrollToBottom();
	}

public:
	bool internalFind(QString str, bool startFromBeginning = false)
	{
		if (startFromBeginning) {
			QTextCursor cursor = te_log->textCursor();
			cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
			cursor.clearSelection();
			te_log->setTextCursor(cursor);
		}
		
		bool found = te_log->find(str);
		if(!found) {
			if (!startFromBeginning)
				return internalFind(str, true);
			
			return false;
		}

		return true;
	}
	
private:
	void removeTrackBar(QTextCursor &cursor)
	{
		if (oldTrackBarPosition) {
			cursor.setPosition(oldTrackBarPosition, QTextCursor::KeepAnchor);
			QTextBlockFormat blockFormat = cursor.blockFormat();
			blockFormat.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
			cursor.clearSelection();
			cursor.setBlockFormat(blockFormat);
		}
	}
		
	void addTrackBar(QTextCursor &cursor)
	{
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		oldTrackBarPosition = cursor.position();
		QTextBlockFormat blockFormat = cursor.blockFormat();
		blockFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QVariant(true));
		cursor.clearSelection();
		cursor.setBlockFormat(blockFormat);
	}

public:		
	void doTrackBar()
	{
		trackBar = false;

		// save position, because our manipulations could change it
		int scrollbarValue = te_log->verticalScrollBar()->value();

		QTextCursor cursor = te_log->textCursor();
		cursor.beginEditBlock();
		PsiTextView::Selection selection = te_log->saveSelection(cursor);

		removeTrackBar(cursor);
		addTrackBar(cursor);

		te_log->restoreSelection(cursor, selection);
		cursor.endEditBlock();
		te_log->setTextCursor(cursor);

		te_log->verticalScrollBar()->setValue(scrollbarValue);
	}

public:
	QString lastReferrer;  // contains nick of last person, who have said "yourNick: ..."
protected:		
	// Nick auto-completion code follows...
	enum TypingStatus {
		Typing_Normal = 0,
		Typing_TabPressed,
		Typing_TabbingNicks,
		Typing_MultipleSuggestions
	};
	TypingStatus typingStatus;
	QString nickSeparator; // in case of "nick: ...", it equals ":"
	QStringList suggestedNicks;
	int  suggestedIndex;
	bool suggestedFromStart;

	QString beforeNickText(QString text) {
		int i;
		for (i = text.length() - 1; i >= 0; --i)
			if ( text[i].isSpace() )
				break;

		QString beforeNick = text.left(i+1);
		return beforeNick;
	}

	QStringList suggestNicks(QString text, bool fromStart) {
		QString nickText = text;
		QString beforeNick;
		if ( !fromStart ) {
			beforeNick = beforeNickText(text);
			nickText   = text.mid(beforeNick.length());
		}

		QStringList nicks = lv_users->nickList();
		QStringList::Iterator it = nicks.begin();
		QStringList suggestedNicks;
		for ( ; it != nicks.end(); ++it) {
			if ( (*it).left(nickText.length()).lower() == nickText.lower() ) {
				if ( fromStart )
					suggestedNicks << *it;
				else
					suggestedNicks << beforeNick + *it;
			}
		}

		return suggestedNicks;
	}

	QString longestSuggestedString(QStringList suggestedNicks) {
		QString testString = suggestedNicks.first();
		while ( testString.length() > 0 ) {
			bool found = true;
			QStringList::Iterator it = suggestedNicks.begin();
			for ( ; it != suggestedNicks.end(); ++it) {
				if ( (*it).left(testString.length()).lower() != testString.lower() ) {
					found = false;
					break;
				}
			}

			if ( found )
				break;

			testString = testString.left( testString.length() - 1 );
		}

		return testString;
	}

	QString insertNick(bool fromStart, QString beforeNick = "") {
		typingStatus = Typing_MultipleSuggestions;
		suggestedFromStart = fromStart;
		suggestedNicks = lv_users->nickList();
		QStringList::Iterator it = suggestedNicks.begin();
		for ( ; it != suggestedNicks.end(); ++it)
			*it = beforeNick + *it;

		QString newText;
		if ( !lastReferrer.isEmpty() ) {
			newText = beforeNick + lastReferrer;
			suggestedIndex = -1;
		}
		else {
			newText = suggestedNicks.first();
			suggestedIndex = 0;
		}

		if ( fromStart ) {
			newText += nickSeparator;
			newText += " ";
		}

		return newText;
	}

	QString suggestNick(bool fromStart, QString origText, bool *replaced) {
		suggestedFromStart = fromStart;
		suggestedNicks = suggestNicks(origText, fromStart);
		suggestedIndex = -1;

		QString newText;
		if ( suggestedNicks.count() ) {
			if ( suggestedNicks.count() == 1 ) {
				newText = suggestedNicks.first();
				if ( fromStart ) {
					newText += nickSeparator;
					newText += " ";
				}
			}
			else {
				newText = longestSuggestedString(suggestedNicks);
				if ( !newText.length() )
					return origText;

				typingStatus = Typing_MultipleSuggestions;
				// TODO: display a tooltip that will contain all suggestedNicks
			}

			*replaced = true;
		}

		return newText;
	}

public:		
	void doAutoNickInsertion() {
		QTextCursor cursor = mle->textCursor();
		
		// we need to get index from beginning of current block
		int index = cursor.position();
		cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
		index -= cursor.position();
		
		QString paraText = cursor.block().text();
		QString origText = paraText.left(index);
		QString newText;
		
		bool replaced = false;

		if ( typingStatus == Typing_MultipleSuggestions ) {
			suggestedIndex++;
			if ( suggestedIndex >= (int)suggestedNicks.count() )
				suggestedIndex = 0;

			newText = suggestedNicks[suggestedIndex];
			if ( suggestedFromStart ) {
				newText += nickSeparator;
				newText += " ";
			}

			replaced = true;
		}

		if ( !cursor.block().position() && !replaced ) {
			if ( !index && typingStatus == Typing_TabbingNicks ) {
				newText = insertNick(true, "");
				replaced = true;
			}
			else {
				newText = suggestNick(true, origText, &replaced);
			}
		}

		if ( !replaced ) {
			if ( (!index || origText[index-1].isSpace()) && typingStatus == Typing_TabbingNicks ) {
				newText = insertNick(false, beforeNickText(origText));
				replaced = true;
			}
			else {
				newText = suggestNick(false, origText, &replaced);
			}
		}

		if ( replaced ) {
			mle->setUpdatesEnabled( false );
			int position = cursor.position() + newText.length();
			
			cursor.beginEditBlock();
			cursor.select(QTextCursor::BlockUnderCursor);
			cursor.insertText(newText + paraText.mid(index, paraText.length() - index));
			cursor.setPosition(position, QTextCursor::KeepAnchor);
			cursor.clearSelection();
			cursor.endEditBlock();
			mle->setTextCursor(cursor);
			
			mle->setUpdatesEnabled( true );
			mle->viewport()->update();
		}
	}

	bool eventFilter( QObject *obj, QEvent *ev ) {
		if (te_log->handleCopyEvent(obj, ev, mle))
			return true;
	
		if ( obj == mle && ev->type() == QEvent::KeyPress ) {
			QKeyEvent *e = (QKeyEvent *)ev;

			if ( e->key() == Qt::Key_Tab ) {
				switch ( typingStatus ) {
				case Typing_Normal:
					typingStatus = Typing_TabPressed;
					break;
				case Typing_TabPressed:
					typingStatus = Typing_TabbingNicks;
					break;
				default:
					break;
				}

				doAutoNickInsertion();
				return TRUE;
			}

			typingStatus = Typing_Normal;

			return FALSE;
		}

		return QObject::eventFilter( obj, ev );
	}
};

GCMainDlg::GCMainDlg(PsiAccount *pa, const Jid &j)
:AdvancedWidget<QWidget>(0, Qt::WDestructiveClose)
{
  	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);
	nicknumber=0;
	d = new Private(this);
	d->pa = pa;
	d->jid = j.userHost();
	d->self = j.resource();
	d->pa->dialogRegister(this, d->jid);
	connect(d->pa, SIGNAL(updatedActivity()), SLOT(pa_updatedActivity()));
	d->mucManager = new MUCManager(d->pa->client(),d->jid);

	options_ = PsiOptions::instance();

	d->pending = 0;
	d->flashTimer = 0;
	d->connecting = false;

	d->histAt = 0;
	d->findDlg = 0;
	d->configDlg = 0;

	d->state = Private::Connected;

#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/groupChat"));
#endif

	QVBoxLayout *dlg_layout = new QVBoxLayout(this, 4);

	QWidget *vsplit;
	if ( !option.chatLineEdit ) {
		vsplit = new QSplitter(this);
		((QSplitter *)vsplit)->setOrientation( Qt::Vertical );
		dlg_layout->addWidget(vsplit);
	}
	else
		vsplit = this;

	// --- top part ---
	QWidget *sp_top = new QWidget(vsplit);
	sp_top->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );
	if ( option.chatLineEdit )
		dlg_layout->addWidget( sp_top );
	QVBoxLayout *vb_top = new QVBoxLayout(sp_top, 0, 4);

	// top row
	QWidget *sp_top_top = new QWidget( sp_top );
	vb_top->addWidget( sp_top_top );
	QHBoxLayout *hb_top = new QHBoxLayout( sp_top_top, 0, 4 );

	d->pb_topic = new QPushButton(tr("Topic:"), sp_top_top);
	connect(d->pb_topic, SIGNAL(clicked()), SLOT(doTopic()));
	hb_top->addWidget(d->pb_topic);

	d->le_topic = new QLineEdit(sp_top_top);
	d->le_topic->setReadOnly(true);
	PsiToolTip::install(d->le_topic);
	hb_top->addWidget(d->le_topic);

	d->act_find = new IconAction(tr("Find"), "psi/search", tr("&Find"), 0, this);
	connect(d->act_find, SIGNAL(activated()), SLOT(openFind()));
	d->act_find->addTo( sp_top_top );
	
	d->lb_ident = new AccountLabel(d->pa, sp_top_top, true);
	d->lb_ident->setSizePolicy(QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum ));
	hb_top->addWidget(d->lb_ident);
	connect(d->pa->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	// bottom row
	QSplitter *hsp = new QSplitter(sp_top);
	hsp->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
	vb_top->addWidget(hsp);
	hsp->setOrientation(Qt::Horizontal);

	d->te_log = new ChatView(hsp);
#ifdef Q_WS_MAC
	connect(d->te_log,SIGNAL(selectionChanged()),SLOT(logSelectionChanged()));
	d->te_log->setFocusPolicy(Qt::NoFocus);
#endif

	d->lv_users = new GCUserView(this,hsp);
	d->lv_users->setMinimumWidth(20);
	connect(d->lv_users, SIGNAL(action(const QString &, const Status &, int)), SLOT(lv_action(const QString &, const Status &, int)));

	// --- bottom part ---
	QWidget *sp_bottom = new QWidget(vsplit);
	sp_bottom->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Maximum );
	if ( option.chatLineEdit )
		dlg_layout->addWidget( sp_bottom );
	QVBoxLayout *vb_bottom = new QVBoxLayout(sp_bottom, 0, 4);

	// toolbar
	d->act_clear = new IconAction (tr("Clear chat window"), "psi/clearChat", tr("Clear chat window"), 0, this);
	connect( d->act_clear, SIGNAL( activated() ), SLOT( doClearButton() ) );
	
	d->act_configure = new IconAction(tr("Configure Room"), "psi/configure-room", tr("&Configure Room"), 0, this);
	connect(d->act_configure, SIGNAL(activated()), SLOT(configureRoom()));

	connect(pa->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), d, SLOT(addEmoticon(QString)));
	d->act_icon = new IconAction( tr( "Select icon" ), "psi/smile", tr( "Select icon" ), 0, this );
	d->act_icon->setMenu( pa->psi()->iconSelectPopup() );

	d->toolbar = new QToolBar( tr("Groupchat toolbar"), 0);
	d->toolbar->setIconSize(QSize(16,16));
	d->toolbar->addAction(d->act_clear);
	d->toolbar->addAction(d->act_configure);
	d->toolbar->addWidget(new StretchWidget(d->toolbar));
	d->toolbar->addAction(d->act_icon);
	d->toolbar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
	vb_bottom->addWidget( d->toolbar );

	// Common actions
	d->act_send = new QAction(this);
	addAction(d->act_send);
	connect(d->act_send,SIGNAL(activated()), SLOT(mle_returnPressed()));
	d->act_close = new QAction(this);
	addAction(d->act_close);
	connect(d->act_close,SIGNAL(activated()), SLOT(close()));
	d->act_scrollup = new QAction(this);
	addAction(d->act_scrollup);
	connect(d->act_scrollup,SIGNAL(activated()), SLOT(scrollUp()));
	d->act_scrolldown = new QAction(this);
	addAction(d->act_scrolldown);
	connect(d->act_scrolldown,SIGNAL(activated()), SLOT(scrollDown()));

	// chat edit
	if ( !option.chatLineEdit ) {
		d->mle = new ChatEdit(sp_bottom, this);
		vb_bottom->addWidget(d->mle);
	}
	else {
		QHBoxLayout *hb5 = new QHBoxLayout( dlg_layout );
		d->mle = new LineEdit( vsplit, this );
#ifdef Q_WS_MAC
		hb5->addSpacing( 16 );
#endif
		hb5->addWidget( d->mle );
#ifdef Q_WS_MAC
		hb5->addSpacing( 16 );
#endif
	}

	d->mle->installEventFilter( d );
	d->mle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	d->pm_settings = new Q3PopupMenu(this);
	connect(d->pm_settings, SIGNAL(aboutToShow()), SLOT(buildMenu()));

	// resize the horizontal splitter
	QList<int> list;
	list << 500;
	list << 80;
	hsp->setSizes(list);

	list.clear();
	list << 324;
	list << 10;
	if ( !option.chatLineEdit )
		(( QSplitter *)vsplit)->setSizes(list);

	resize(580,420);

	d->smallChat = option.smallChats;
	X11WM_CLASS("groupchat");

	d->mle->setFocus();

	setAcceptDrops(true);

	// Connect signals from MUC manager
	connect(d->mucManager,SIGNAL(action_error(MUCManager::Action, int, const QString&)), SLOT(action_error(MUCManager::Action, int, const QString&)));

	setLooks();
	setShortcuts();
	updateCaption();
	setConnecting();
}

GCMainDlg::~GCMainDlg()
{
	if(d->state != Private::Idle)
		d->pa->groupChatLeave(d->jid.host(), d->jid.user());

	//QMimeSourceFactory *m = d->te_log->mimeSourceFactory();
	//d->te_log->setMimeSourceFactory(0);
	//delete m;

	d->pa->dialogUnregister(this);
	delete d->mucManager;
	delete d;
}

void GCMainDlg::setShortcuts()
{
	d->act_clear->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
	d->act_find->setShortcuts(ShortcutManager::instance()->shortcuts("chat.find"));
	//d->act_send->setShortcuts(ShortcutManager::instance()->shortcuts("chat.send"));
	//d->act_close->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	d->act_scrollup->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-up"));
	d->act_scrolldown->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-down"));
}

void GCMainDlg::scrollUp() {
	d->te_log->verticalScrollBar()->setValue(d->te_log->verticalScrollBar()->value() - d->te_log->verticalScrollBar()->pageStep()/2);
}

void GCMainDlg::scrollDown() {
	d->te_log->verticalScrollBar()->setValue(d->te_log->verticalScrollBar()->value() + d->te_log->verticalScrollBar()->pageStep()/2);
}

// FIXME: This should be unnecessary, since these keys are all registered as
// actions in the constructor. Somehow, Qt ignores this.
void GCMainDlg::keyPressEvent(QKeyEvent *e)
{
	QKeySequence key = e->key() + ( e->modifiers() & ~Qt::KeypadModifier);
	if(!option.useTabs && ShortcutManager::instance()->shortcuts("common.close").contains(key))
		close();
	else if(ShortcutManager::instance()->shortcuts("chat.send").contains(key))
		mle_returnPressed();
	else if(ShortcutManager::instance()->shortcuts("common.scroll-up").contains(key))
		scrollUp();
	else if(ShortcutManager::instance()->shortcuts("common.scroll-down").contains(key))
		scrollDown();
	else
		e->ignore();
}

void GCMainDlg::closeEvent(QCloseEvent *e)
{
	e->accept();
}

void GCMainDlg::windowActivationChange(bool oldstate)
{
	QWidget::windowActivationChange(oldstate);

	// if we're bringing it to the front, get rid of the '*' if necessary
	if(isActiveWindow()) {
		if(d->pending > 0) {
			d->pending = 0;
			updateCaption();
		}
		doFlash(false);

		d->mle->setFocus();
		d->trackBar = false;
	} else {
		d->trackBar = true;
	}
}

void GCMainDlg::mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason)
{
	QString m = message;
	
	if (!actor.isEmpty())
		m += tr(" by %1").arg(actor.full());
	m += ".";
	
	if (!reason.isEmpty())
		m += tr("\nReason: %1").arg(reason);

	QMessageBox::information(this, title, m);
}

void GCMainDlg::logSelectionChanged()
{
#ifdef Q_WS_MAC
	// A hack to only give the message log focus when text is selected
	if (d->te_log->hasSelectedText()) 
		d->te_log->setFocus();
	else 
		d->mle->setFocus();
#endif
}

void GCMainDlg::setConnecting()
{
	d->connecting = true;
	QTimer::singleShot(5000,this,SLOT(unsetConnecting()));
}

void GCMainDlg::updateIdentityVisibility()
{
	d->lb_ident->setVisible(d->pa->psi()->contactList()->enabledAccounts().count() > 1);
}

void GCMainDlg::unsetConnecting()
{
	d->connecting = false;
}

void GCMainDlg::action_error(MUCManager::Action, int, const QString& err) 
{
	appendSysMsg(err, false);
}

void GCMainDlg::mle_returnPressed()
{
	if(d->mle->text().isEmpty())
		return;

	QString str = d->mle->text();
	if(str == "/clear") {
		doClear();

		d->histAt = 0;
		d->hist.prepend(str);
		d->mle->setText("");
		return;
	}

	if(str.lower().startsWith("/nick ")) {
		QString nick = str.mid(6).stripWhiteSpace();
		if ( !nick.isEmpty() ) {
			d->self = nick;
			d->pa->groupChatChangeNick(d->jid.host(), d->jid.user(), d->self, d->pa->status());
		}
		d->mle->setText("");
		return;
	}

	if(d->state != Private::Connected)
		return;

	Message m(d->jid);
	m.setType("groupchat");
	m.setBody(str);
	m.setTimeStamp(QDateTime::currentDateTime());

	aSend(m);

	d->histAt = 0;
	d->hist.prepend(str);
	d->mle->setText("");
}

/*void GCMainDlg::le_upPressed()
{
	if(d->histAt < (int)d->hist.count()) {
		++d->histAt;
		d->le_input->setText(d->hist[d->histAt-1]);
	}
}

void GCMainDlg::le_downPressed()
{
	if(d->histAt > 0) {
		--d->histAt;
		if(d->histAt == 0)
			d->le_input->setText("");
		else
			d->le_input->setText(d->hist[d->histAt-1]);
	}
}*/

void GCMainDlg::doTopic()
{
	bool ok = false;
	QString str = QInputDialog::getText(
		tr("Set Groupchat Topic"),
		tr("Enter a topic:"),
		QLineEdit::Normal, d->le_topic->text(), &ok, this);

	if(ok) {
		Message m(d->jid);
		m.setType("groupchat");
		m.setSubject(str);
		m.setTimeStamp(QDateTime::currentDateTime());
		aSend(m);
	}
}

void GCMainDlg::doClear()
{
	d->te_log->setText("");
}

void GCMainDlg::doClearButton()
{
	int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"), tr("&Yes"), tr("&No"));
	if(n == 0)
		doClear();
}

void GCMainDlg::openFind()
{
	if(d->findDlg)
		bringToFront(d->findDlg);
	else {
		d->findDlg = new GCFindDlg(d->lastSearch, this);
		connect(d->findDlg, SIGNAL(find(const QString &)), SLOT(doFind(const QString &)));
		d->findDlg->show();
	}
}

void GCMainDlg::configureRoom()
{
	if(d->configDlg)
		bringToFront(d->configDlg);
	else {
		GCUserViewItem* c = (GCUserViewItem*) d->lv_users->findEntry(d->self);
		d->configDlg = new MUCConfigDlg(d->mucManager, this);
		d->configDlg->setRoleAffiliation(c->s.mucItem().role(), c->s.mucItem().affiliation());
		d->configDlg->show();
	}
}

void GCMainDlg::doFind(const QString &str)
{
	d->lastSearch = str;
	if (d->internalFind(str))
		d->findDlg->found();
	else
		d->findDlg->error(str);
}

void GCMainDlg::goDisc()
{
	if(d->state != Private::Idle) {
		d->state = Private::Idle;
		d->pb_topic->setEnabled(false);
		appendSysMsg(tr("Disconnected."), true);
		d->mle->setEnabled(false);
	}
}

void GCMainDlg::goConn()
{
	if(d->state == Private::Idle) {
		d->state = Private::Connecting;
		appendSysMsg(tr("Reconnecting..."), true);

		QString host = d->jid.host();
		QString room = d->jid.user();
		QString nick = d->self;

		if(!d->pa->groupChatJoin(host, room, nick, d->password)) {
			appendSysMsg(tr("Error: You are in or joining this room already!"), true);
			d->state = Private::Idle;
		}
	}
}

void GCMainDlg::dragEnterEvent(QDragEnterEvent *e)
{
	e->accept(e->mimeData()->hasText());
}

void GCMainDlg::dropEvent(QDropEvent *e)
{
	Jid jid(e->mimeData()->text());
	if (jid.isValid()) {
		Message m;
		m.setTo(d->jid);
		m.addMUCInvite(MUCInvite(jid));
		if (!d->password.isEmpty())
			m.setMUCPassword(d->password);
		m.setTimeStamp(QDateTime::currentDateTime());
		d->pa->dj_sendMessage(m);
	}
}


void GCMainDlg::pa_updatedActivity()
{
	if(!d->pa->loggedIn())
		goDisc();
	else {
		if(d->state == Private::Idle)
			goConn();
		else if(d->state == Private::Connected)
			d->pa->groupChatSetStatus(d->jid.host(), d->jid.user(), d->pa->status());
	}
}

Jid GCMainDlg::jid() const
{
	return d->jid;
}

PsiAccount* GCMainDlg::account() const
{
	return d->pa;
}

void GCMainDlg::error(int, const QString &str)
{
	d->pb_topic->setEnabled(false);

	if(d->state == Private::Connecting)
		appendSysMsg(tr("Unable to join groupchat.  Reason: %1").arg(str), true);
	else
		appendSysMsg(tr("Unexpected groupchat error: %1").arg(str), true);

	d->state = Private::Idle;
}

void GCMainDlg::presence(const QString &nick, const Status &s)
{
	if(s.hasError()) {
		QString message;
		if (s.errorCode() == 409)
			message = tr("Please choose a different nickname");
		else
			message = tr("An error occurred");
		appendSysMsg(message, false, QDateTime::currentDateTime());
	}

	if (nick == d->self) {
		// Update configuration dialog
		if (d->configDlg) 
			d->configDlg->setRoleAffiliation(s.mucItem().role(),s.mucItem().affiliation());
		d->act_configure->setEnabled(s.mucItem().affiliation() >= MUCItem::Member);
	}
	
	if(s.isAvailable()) {
		// Available
		if (s.mucStatus() == 201) {
			appendSysMsg(tr("New room created"), false, QDateTime::currentDateTime());
			if (options_->getOption("options.muc.accept-defaults").toBool())
				d->mucManager->setDefaultConfiguration();
			else if (options_->getOption("options.muc.auto-configure").toBool())
				QTimer::singleShot(0, this, SLOT(configureRoom()));
		}

		GCUserViewItem* contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact == NULL) {
			//contact joining
			if ( !d->connecting && options_->getOption("options.muc.show-joins").toBool() ) {
				QString message = tr("%1 has joined the room");

				if ( options_->getOption("options.muc.show-role-affiliation").toBool() ) {
					if (s.mucItem().role() != MUCItem::NoRole) {
						if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
							message = tr("%3 has joined the room as %1 and %2").arg(MUCManager::roleToString(s.mucItem().role(),true)).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
						}
						else {
							message = tr("%2 has joined the room as %1").arg(MUCManager::roleToString(s.mucItem().role(),true));
						}
					}
					else if (s.mucItem().affiliation() != MUCItem::NoAffiliation) {
						message = tr("%2 has joined the room as %1").arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
					}
				}
				if (!s.mucItem().jid().isEmpty())
					message = message.arg(QString("%1 (%2)").arg(nick).arg(s.mucItem().jid().full()));
				else
					message = message.arg(nick);
				appendSysMsg(message, false, QDateTime::currentDateTime());
			}
		}
		else {
			// Status change
			if ( !d->connecting && options_->getOption("options.muc.show-role-affiliation").toBool() ) {
				QString message;
				if (contact->s.mucItem().role() != s.mucItem().role() && s.mucItem().role() != MUCItem::NoRole) {
					if (contact->s.mucItem().affiliation() != s.mucItem().affiliation()) {
						message = tr("%1 is now %2 and %3").arg(nick).arg(MUCManager::roleToString(s.mucItem().role(),true)).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
					}
					else {
						message = tr("%1 is now %2").arg(nick).arg(MUCManager::roleToString(s.mucItem().role(),true));
					}
				}
				else if (contact->s.mucItem().affiliation() != s.mucItem().affiliation()) {
					message += tr("%1 is now %2").arg(nick).arg(MUCManager::affiliationToString(s.mucItem().affiliation(),true));
				}

				if (!message.isEmpty())
					appendSysMsg(message, false, QDateTime::currentDateTime());
			}
		}
		d->lv_users->updateEntry(nick, s);
	} 
	else {
		// Unavailable
		if (s.hasMUCDestroy()) {
			// Room was destroyed
			QString message = tr("This room has been destroyed.");
			if (!s.mucDestroy().reason().isEmpty()) {
				message += "\n";
				message += tr("Reason: %1").arg(s.mucDestroy().reason());
			}
			if (!s.mucDestroy().jid().isEmpty()) {
				message += "\n";
				message += tr("Do you want to join the alternate venue '%1' ?").arg(s.mucDestroy().jid().full());
				int ret = QMessageBox::information(this, tr("Room Destroyed"), message, QMessageBox::Yes, QMessageBox::No);
				if (ret == QMessageBox::Yes) {
					d->pa->actionJoin(s.mucDestroy().jid().full());
				}
			}
			else {
				QMessageBox::information(this,tr("Room Destroyed"), message);
			}
			close();
		}
		if ( !d->connecting && options_->getOption("options.muc.show-joins").toBool() ) {
			QString message;
			QString nickJid;
			GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
			if (contact && !contact->s.mucItem().jid().isEmpty())
				nickJid = QString("%1 (%2)").arg(nick).arg(contact->s.mucItem().jid().full());
			else
				nickJid = nick;

			switch (s.mucStatus()) {
				case 301:
					// Ban
					if (nick == d->self) {
						mucInfoDialog(tr("Banned"), tr("You have been banned from the room"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been banned by %1").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been banned").arg(nickJid);

					if (!s.mucItem().reason().isEmpty()) 
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				case 303:
					message = tr("%1 is now known as %2").arg(nick).arg(s.mucItem().nick());
					d->lv_users->updateEntry(s.mucItem().nick(), s);
					break;
					
				case 307:
					// Kick
					if (nick == d->self) {
						mucInfoDialog(tr("Kicked"), tr("You have been kicked from the room"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been kicked by %2").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been kicked").arg(nickJid);
					if (!s.mucItem().reason().isEmpty()) 
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;
					
				case 321:
					// Remove due to affiliation change
					if (nick == d->self) {
						mucInfoDialog(tr("Removed"), tr("You have been removed from the room due to an affiliation change"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been removed from the room by %2 due to an affilliation change").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been removed from the room due to an affilliation change").arg(nickJid);

					if (!s.mucItem().reason().isEmpty()) 
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;
					
				case 322:
					// Remove due to members only
					if (nick == d->self) {
						mucInfoDialog(tr("Removed"), tr("You have been removed from the room because the room was made members only"), s.mucItem().actor(), s.mucItem().reason());
						close();
					}

					if (!s.mucItem().actor().isEmpty())
						message = tr("%1 has been removed from the room by %2 because the room was made members-only").arg(nickJid).arg(s.mucItem().actor().full());
					else
						message = tr("%1 has been removed from the room because the room was made members-only").arg(nickJid);

					if (!s.mucItem().reason().isEmpty()) 
						message += QString(" (%1)").arg(s.mucItem().reason());
					break;

				default:
					//contact leaving
					message = tr("%1 has left the room").arg(nickJid);
			}
			appendSysMsg(message, false, QDateTime::currentDateTime());
		}
		d->lv_users->removeEntry(nick);
	}
	
	if (!s.capsNode().isEmpty()) {
		Jid caps_jid(s.mucItem().jid().isEmpty() ? Jid(d->jid).withResource(nick) : s.mucItem().jid());
		d->pa->capsManager()->updateCaps(caps_jid,s.capsNode(),s.capsVersion(),s.capsExt());
	}

}

void GCMainDlg::message(const Message &_m)
{
	Message m = _m;
	QString from = m.from().resource();
	bool alert = false;

	if(!m.subject().isEmpty()) {
		d->le_topic->setText(m.subject());
		d->le_topic->setCursorPosition(0);
		d->le_topic->setToolTip(QString("<qt><p>%1</p></qt>").arg(m.subject()));
		if (d->connecting)
			return;
		if(m.body().isEmpty()) {
			if (!from.isEmpty())
				m.setBody(QString("/me ") + tr("has set the topic to: %1").arg(m.subject()));
			else
				// The topic was set by the server
				m.setBody(tr("The topic has been set to: %1").arg(m.subject()));
		}
	}

	if(m.body().isEmpty())
		return;

	// code to determine if the speaker was addressing this client in chat
	if(m.body().contains(d->self))
		alert = true;

	if (m.body().left(d->self.length()) == d->self)
		d->lastReferrer = m.from().resource();

	if(option.gcHighlighting) {
		for(QStringList::Iterator it=option.gcHighlights.begin();it!=option.gcHighlights.end();it++) {
			if(m.body().contains((*it), Qt::CaseInsensitive)) {
				alert = true;
			}
		}
	}

	// play sound?
	if(from == d->self) {
		if(!m.spooled())
			d->pa->playSound(option.onevent[eSend]);
	}
	else {
		if(alert || (!option.noGCSound && !m.spooled() && !from.isEmpty()) )
			d->pa->playSound(option.onevent[eChat2]);
	}

	if(from.isEmpty())
		appendSysMsg(m.body(), alert, m.timeStamp());
	else
		appendMessage(m, alert);
}

void GCMainDlg::joined()
{
	if(d->state == Private::Connecting) {
		d->lv_users->clear();
		d->state = Private::Connected;
		d->pb_topic->setEnabled(true);
		d->mle->setEnabled(true);
		setConnecting();
		appendSysMsg(tr("Connected."), true);
	}
}

void GCMainDlg::setPassword(const QString& p)
{
	d->password = p;
}

const QString& GCMainDlg::nick() const
{
	return d->self;
}

void GCMainDlg::appendSysMsg(const QString &str, bool alert, const QDateTime &ts)
{
	if (d->trackBar)
	 	d->doTrackBar();

	if (!option.gcHighlighting)
		alert=false;

	QDateTime time = QDateTime::currentDateTime();
	if(!ts.isNull())
		time = ts;

	QString timestr = d->te_log->formatTimeStamp(time);
	d->te_log->appendText(QString("<font color=\"#00A000\">[%1]").arg(timestr) + QString(" *** %1</font>").arg(Qt::escape(str)));

	if(alert)
		doAlert();
}

QString GCMainDlg::getNickColor(QString nick)
{
	int sender;
	if(nick == d->self||nick.isEmpty())
		sender = -1;
	else {
		if (!nicks.contains(nick)) {
			//not found in map
			nicks.insert(nick,nicknumber);
			nicknumber++;
		}
		sender=nicks[nick];
	}

	if(!option.gcNickColoring || option.gcNickColors.empty()) {
		return "#000000";
	}
	else if(sender == -1 || option.gcNickColors.size() == 1) {
		return option.gcNickColors[option.gcNickColors.size()-1];
	}
	else {
		int n = sender % (option.gcNickColors.size()-1);
		return option.gcNickColors[n];
	}
}

void GCMainDlg::appendMessage(const Message &m, bool alert)
{
	//QString who, color;
	if (!option.gcHighlighting)
		alert=false;
	QString who, textcolor, nickcolor,alerttagso,alerttagsc;

	who = m.from().resource();
	if (d->trackBar&&m.from().resource() != d->self&&!m.spooled())
	 	d->doTrackBar();
	/*if(local) {
		color = "#FF0000";
	}
	else {
		color = "#0000FF";
	}*/
	nickcolor = getNickColor(who);
	textcolor = d->te_log->palette().active().text().name();
	if(alert) {
		textcolor = "#FF0000";
		alerttagso = "<b>";
		alerttagsc = "</b>";
	}
	if(m.spooled())
		nickcolor = "#008000"; //color = "#008000";

	QString timestr = d->te_log->formatTimeStamp(m.timeStamp());

	bool emote = false;
	if(m.body().left(4) == "/me ")
		emote = true;

	QString txt;
	if(emote)
		txt = TextUtil::plain2rich(m.body().mid(4));
	else
		txt = TextUtil::plain2rich(m.body());

	txt = TextUtil::linkify(txt);

	if(option.useEmoticons)
		txt = TextUtil::emoticonify(txt);

	if(emote) {
		//d->te_log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(Qt::escape(who)) + txt + "</font>");
		d->te_log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(Qt::escape(who)) + alerttagso + txt + alerttagsc + "</font>");
	}
	else {
		if(option.chatSays) {
			//d->te_log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1] ").arg(timestr) + QString("%1 says:").arg(Qt::escape(who)) + "</font><br>" + txt);
			d->te_log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] ").arg(timestr) + QString("%1 says:").arg(Qt::escape(who)) + "</font><br>" + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + txt + alerttagsc + "</font>");
		}
		else {
			//d->te_log->append(QString("<font color=\"%1\">").arg(color) + QString("[%1] &lt;").arg(timestr) + Qt::escape(who) + QString("&gt;</font> ") + txt);
			d->te_log->appendText(QString("<font color=\"%1\">").arg(nickcolor) + QString("[%1] &lt;").arg(timestr) + Qt::escape(who) + QString("&gt;</font> ") + QString("<font color=\"%1\">").arg(textcolor) + alerttagso + txt + alerttagsc +"</font>");
		}
	}

	//if(local)
	if(m.from().resource() == d->self)
		d->deferredScroll();

	// if we're not active, notify the user by changing the title
	if(!isActiveWindow()) {
		++d->pending;
		updateCaption();
	}

	//if someone directed their comments to us, notify the user
	if(alert)
		doAlert();

	//if the message spoke to us, alert the user before closing this window
	//except that keepopen doesn't seem to be implemented for this class yet.
	/*if(alert) {
		d->keepOpen = true;
		QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
        }*/
}

void GCMainDlg::doAlert()
{
	if(!isActiveWindow())
		doFlash(true);
}

void GCMainDlg::updateCaption()
{
	QString cap = "";

	if(d->pending > 0) {
		cap += "* ";
		if(d->pending > 1)
			cap += QString("[%1] ").arg(d->pending);
	}
	cap += d->jid.full();

	// if taskbar flash, then we need to erase first and redo
#ifdef Q_WS_WIN
	bool on = false;
	if(d->flashTimer)
		on = d->flashCount & 1;
	if(on)
		FlashWindow(winId(), true);
#endif
	setWindowTitle(cap);
#ifdef Q_WS_WIN
	if(on)
		FlashWindow(winId(), true);
#endif
}

#ifdef Q_WS_WIN
void GCMainDlg::doFlash(bool yes)
{
	if(yes) {
		if(d->flashTimer)
			return;
		d->flashTimer = new QTimer(this);
		connect(d->flashTimer, SIGNAL(timeout()), SLOT(flashAnimate()));
		d->flashCount = 0;
		flashAnimate(); // kick the first one immediately
		d->flashTimer->start(500);
	}
	else {
		if(d->flashTimer) {
			delete d->flashTimer;
			d->flashTimer = 0;
			FlashWindow(winId(), false);
		}
	}
}
#else
void GCMainDlg::doFlash(bool)
{
}
#endif

void GCMainDlg::flashAnimate()
{
#ifdef Q_WS_WIN
	FlashWindow(winId(), true);
	++d->flashCount;
	if(d->flashCount == 5)
		d->flashTimer->stop();
#endif
}

void GCMainDlg::setLooks()
{
	// update the fonts
	QFont f;
	f.fromString(option.font[fChat]);
	d->te_log->setFont(f);
	d->mle->setFont(f);

	f.fromString(option.font[fRoster]);
	d->lv_users->Q3ListView::setFont(f);

	if ( d->smallChat ) {
		d->toolbar->hide();
	}
	else {
		d->toolbar->show();
	}

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);

	// update the widget icon
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/groupChat"));
#endif
}

void GCMainDlg::optionsUpdate()
{
	/*QMimeSourceFactory *m = d->te_log->mimeSourceFactory();
	d->te_log->setMimeSourceFactory(PsiIconset::instance()->emoticons.generateFactory());
	delete m;*/

	setLooks();
	setShortcuts();

	// update status icons
	d->lv_users->updateAll();
}

void GCMainDlg::lv_action(const QString &nick, const Status &s, int x)
{
	if(x == 0) {
		d->pa->invokeGCMessage(d->jid.withResource(nick));
	}
	else if(x == 1) {
		d->pa->invokeGCChat(d->jid.withResource(nick));
	}
	else if(x == 2) {
		UserListItem u;
		u.setJid(d->jid.withResource(nick));
		u.setName(nick);

		// make a resource so the contact appears online
		UserResource ur;
		ur.setName(nick);
		ur.setStatus(s);
		u.userResourceList().append(ur);

		StatusShowDlg *w = new StatusShowDlg(u);
		w->show();
	}
	else if(x == 3) {
		d->pa->invokeGCInfo(d->jid.withResource(nick));
	}
	else if(x == 4) {
		d->pa->invokeGCFile(d->jid.withResource(nick));
	}
	else if(x == 10) {
		d->mucManager->kick(nick);
	}
	else if(x == 11) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		d->mucManager->ban(contact->s.mucItem().jid());
	}
	else if(x == 12) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Visitor)
			d->mucManager->setRole(nick, MUCItem::Visitor);
	}
	else if(x == 13) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Participant)
			d->mucManager->setRole(nick, MUCItem::Participant);
	}
	else if(x == 14) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().role() != MUCItem::Moderator)
			d->mucManager->setRole(nick, MUCItem::Moderator);
	}
	/*else if(x == 15) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::NoAffiliation)
			d->mucManager->setAffiliation(contact->s.mucItem().jid(), MUCItem::NoAffiliation);
	}
	else if(x == 16) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Member)
			d->mucManager->setAffiliation(contact->s.mucItem().jid(), MUCItem::Member);
	}
	else if(x == 17) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Admin)
			d->mucManager->setAffiliation(contact->s.mucItem().jid(), MUCItem::Admin);
	}
	else if(x == 18) {
		GCUserViewItem *contact = (GCUserViewItem*) d->lv_users->findEntry(nick);
		if (contact->s.mucItem().affiliation() != MUCItem::Owner)
			d->mucManager->setAffiliation(contact->s.mucItem().jid(), MUCItem::Owner);
	}*/
}

void GCMainDlg::contextMenuEvent(QContextMenuEvent *)
{
	d->pm_settings->exec(QCursor::pos());
}

void GCMainDlg::buildMenu()
{
	// Dialog menu
	d->pm_settings->clear();
	d->pm_settings->insertItem(tr("Toggle Compact/Full Size"), this, SLOT(toggleSmallChat()));

	d->act_clear->addTo( d->pm_settings );
	d->act_configure->addTo( d->pm_settings );
	d->pm_settings->insertSeparator();

	d->act_icon->addTo( d->pm_settings );
}

void GCMainDlg::toggleSmallChat()
{
	d->smallChat = !d->smallChat;
	setLooks();
}

//----------------------------------------------------------------------------
// GCFindDlg
//----------------------------------------------------------------------------
GCFindDlg::GCFindDlg(const QString &str, QWidget *parent, const char *name)
:QDialog(parent, name, false, Qt::WDestructiveClose)
{
	setWindowTitle(tr("Find"));
	QVBoxLayout *vb = new QVBoxLayout(this, 4);
	QHBoxLayout *hb = new QHBoxLayout(vb);
	QLabel *l = new QLabel(tr("Find:"), this);
	hb->addWidget(l);
	le_input = new QLineEdit(this);
	hb->addWidget(le_input);
	vb->addStretch(1);

	QFrame *Line1 = new QFrame(this);
	Line1->setFrameShape( QFrame::HLine );
	Line1->setFrameShadow( QFrame::Sunken );
	Line1->setFrameShape( QFrame::HLine );
	vb->addWidget(Line1);

	hb = new QHBoxLayout(vb);
	hb->addStretch(1);
	QPushButton *pb_close = new QPushButton(tr("&Close"), this);
	connect(pb_close, SIGNAL(clicked()), SLOT(close()));
	hb->addWidget(pb_close);
	QPushButton *pb_find = new QPushButton(tr("&Find"), this);
	pb_find->setDefault(true);
	connect(pb_find, SIGNAL(clicked()), SLOT(doFind()));
	hb->addWidget(pb_find);
	pb_find->setAutoDefault(true);

	resize(200, minimumSize().height());

	le_input->setText(str);
	le_input->setFocus();
}

GCFindDlg::~GCFindDlg()
{
}

void GCFindDlg::found()
{
	// nothing here to do...
}

void GCFindDlg::error(const QString &str)
{
	QMessageBox::warning(this, tr("Find"), tr("Search string '%1' not found.").arg(str));
	le_input->setFocus();
}

void GCFindDlg::doFind()
{
	emit find(le_input->text());
}

#include "groupchatdlg.moc"
