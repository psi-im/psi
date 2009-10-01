/*
 * discodlg.cpp - main dialog for the Service Discovery protocol
 * Copyright (C) 2003  Michail Pishchagin
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

#include "discodlg.h"

#include <q3listview.h>
#include <q3popupmenu.h>
#include <Q3PtrList>
#include <Q3Dict>

#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QAction>
#include <QSignalMapper>
#include <QPushButton>
#include <QToolButton>
#include <QToolBar>
#include <QScrollBar>

#include <QActionGroup>
#include <QEvent>
#include <QList>
#include <QContextMenuEvent>

#include "xmpp_tasks.h"

#include "tasklist.h"
#include "psiaccount.h"
#include "psicon.h"
#include "busywidget.h"
#include "iconaction.h"
#include "psiiconset.h"
#include "psitooltip.h"
#include "stretchwidget.h"
#include "psioptions.h"
#include "accountlabel.h"

//----------------------------------------------------------------------------

PsiIcon category2icon(const QString &category, const QString &type, int status=STATUS_ONLINE)
{
	// TODO: update this to http://www.jabber.org/registrar/disco-categories.html#gateway

	// still have to add more options...
	if ( category == "service" || category == "gateway" ) {
		QString trans;

		if (type == "aim")
			trans = "aim";
		else if (type == "icq")
			trans = "icq";
		else if (type == "msn")
			trans = "msn";
		else if (type == "yahoo")
			trans = "yahoo";
		else if (type == "gadu-gadu" || type == "x-gadugadu")
			trans = "gadugadu";
		else if (type == "sms")
			trans = "sms";
		else
			trans = "transport";

		return PsiIconset::instance()->transportStatus(trans, status);

		// irc
		// jud
		// pager
		// jabber
		// serverlist
		// smtp
	}
	else if ( category == "conference" ) {
		if (type == "public" || type == "private" || type == "text" || type == "irc")
			return IconsetFactory::icon("psi/groupChat");
		else if (type == "url")
			return IconsetFactory::icon("psi/www");
		// irc
		// list
		// topic
	}
	else if ( category == "validate" ) {
		if (type == "xml")
			return IconsetFactory::icon("psi/xml");
		// grammar
		// spell
	}
	else if ( category == "user" || category == "client" ) {
		// client
		// forward
		// inbox
		// portable
		// voice
		return PsiIconset::instance()->status(STATUS_ONLINE);
	}
	// application
	   // bot
	   // calendar
	   // editor
	   // fileserver
	   // game
	   // whiteboard
	// headline
	   // logger
	   // notice
	   // rss
	   // stock
	// keyword
	   // dictionary
	   // dns
	   // software
	   // thesaurus
	   // web
	   // whois
	// render
	   // en2ru
	   // ??2??
	   // tts
	return PsiIcon();
}

//----------------------------------------------------------------------------
// DiscoData -- a shared data struct
//----------------------------------------------------------------------------

class DiscoListItem;
class DiscoConnector : public QObject
{
	Q_OBJECT
public:
	DiscoConnector(QObject *parent)
	: QObject(parent) {}

signals:
	void itemUpdated(Q3ListViewItem *);

private:
	friend class DiscoListItem;
};

struct DiscoData {
	PsiAccount *pa;
	TaskList *tasks;
	DiscoConnector *d;

	enum Protocol {
		Auto,

		Disco,
		Browse,
		Agents
	};
	Protocol protocol;
};

//----------------------------------------------------------------------------
// DiscoListItem
//----------------------------------------------------------------------------

class DiscoListItem : public QObject, public Q3ListViewItem
{
	Q_OBJECT
public:
	DiscoListItem(DiscoItem it, DiscoData *d, Q3ListView *parent);
	DiscoListItem(DiscoItem it, DiscoData *d, Q3ListViewItem *parent);
	~DiscoListItem();

	QString text(int columns) const;
	void setOpen(bool open);
	const DiscoItem &item() const;

	void itemSelected();

public slots: // the two are used internally by class, and also called by DiscoDlg::Private::refresh()
	void updateInfo();
	void updateItems(bool parentAutoItems = false);
	QString getErrorInfo() const;

private slots:
	void discoItemsFinished();
	void discoInfoFinished();

	void doBrowse(bool parentAutoItems = false);
	void doAgents(bool parentAutoItems = false);
	void browseFinished();
	void agentsFinished();

private:
	DiscoItem di;
	DiscoData *d;
	bool isRoot;
	bool alreadyItems, alreadyInfo;
	bool autoItems; // used in updateItemsFinished
	bool autoInfo;
	QString errorInfo;

	void copyItem(const DiscoItem &);
	void updateInfo(const DiscoItem &);
	void updateItemsFinished(const DiscoList &);
	void autoItemsChildren() const; // automatically call disco#items for children :-)
	QString hash() { return computeHash( item().jid().full(), item().node() ); }
	QString computeHash( QString jid, QString node );

	// helper functions
	void init(DiscoItem it, DiscoData *dd);

	bool autoItemsEnabled() const;
	bool autoInfoEnabled() const;
	DiscoDlg *dlg() const;
};

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, Q3ListView *parent)
: Q3ListViewItem (parent)
{
	isRoot = true;

	init(it, _d);
}

DiscoListItem::DiscoListItem(DiscoItem it, DiscoData *_d, Q3ListViewItem *parent)
: Q3ListViewItem (parent)
{
	isRoot = false;

	init(it, _d);
}

DiscoListItem::~DiscoListItem()
{
}

void DiscoListItem::init(DiscoItem _item, DiscoData *_d)
{
	d = _d;
	di = _item;
	copyItem(_item);

	alreadyItems = alreadyInfo = false;

	if ( !autoItemsEnabled() )
		setExpandable (true);

	autoInfo = false;
	if ( autoInfoEnabled() || isRoot ) {
		updateInfo();
		if ( !isRoot )
			autoInfo = true;
	}
}

void DiscoListItem::copyItem(const DiscoItem &it)
{
	if ( !(!di.jid().full().isEmpty() && it.jid().full().isEmpty()) )
		di.setJid ( it.jid() );
	if ( !(!di.node().isEmpty() && it.node().isEmpty()) )
		di.setNode ( it.node() );
	if ( !(!di.name().isEmpty() && it.name().isEmpty()) )
		di.setName ( it.name() );
	
	if ( di.name().isEmpty() && !di.jid().full().isEmpty() ) // use JID in the Name column
		di.setName ( di.jid().full() );

	di.setAction ( it.action() );

	if ( !(!di.features().list().isEmpty() && it.features().list().isEmpty()) )
		di.setFeatures ( it.features() );
	if ( !(!di.identities().isEmpty() && it.identities().isEmpty()) )
		di.setIdentities ( it.identities() );

	if ( di.jid().bare().left(4) == "jud." || di.jid().bare().left(6) == "users." ) {
		// nasty hack for the nasty (and outdated) JUD service :-/
		if ( !di.features().canSearch() ) {
			QStringList features = di.features().list();
			features << "jabber:iq:search";
			di.setFeatures( features );
		}

		bool found = false;
		DiscoItem::Identities::ConstIterator it = di.identities().begin();
		for ( ; it != di.identities().end(); ++it) {
			if ( (*it).category == "service" && (*it).type == "jud" ) {
				found = true;
				break;
			}
		}
		if ( !found ) {
			DiscoItem::Identity id;
			id.category = "service";
			id.type     = "jud";
			DiscoItem::Identities ids;
			ids << id;
			di.setIdentities( ids );
		}
	}

	bool pixmapOk = false;
	if ( !di.identities().isEmpty() ) {
		DiscoItem::Identity id = di.identities().first();

		if ( !id.category.isEmpty() ) {
			PsiIcon ic = category2icon(id.category, id.type);

			if ( !ic.impix().isNull() ) {
				setPixmap (0, ic.impix());
				pixmapOk = true;
			}
		}
	}

	if ( !pixmapOk )
		setPixmap(0, PsiIconset::instance()->status(di.jid(), STATUS_ONLINE).impix());

	repaint();

	if ( isSelected() ) // update actions
		emit d->d->itemUpdated( this );
}

QString DiscoListItem::text (int c) const
{
	if (c == 0)
		return di.name().simplified();
	else if (c == 1)
		return di.jid().full();
	else if (c == 2)
		return di.node().simplified();
	return "";
}

QString DiscoListItem::getErrorInfo() const
{
	return errorInfo;
}

const DiscoItem &DiscoListItem::item() const
{
	return di;
}

DiscoDlg *DiscoListItem::dlg() const
{
	return (DiscoDlg *)listView()->parent();
}

bool DiscoListItem::autoItemsEnabled() const
{
	return dlg()->ck_autoItems->isChecked();
}

bool DiscoListItem::autoInfoEnabled() const
{
	return dlg()->ck_autoInfo->isChecked();
}

void DiscoListItem::setOpen (bool o)
{
	if ( o ) {
		if ( !alreadyItems )
			updateItems();
		else
			autoItemsChildren();
	}

	Q3ListViewItem::setOpen(o);
}

void DiscoListItem::itemSelected()
{
	if ( !alreadyInfo )
		updateInfo();
}

void DiscoListItem::updateItems(bool parentAutoItems)
{
	if ( parentAutoItems ) {
		// save traffic
		if ( alreadyItems )
			return;

		// FIXME: currently, JUD doesn't seem to answer to browsing requests
		if ( item().identities().size() ) {
			DiscoItem::Identity id = item().identities().first();
			if ( id.category == "service" && id.type == "jud" )
				return;
		}
		QString j = item().jid().domain(); // just another method to discover if we're gonna to browse JUD
		if ( item().jid().node().isEmpty() && (j.left(4) == "jud." || j.left(6) == "users.") )
			return;
	}

	autoItems = !parentAutoItems;

	if ( !autoItemsEnabled() )
		autoItems = false;

	if ( d->protocol == DiscoData::Auto || d->protocol == DiscoData::Disco ) {
		JT_DiscoItems *jt = new JT_DiscoItems(d->pa->client()->rootTask());
		connect(jt, SIGNAL(finished()), SLOT(discoItemsFinished()));
		jt->get(di.jid(), di.node());
		jt->go(true);
		d->tasks->append(jt);
	}
	else if ( d->protocol == DiscoData::Browse )
		doBrowse(parentAutoItems);
	else if ( d->protocol == DiscoData::Agents )
		doAgents(parentAutoItems);
}

void DiscoListItem::discoItemsFinished()
{
	JT_DiscoItems *jt = (JT_DiscoItems *)sender();

	if ( jt->success() ) {
		updateItemsFinished(jt->items());
	}
	else if ( d->protocol == DiscoData::Auto ) {
		doBrowse();
		return;
	}
	else if ( !autoItems ) {
		QString error = jt->statusString();
		QMessageBox::critical(dlg(), tr("Error"), tr("There was an error getting items for <b>%1</b>.<br>Reason: %2").arg(di.jid().full()).arg(QString(error).replace('\n', "<br>")));
	}

	alreadyItems = true;
}

void DiscoListItem::doBrowse(bool parentAutoItems)
{
	if ( parentAutoItems ) {
		// save traffic
		if ( alreadyItems )
			return;

		if ( item().identities().size() ) {
			DiscoItem::Identity id = item().identities().first();
			if ( id.category == "service" && id.type == "jud" )
				return;
		}
	}

	autoItems = !parentAutoItems;
	if ( !autoItemsEnabled() )
		autoItems = false;

	JT_Browse *jt = new JT_Browse(d->pa->client()->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(browseFinished()));
	jt->get(di.jid());
	jt->go(true);
	d->tasks->append(jt);
}

void DiscoListItem::browseFinished()
{
	JT_Browse *jt = (JT_Browse *)sender();

	if ( jt->success() ) {
		// update info
		DiscoItem root;
		root.fromAgentItem( jt->root() );
		updateInfo(root);
		alreadyInfo = true;
		autoInfo = false;

		// update items
		AgentList from = jt->agents();
		DiscoList to;
		AgentList::Iterator it = from.begin();
		for ( ; it != from.end(); ++it) {
			DiscoItem item;
			item.fromAgentItem( *it );

			to.append( item );
		}

		updateItemsFinished(to);
	}
	else if ( d->protocol == DiscoData::Auto ) {
		doAgents();
		return;
	}
	else if ( !autoItems ) {
		QString error = jt->statusString();
		QMessageBox::critical(dlg(), tr("Error"), tr("There was an error browsing items for <b>%1</b>.<br>Reason: %2").arg(di.jid().full()).arg(QString(error).replace('\n', "<br>")));
	}

	alreadyItems = true;
}

void DiscoListItem::doAgents(bool parentAutoItems)
{
	if ( parentAutoItems ) {
		// save traffic
		if ( alreadyItems )
			return;

		if ( item().identities().size() ) {
			DiscoItem::Identity id = item().identities().first();
			if ( id.category == "service" && id.type == "jud" )
				return;
		}
	}

	autoItems = !parentAutoItems;
	if ( !autoItemsEnabled() )
		autoItems = false;

	JT_GetServices *jt = new JT_GetServices(d->pa->client()->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(agentsFinished()));
	jt->get(di.jid());
	jt->go(true);
	d->tasks->append(jt);
}

void DiscoListItem::agentsFinished()
{
	JT_GetServices *jt = (JT_GetServices *)sender();

	if ( jt->success() ) {
		// update info
		DiscoItem root;
		DiscoItem::Identity id;
		id.name     = tr("Jabber Service");
		id.category = "service";
		id.type     = "jabber";
		DiscoItem::Identities ids;
		ids.append(id);
		root.setIdentities(ids);
		updateInfo(root);
		alreadyInfo = true;
		autoInfo = false;

		// update items
		AgentList from = jt->agents();
		DiscoList to;
		AgentList::Iterator it = from.begin();
		for ( ; it != from.end(); ++it) {
			DiscoItem item;
			item.fromAgentItem( *it );

			to.append( item );
		}

		updateItemsFinished(to);
	}
	else if ( !autoItems ) {
		QString error = jt->statusString();
		QMessageBox::critical(dlg(), tr("Error"), tr("There was an error getting agents for <b>%1</b>.<br>Reason: %2").arg(di.jid().full()).arg(QString(error).replace('\n', "<br>")));
	}

	alreadyItems = true;
}

QString DiscoListItem::computeHash( QString jid, QString node )
{
	QString ret = jid.replace( '@', "\\@" );
	ret += "@";
	ret += node.replace( '@', "\\@" );
	return ret;
}

void DiscoListItem::updateItemsFinished(const DiscoList &list)
{
	Q3Dict<DiscoListItem> children;
	DiscoListItem *child = (DiscoListItem *)firstChild();
	while ( child ) {
		children.insert( child->hash(), child );

		child = (DiscoListItem *)child->nextSibling();
	}

	// add/update items
	for(DiscoList::ConstIterator it = list.begin(); it != list.end(); ++it) {
		const DiscoItem a = *it;

		QString key = computeHash(a.jid().full(), a.node());
		DiscoListItem *child = children[ key ];

		if ( child ) {
			child->copyItem ( a );
			children.remove( key );
		}
		else {
			new DiscoListItem (a, d, this);
		}
	}

	// remove all items that are not on new DiscoList
	children.setAutoDelete( true );
	children.clear();

	if ( autoItems && isOpen() )
		autoItemsChildren();

	// don't forget to remove '+' (or '-') sign in case, that the child list is empty
	setExpandable ( !list.isEmpty() );

	repaint();

	// root item is initially hidden
	if ( isRoot && !isVisible() )
		setVisible (true);
}

void DiscoListItem::autoItemsChildren() const
{
	if ( !autoItemsEnabled() )
		return;

	DiscoListItem *child = (DiscoListItem *)firstChild();
	while ( child ) {
		child->updateItems(true);

		child = (DiscoListItem *)child->nextSibling();
	}
}

void DiscoListItem::updateInfo()
{
	if ( d->protocol != DiscoData::Auto && d->protocol != DiscoData::Disco )
		return;

	JT_DiscoInfo *jt = new JT_DiscoInfo(d->pa->client()->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(discoInfoFinished()));
	jt->get(di.jid(), di.node());
	jt->go(true);
	d->tasks->append(jt);
}

void DiscoListItem::discoInfoFinished()
{
	JT_DiscoInfo *jt = (JT_DiscoInfo *)sender();

	if ( jt->success() ) {
		updateInfo( jt->item() );
	}
	else {
		QString error_str = jt->statusString();
		int error_code = jt->statusCode();
		setExpandable(false);

		// we change the icon for the items with disco#info returning type=="cancel" || type=="wait" error codes
		// based on http://www.jabber.org/jeps/jep-0086.html

		// FIXME: use another method for checking XMPP error-types when Iris will provide one
		if ( error_code==400 || error_code==404 || error_code==405 || error_code==409 ||
		     error_code==500 || error_code==501 || error_code==503 || error_code==504 ) {
			bool pixmapOk = false;
			if ( !di.identities().isEmpty() ) {
				DiscoItem::Identity id = di.identities().first();
				if ( !id.category.isEmpty() ) {
					PsiIcon ic = category2icon(id.category, id.type, STATUS_ERROR);

					if ( !ic.impix().isNull() ) {
						setPixmap (0, ic.impix());
						pixmapOk = true;
					}
				}
			}
			if ( !pixmapOk )
				setPixmap(0, PsiIconset::instance()->status(di.jid(), STATUS_ERROR).impix());
		} else {
			repaint(); //for setExpandable() called earlier
		}

		errorInfo=QString("%1").arg(QString(error_str).replace('\n', "<br>"));

		if ( !autoInfo && d->protocol != DiscoData::Auto ) {	
			QMessageBox::critical(dlg(), tr("Error"), tr("There was an error getting item's info for <b>%1</b>.<br>Reason: %2").arg(di.jid().full()).arg(QString(error_str).replace('\n', "<br>")));
		}
	}

	alreadyInfo = true;
	autoInfo = false;
}

void DiscoListItem::updateInfo(const DiscoItem &item)
{
	copyItem( item );

	if ( isRoot && !isVisible() )
		setVisible (true);
}

//----------------------------------------------------------------------------
// DiscoList
//----------------------------------------------------------------------------

class DiscoListView : public Q3ListView
{
	Q_OBJECT
public:
	DiscoListView(QWidget *parent);

protected:
	bool maybeTip(const QPoint &);

	// reimplemented
	bool eventFilter(QObject* o, QEvent* e);
	void resizeEvent(QResizeEvent*);
};

DiscoListView::DiscoListView(QWidget *parent)
: Q3ListView(parent)
{
	addColumn( tr( "Name" ) );
	addColumn( tr( "JID" ) );
	addColumn( tr( "Node" ) );
	for (int i = 0; i < 3; i++)
		setColumnWidthMode(i, Q3ListView::Manual);
	header()->setStretchEnabled(true, 0);
}

void DiscoListView::resizeEvent(QResizeEvent* e)
{
	Q3ListView::resizeEvent(e);

	setColumnWidth(2, header()->fontMetrics().width(columnText(2)) * 2);
	float remainingWidth = visibleWidth() - columnWidth(2);
	setColumnWidth(1, int(remainingWidth * 0.3));

	header()->adjustHeaderSize();
}

/**
 * \param pos should be in global coordinate system.
 */
bool DiscoListView::maybeTip(const QPoint &pos)
{
	DiscoListItem* i = (DiscoListItem*)itemAt(viewport()->mapFromGlobal(pos));
	if(!i)
		return false;

	// NAME <JID> (Node "NODE")
	//
	// Identities:
	// (icon) NAME (Category "CATEGORY"; Type "TYPE")
	// (icon) NAME (Category "CATEGORY"; Type "TYPE")
	//
	// Features:
	// NAME (http://jabber.org/feature)
	// NAME (http://jabber.org/feature)

	// top row
	QString text = "<qt><nobr>";
	DiscoItem item = i->item();

	if ( item.name()!=item.jid().full() )
		text += item.name() + " ";

	text += "&lt;" + item.jid().full() + "&gt;";

	if ( !item.node().isEmpty() )
		text += " (" + tr("Node") + " \"" + item.node() + "\")";

	text += "</nobr>";

	if ( !item.identities().isEmpty() || !item.features().list().isEmpty() )
		text += "<br>\n";

	// identities
	if ( !item.identities().isEmpty() ) {
		text += "<br>\n<b>" + tr("Identities:") + "</b>\n";

		DiscoItem::Identities::ConstIterator it = item.identities().begin();
		for ( ; it != item.identities().end(); ++it) {
			text += "<br>";
			PsiIcon icon( category2icon((*it).category, (*it).type) );
			if ( !icon.name().isEmpty() )
				text += "<icon name=\"" + icon.name() + "\"> ";
			text += (*it).name;
			text += " (" + tr("Category") + " \"" + (*it).category + "\"; " + tr("Type") + " \"" + (*it).type + "\")\n";
		}

		if ( !item.features().list().isEmpty() )
			text += "<br>\n";
	}

	// features
	if ( !item.features().list().isEmpty() ) {
		text += "<br>\n<b>" + tr("Features:") + "</b>\n";

		QStringList features = item.features().list();
		QStringList::ConstIterator it = features.begin();
		for ( ; it != features.end(); ++it) {
			Features f( *it );
			text += "\n<br>";
			if ( f.id() > Features::FID_None )
				text += f.name() + " (";
			text += *it;
			if ( f.id() > Features::FID_None )
				text += ")";
		}
	}

	QString errorInfo=i->getErrorInfo();
	if ( !errorInfo.isEmpty() ) {
		text += "<br>\n<br>\n<b>" + tr("Error:") + "</b>\n";
		text += errorInfo;
	}

	text += "</qt>";
	QRect r( itemRect(i) );
	PsiToolTip::showText(pos, text, this);
	return true;
}

bool DiscoListView::eventFilter(QObject* o, QEvent* e)
{
	if (e->type() == QEvent::ToolTip && o->isWidgetType()) {
		QWidget*    w  = static_cast<QWidget*>(o);
		QHelpEvent* he = static_cast<QHelpEvent*>(e);
		maybeTip(w->mapToGlobal(he->pos()));
		return true;
	}
	return Q3ListView::eventFilter(o, e);
}

//----------------------------------------------------------------------------
// DiscoDlg::Private
//----------------------------------------------------------------------------

class DiscoDlg::Private : public QObject
{
	Q_OBJECT

private:
	class ProtocolAction : public QAction
	{
	public:
		ProtocolAction(QString text, QString toolTip, QObject *parent, QSignalMapper *sm, int parm)
			: QAction(text, parent)
		{
			setText(text);
			setIconText(text);
	
			setCheckable(true);
			setToolTip(toolTip);
			connect(this, SIGNAL(activated()), sm, SLOT(map()));
			sm->setMapping(this, parm);
		}
	};

	// helper class to store browser history
	class History {
	private:
		Q3ListViewItem *item;
	public:
		History(Q3ListViewItem *it) {
			item = it;
		}

		~History() {
			if ( item )
				delete item;
		}

		Q3ListViewItem *takeItem() {
			Q3ListViewItem *i = item;
			item = 0;
			return i;
		}
	};

public: // data
	DiscoDlg *dlg;
	Jid jid;
	QString node;

	DiscoData data;

	QToolBar *toolBar;
	IconAction *actBrowse, *actBack, *actForward, *actRefresh, *actStop;

	// custom actions, that will be added to toolbar and context menu
	IconAction *actRegister, *actSearch, *actJoin, *actAHCommand, *actVCard, *actAdd;

	typedef Q3PtrList<History> HistoryList;
	HistoryList backHistory, forwardHistory;

	BusyWidget *busy;

public: // functions
	Private(DiscoDlg *parent, PsiAccount *pa);
	~Private();

public slots:
	void doDisco(QString host = QString::null, QString node = QString::null);

	void actionStop();
	void actionRefresh();
	void actionBrowse();

	void actionBack();
	void actionForward();
	void updateBackForward();
	void backForwardHelper(Q3ListViewItem *);

	void updateComboBoxes(Jid j, QString node);

	void itemUpdateStarted();
	void itemUpdateFinished();

	void disableButtons();
	void enableButtons(const DiscoItem &);

	void itemSelected (Q3ListViewItem *);
	void itemDoubleclicked (Q3ListViewItem *);
	bool eventFilter (QObject *, QEvent *);

	void setProtocol(int);

	// features...
	void actionActivated(int);

	void objectDestroyed(QObject *);

private:
	friend class DiscoListItem;
};

DiscoDlg::Private::Private(DiscoDlg *parent, PsiAccount *pa)
{
	dlg = parent;
	data.pa = pa;
	data.tasks = new TaskList;
	connect(data.tasks, SIGNAL(started()),  SLOT(itemUpdateStarted()));
	connect(data.tasks, SIGNAL(finished()), SLOT(itemUpdateFinished()));
	data.d = new DiscoConnector(this);
	connect(data.d, SIGNAL(itemUpdated(Q3ListViewItem *)), SLOT(itemSelected (Q3ListViewItem *)));
	data.protocol = DiscoData::Auto;

	backHistory.setAutoDelete(true);
	forwardHistory.setAutoDelete(true);

	// mess with widgets
	busy = parent->busy;
	connect(busy, SIGNAL(destroyed(QObject *)), SLOT(objectDestroyed(QObject *)));

	Q3ListView *lv_discoOld = dlg->lv_disco;
	dlg->lv_disco = new DiscoListView(dlg);
	replaceWidget(lv_discoOld, dlg->lv_disco);

	dlg->lv_disco->installEventFilter (this);
	connect(dlg->lv_disco, SIGNAL(selectionChanged (Q3ListViewItem *)), SLOT(itemSelected (Q3ListViewItem *)));;
	connect(dlg->lv_disco, SIGNAL(doubleClicked (Q3ListViewItem *)),    SLOT(itemDoubleclicked (Q3ListViewItem *)));;

	// protocol actions
	QSignalMapper *pm = new QSignalMapper(this);
	connect(pm, SIGNAL(mapped(int)), SLOT(setProtocol(int)));
	QActionGroup *protocolActions = new QActionGroup (this);
	protocolActions->setExclusive(true);

	ProtocolAction *autoProtocol = NULL, *discoProtocol = NULL, *browseProtocol = NULL, *agentsProtocol = NULL;
	if (PsiOptions::instance()->getOption("options.ui.show-deprecated.service-discovery.protocol-selector").toBool()) {
		autoProtocol = new ProtocolAction (tr("Auto"), tr("Automatically determine protocol"), protocolActions, pm, DiscoData::Auto);
		discoProtocol = new ProtocolAction ("D", tr("Service Discovery"), protocolActions, pm, DiscoData::Disco);
		browseProtocol = new ProtocolAction ("B", tr("Browse Services"), protocolActions, pm, DiscoData::Browse);
		agentsProtocol = new ProtocolAction ("A", tr("Browse Agents"), protocolActions, pm, DiscoData::Agents);
		autoProtocol->setChecked(true);
	}

	// create actions
	actBrowse = new IconAction (tr("Browse"), "psi/jabber", tr("&Browse"), 0, dlg);
	connect (actBrowse, SIGNAL(activated()), SLOT(actionBrowse()));
	actRefresh = new IconAction (tr("Refresh Item"), "psi/reload", tr("&Refresh Item"), 0, dlg);
	connect (actRefresh, SIGNAL(activated()), SLOT(actionRefresh()));
	actStop = new IconAction (tr("Stop"), "psi/stop", tr("Sto&p"), 0, dlg);
	connect (actStop, SIGNAL(activated()), SLOT(actionStop()));
	actBack = new IconAction (tr("Back"), "psi/arrowLeft", tr("&Back"), 0, dlg);
	connect (actBack, SIGNAL(activated()), SLOT(actionBack()));
	actForward = new IconAction (tr("Forward"), "psi/arrowRight", tr("&Forward"), 0, dlg);
	connect (actForward, SIGNAL(activated()), SLOT(actionForward()));

	// custom actions
	QSignalMapper *sm = new QSignalMapper(this);
	connect(sm, SIGNAL(mapped(int)), SLOT(actionActivated(int)));
	actRegister = new IconAction (tr("Register"), "psi/register", tr("&Register"), 0, dlg);
	connect (actRegister, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actRegister, Features::FID_Register);
	actSearch = new IconAction (tr("Search"), "psi/search", tr("&Search"), 0, dlg);
	connect (actSearch, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actSearch, Features::FID_Search);
	actJoin = new IconAction (tr("Join"), "psi/groupChat", tr("&Join"), 0, dlg);
	connect (actJoin, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actJoin, Features::FID_Groupchat);
	actAHCommand = new IconAction (tr("Execute command"), "psi/command", tr("&Execute command"), 0, dlg);
	connect (actAHCommand, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actAHCommand, Features::FID_AHCommand);
	actVCard = new IconAction (tr("vCard"), "psi/vCard", tr("&vCard"), 0, dlg);
	connect (actVCard, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actVCard, Features::FID_VCard);
	actAdd = new IconAction (tr("Add to roster"), "psi/addContact", tr("&Add to roster"), 0, dlg);
	connect (actAdd, SIGNAL(activated()), sm, SLOT(map()));
	sm->setMapping(actAdd, Features::FID_Add);

	// create toolbar
	toolBar = new QToolBar(tr("Service Discovery toolbar"), dlg);
	toolBar->setIconSize(QSize(16,16));
	toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	replaceWidget(dlg->toolBarPlaceholder, toolBar);

	toolBar->addAction(actBack);
	toolBar->addAction(actBrowse);
	toolBar->addAction(actForward);

	toolBar->addSeparator();
	toolBar->addAction(actRefresh);
	toolBar->addAction(actStop);

	// custom actions
	toolBar->addSeparator();
	toolBar->addAction(actRegister);
	toolBar->addAction(actSearch);
	toolBar->addAction(actJoin);

	toolBar->addSeparator();
	toolBar->addAction(actAdd);
	toolBar->addAction(actVCard);
	toolBar->addAction(actAHCommand);

	// select protocol
	if (PsiOptions::instance()->getOption("options.ui.show-deprecated.service-discovery.protocol-selector").toBool()) {
		toolBar->addSeparator();
		toolBar->addAction(autoProtocol);
		toolBar->addAction(discoProtocol);
		toolBar->addAction(browseProtocol);
		toolBar->addAction(agentsProtocol);
	}

	toolBar->addWidget(new StretchWidget(toolBar));
	AccountLabel* lb_ident = new AccountLabel(toolBar);
	lb_ident->setAccount(pa);
	lb_ident->setShowJid(false);
	toolBar->addWidget(lb_ident);

	// misc stuff
	disableButtons();
	actStop->setEnabled(false);	// stop action is not handled by disableButtons()
	updateBackForward();		// same applies to back & forward
}

DiscoDlg::Private::~Private()
{
	delete data.tasks;
}

void DiscoDlg::Private::doDisco(QString _host, QString _node)
{
	PsiAccount *pa = data.pa;
	if ( !pa->checkConnected(dlg) )
		return;

	// Strip whitespace
	Jid j;
	QString host = _host;
	if ( host.isEmpty() )
		host = dlg->cb_address->currentText();
	j = host.stripWhiteSpace();
	if ( !j.isValid() )
		return;

	QString n = _node.stripWhiteSpace();
	if ( n.isEmpty() )
		n = dlg->cb_node->currentText().stripWhiteSpace();

	// check, whether we need to update history
	if ( (jid.full() != j.full()) || (node != n) ) {
		Q3ListViewItem *item = dlg->lv_disco->firstChild(); // get the root item

		if ( item ) {
			dlg->lv_disco->takeItem( item );

			backHistory.append( new History(item) );
			forwardHistory.clear();
		}
	}

	jid  = j;
	node = n;

	updateComboBoxes(jid, node);

	data.tasks->clear(); // also will call all all necessary functions
	disableButtons();
	updateBackForward();

	dlg->lv_disco->clear();

	// create new root item
	DiscoItem di;
	di.setJid( jid );
	di.setNode( node );

	DiscoListItem *root = new DiscoListItem (di, &data, dlg->lv_disco);
	root->setVisible (false); // don't confuse users with empty root

	root->setOpen(true); // begin browsing
}

void DiscoDlg::Private::updateComboBoxes(Jid j, QString n)
{
	data.pa->psi()->recentBrowseAdd( j.full() );
	dlg->cb_address->clear();
	dlg->cb_address->insertStringList(data.pa->psi()->recentBrowseList());

	data.pa->psi()->recentNodeAdd( n );
	dlg->cb_node->clear();
	dlg->cb_node->insertStringList(data.pa->psi()->recentNodeList());
}

void DiscoDlg::Private::actionStop()
{
	data.tasks->clear();
}

void DiscoDlg::Private::actionRefresh()
{
	DiscoListItem *it = (DiscoListItem *)dlg->lv_disco->selectedItem();
	if ( !it )
		return;

	it->updateItems();
	it->updateInfo();
}

void DiscoDlg::Private::actionBrowse()
{
	DiscoListItem *it = (DiscoListItem *)dlg->lv_disco->selectedItem();
	if ( !it )
		return;

	doDisco(it->item().jid().full(), it->item().node());
}

void DiscoDlg::Private::actionBack()
{
	// add current selection to forward history
	Q3ListViewItem *item = dlg->lv_disco->firstChild();
	if ( item ) {
		dlg->lv_disco->takeItem( item );

		forwardHistory.append( new History(item) );
	}

	// now, take info from back history...
	Q3ListViewItem *i = backHistory.last()->takeItem();
	backHistory.removeLast();

	// and restore view
	backForwardHelper(i);
}

void DiscoDlg::Private::actionForward()
{
	// add current selection to back history
	Q3ListViewItem *item = dlg->lv_disco->firstChild();
	if ( item ) {
		dlg->lv_disco->takeItem( item );

		backHistory.append( new History(item) );
	}

	// now, take info from forward history...
	Q3ListViewItem *i = forwardHistory.last()->takeItem();
	forwardHistory.removeLast();

	// and restore view
	backForwardHelper(i);
}

void DiscoDlg::Private::backForwardHelper(Q3ListViewItem *root)
{
	DiscoListItem *i = (DiscoListItem *)root;

	jid  = i->item().jid();
	node = i->item().node();

	updateComboBoxes(jid, node);

	data.tasks->clear(); // also will call all all necessary functions
	disableButtons();
	updateBackForward();

	dlg->lv_disco->insertItem( root );

	// fixes multiple selection bug
	Q3ListViewItemIterator it( dlg->lv_disco );
	while ( it.current() ) {
		Q3ListViewItem *item = it.current();
		++it;

		if ( item->isSelected() )
			for (int i = 0; i <= 1; i++) // it's boring to write same line twice :-)
				dlg->lv_disco->setSelected(item, (bool)i);
	}
}

void DiscoDlg::Private::updateBackForward()
{
	actBack->setEnabled ( !backHistory.isEmpty() );
	actForward->setEnabled ( !forwardHistory.isEmpty() );
}

void DiscoDlg::Private::itemUpdateStarted()
{
	actStop->setEnabled(true);
	if ( busy )
		busy->start();
}

void DiscoDlg::Private::itemUpdateFinished()
{
	actStop->setEnabled(false);
	if ( busy )
		busy->stop();
}

void DiscoDlg::Private::disableButtons()
{
	DiscoItem di;
	enableButtons ( di );
}

void DiscoDlg::Private::enableButtons(const DiscoItem &it)
{
	bool itemSelected = !it.jid().full().isEmpty();
	actRefresh->setEnabled( itemSelected );
	actBrowse->setEnabled( itemSelected );

	// custom actions
	Features f = it.features();
	actRegister->setEnabled( f.canRegister() );
	actSearch->setEnabled( f.canSearch() );
	actJoin->setEnabled( f.canGroupchat() );
	actAdd->setEnabled( itemSelected );
	actVCard->setEnabled( f.haveVCard() );
	actAHCommand->setEnabled( f.canCommand() );
}

void DiscoDlg::Private::itemSelected (Q3ListViewItem *item)
{
	DiscoListItem *it = (DiscoListItem *)item;
	if ( !it ) {
		disableButtons();
		return;
	}

	it->itemSelected();

	const DiscoItem di = it->item();
	enableButtons ( di );
}

void DiscoDlg::Private::itemDoubleclicked (Q3ListViewItem *item)
{
	DiscoListItem *it = (DiscoListItem *)item;
	if ( !it )
		return;

	const DiscoItem d = it->item();
	const Features &f = d.features();

	// set the prior state of item
	// FIXME: causes minor flickering
	long id = 0;

	// trigger default action
	if (f.canCommand() ) {
		id = Features::FID_AHCommand;
	}
	if (!d.identities().isEmpty()) {
		// FIXME: check the category and type for JUD!
		DiscoItem::Identity ident = d.identities().first();
		bool searchFirst = ident.category == "service" && ident.type == "jud";

		if ( searchFirst && f.canSearch() ) {
			id = Features::FID_Search;
		}
		else if ( ident.category == "conference" &&  f.canGroupchat() ) {
			id = Features::FID_Groupchat;
		}
		else if ( f.canRegister() ) {
				id = Features::FID_Register;
		}
	}

	if ( id > 0 ) {
		if ( !it->isOpen() ) {
			if ( it->isExpandable() || it->childCount() )
				it->setOpen( true );
		}
		else {
			it->setOpen( false );
		}
		emit dlg->featureActivated( Features::feature(id), d.jid(), d.node() );
	}
}

bool DiscoDlg::Private::eventFilter (QObject *object, QEvent *event)
{
	if ( object == dlg->lv_disco ) {
		if ( event->type() == QEvent::ContextMenu ) {
			QContextMenuEvent *e = (QContextMenuEvent *)event;

			DiscoListItem *it = (DiscoListItem *)dlg->lv_disco->selectedItem();
			if ( !it )
				return true;

			// prepare features list
			QList<long> idFeatures;
			QStringList features = it->item().features().list();
			{	// convert all features to their IDs
				QStringList::Iterator it = features.begin();
				for ( ; it != features.end(); ++it) {
					Features f( *it );
					if ( f.id() > Features::FID_None )
						idFeatures.append( Features::id(*it) );
				}
				//qHeapSort(idFeatures);
			}

			QList<long> ids;
			{	// ensure, that there's in no duplicated IDs inside. FIXME: optimize this, anyone?
				long id = 0, count = 0;
				QList<long>::Iterator it;
				while ( count < (long)idFeatures.count() ) {
					bool found = false;

					for (it = idFeatures.begin(); it != idFeatures.end(); ++it) {
						if ( id == *it ) {
							if ( !found ) {
								found = true;
								ids.append( id );
							}
							count++;
						}
					}
					id++;
				}
			}

			// prepare popup menu
			Q3PopupMenu p;

			actBrowse->addTo (&p);
			actRefresh->addTo (&p);
			actStop->addTo (&p);

			// custom actions
			p.insertSeparator();
			actRegister->addTo(&p);
			actSearch->addTo(&p);
			actJoin->addTo(&p);

			p.insertSeparator();
			actAdd->addTo(&p);
			actVCard->addTo(&p);
			actAHCommand->addTo(&p);

			// popup with all available features
			Q3PopupMenu *fm = new Q3PopupMenu(&p);
			{
				QList<long>::Iterator it = ids.begin();
				for ( ; it != ids.end(); ++it)
					fm->insertItem(Features::name(*it), *it + 10000); // TODO: add pixmap
			}

			//p.insertSeparator();
			//int menuId = p.insertItem(tr("Activate &Feature"), fm);
			//p.setItemEnabled(menuId, !ids.isEmpty());

			// display popup
			e->accept();
			int r = p.exec ( e->globalPos() );

			if ( r > 10000 )
				actionActivated(r-10000);

			return true;
		}
	}

	return false;
}

void DiscoDlg::Private::actionActivated(int id)
{
	DiscoListItem *it = (DiscoListItem *)dlg->lv_disco->selectedItem();
	if ( !it )
		return;

	emit dlg->featureActivated(Features::feature(id), it->item().jid(), it->item().node());
}

void DiscoDlg::Private::objectDestroyed(QObject *obj)
{
	if ( obj == busy )
		busy = 0;
}

void DiscoDlg::Private::setProtocol(int p)
{
	data.protocol = (DiscoData::Protocol)p;
}

//----------------------------------------------------------------------------
// DiscoDlg
//----------------------------------------------------------------------------

DiscoDlg::DiscoDlg(PsiAccount *pa, const Jid &jid, const QString &node)
	: QDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setupUi(this);
	// restore options
	ck_autoItems->setChecked(PsiOptions::instance()->getOption("options.ui.service-discovery.automatically-get-items").toBool());
	ck_autoInfo->setChecked(PsiOptions::instance()->getOption("options.ui.service-discovery.automatically-get-info").toBool());

	// initialize
	d = new Private(this, pa);
	d->jid  = jid;
	d->node = node;
	d->data.pa->dialogRegister(this);

	setWindowTitle(CAP(caption()));
	setWindowIcon(PsiIconset::instance()->transportStatus("transport", STATUS_ONLINE).icon());
	X11WM_CLASS("disco");

	connect (pb_browse, SIGNAL(clicked()), d, SLOT(doDisco()));
	pb_browse->setDefault(false);
	pb_browse->setAutoDefault(false);

	connect(pb_close, SIGNAL(clicked()), this, SLOT(close()));
	pb_close->setDefault(false);
	pb_close->setAutoDefault(false);

	cb_address->insertStringList(pa->psi()->recentBrowseList()); // FIXME
	cb_address->setFocus();
	connect(cb_address, SIGNAL(activated(const QString &)), d, SLOT(doDisco()));
	cb_address->setCurrentText(d->jid.full());

	cb_node->insertStringList(pa->psi()->recentNodeList());
	connect(cb_node, SIGNAL(activated(const QString &)), d, SLOT(doDisco()));
	cb_node->setCurrentText(node);

	if ( pa->loggedIn() )
		doDisco();
}

DiscoDlg::~DiscoDlg()
{
	d->data.pa->dialogUnregister(this);
	delete d;

	// save options
	PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-items", (bool) ck_autoItems->isChecked());
	PsiOptions::instance()->setOption("options.ui.service-discovery.automatically-get-info", (bool) ck_autoInfo->isChecked());
}

void DiscoDlg::doDisco(QString host, QString node)
{
	d->doDisco(host, node);
}

#include "discodlg.moc"
