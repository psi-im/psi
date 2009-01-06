/*
 * contactview.h - contact list widget
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

#ifndef CONTACTVIEW_H
#define CONTACTVIEW_H

#include <QObject>
#include <Q3ListView>
#include <QList>
#include <QPixmap>
#include <QKeyEvent>
#include <QEvent>
#include <QDropEvent>

#include "xmpp_clientstream.h"

class IconAction;
class UserListItem;
class ContactView;
class ContactViewItemB;
class PsiAccount;
class PsiIcon;
class QTimer;
class QPixmap;
namespace XMPP {
	class Status;
}

using namespace XMPP;

class ContactProfile;

// ContactProfile: this holds/manipulates a roster profile for an account
class ContactProfile : public QObject
{
	Q_OBJECT
public:
	ContactProfile(PsiAccount *, const QString &name, ContactView *, bool unique=false);
	~ContactProfile();

	void setEnabled(bool e=TRUE);

	const QString & name() const;
	void setName(const QString &name);
	void setState(int);
	void setUsingSSL(bool);
	ContactView *contactView() const;
	ContactViewItemB *self() const;

	PsiAccount *psiAccount() const;

	void updateEntry(const UserListItem &);
	void removeEntry(const Jid &);
	void updateSelf();
	void addSelf();
	void removeSelf();
	void setAlert(const Jid &, const PsiIcon *);
	void clearAlert(const Jid &);
	void animateNick(const Jid &);

	void addAllNeededContactItems();
	void removeAllUnneededContactItems();
	void resetAllContactItemNames();

	void ensureVisible(const Jid &);
	void clear();

	QString makeTip(bool trim, bool doLinkify) const;

	ContactViewItemB *checkGroup(int type);
	ContactViewItemB *checkGroup(const QString &name);

	void setName(const char *);


signals:
	void actionDefault(const Jid &);
	void actionRecvEvent(const Jid &);
	void actionSendMessage(const Jid &);
	void actionSendMessage(const QList<XMPP::Jid>&);
	void actionSendUrl(const Jid &);
	void actionRemove(const Jid &);
	void actionRename(const Jid &, const QString &);
	void actionGroupRename(const QString &, const QString &);
	void actionHistory(const Jid &);
	void actionOpenChat(const Jid &);
	void actionOpenChatSpecific(const Jid &);
#ifdef WHITEBOARDING
	void actionOpenWhiteboard(const Jid &);
	void actionOpenWhiteboardSpecific(const Jid &);
#endif
	void actionAgentSetStatus(const Jid &, Status &);
	void actionInfo(const Jid &);
	void actionAuth(const Jid &);
	void actionAuthRequest(const Jid &);
	void actionAuthRemove(const Jid &);
	void actionAdd(const Jid &);
	void actionGroupAdd(const Jid &, const QString &);
	void actionGroupRemove(const Jid &, const QString &);
	void actionSendFile(const Jid &);
	void actionSendFiles(const Jid &, const QStringList &);
	void actionVoice(const Jid &);
	void actionExecuteCommand(const Jid &, const QString&);
	void actionExecuteCommandSpecific(const Jid &, const QString&);
	void actionDisco(const Jid &, const QString &);
	void actionInvite(const Jid &, const QString &);
	void actionAssignKey(const Jid &);
	void actionUnassignKey(const Jid &);
	void actionSetMood();
	void actionSetAvatar();
	void actionUnsetAvatar();

private slots:
	void updateGroups();

public:
	class Entry;
	class Private;
private:
	Private *d;

	ContactViewItemB *addGroup(int type);
	ContactViewItemB *addGroup(const QString &name);
	ContactViewItemB *ensureGroup(int type);
	ContactViewItemB *ensureGroup(const QString &name);
	void checkDestroyGroup(ContactViewItemB *group);
	void checkDestroyGroup(const QString &group);
	ContactViewItemB *addContactItem(Entry *e, ContactViewItemB *group_item);
	ContactViewItemB *ensureContactItem(Entry *e, ContactViewItemB *group_item);
	void removeContactItem(Entry *e, ContactViewItemB *i);
	void addNeededContactItems(Entry *e);
	void removeUnneededContactItems(Entry *e);
	void clearContactItems(Entry *e);

	void removeEntry(Entry *);
	Entry *findEntry(const Jid &) const;
	Entry *findEntry(ContactViewItemB *) const;

	void ensureVisible(Entry *);

	// useful functions to grab groups of users
	QList<XMPP::Jid> contactListFromCVGroup(ContactViewItemB *) const;
	int contactSizeFromCVGroup(ContactViewItemB *) const;
	int contactsOnlineFromCVGroup(ContactViewItemB *) const;
	QList<XMPP::Jid> contactListFromGroup(const QString &groupName) const;
	int contactSizeFromGroup(const QString &groupName) const;

	void updateGroupInfo(ContactViewItemB *group);
	QStringList groupList() const;

	void deferredUpdateGroups();

	friend class ContactView;
	void scActionDefault(ContactViewItemB *);
	void scRecvEvent(ContactViewItemB *);
	void scSendMessage(ContactViewItemB *);
	void scRename(ContactViewItemB *);
	void scVCard(ContactViewItemB *);
	void scHistory(ContactViewItemB *);
	void scOpenChat(ContactViewItemB *);
#ifdef WHITEBOARDING
	void scOpenWhiteboard(ContactViewItemB *);
#endif
	void scAgentSetStatus(ContactViewItemB *, Status &);
	void scRemove(ContactViewItemB *);
	void doItemRenamed(ContactViewItemB *, const QString &);
	void doContextMenu(ContactViewItemB *, const QPoint &);

	friend class ContactViewItemB;
	void dragDrop(const QString &, ContactViewItemB *);
	void dragDropFiles(const QStringList &, ContactViewItemB *);
};

// ContactView: the actual widget
class ContactView : public Q3ListView
{
	Q_OBJECT
public:
	ContactView(QWidget *parent=0, const char *name=0);
	~ContactView();

	bool isShowOffline() const { return v_showOffline; }
	bool isShowAgents() const { return v_showAgents; }
	bool isShowAway() const { return v_showAway; }
	bool isShowHidden() const { return v_showHidden; }
	bool isShowSelf() const { return v_showSelf; }
	bool isShowStatusMsg() const { return v_showStatusMsg; }

	bool filterContact(ContactViewItemB *item, bool refineSearch = false);
	bool filterGroup(ContactViewItemB *item, bool refineSearch = false);
	void setFilter(QString const &text);
	void clearFilter();

	void clear();
	void resetAnim();
	QTimer *animTimer() const;

	IconAction *qa_send, *qa_chat, *qa_ren, *qa_hist, *qa_logon, *qa_recv, *qa_rem, *qa_vcard;
	IconAction *qa_assignAvatar, *qa_clearAvatar;
#ifdef WHITEBOARDING
	IconAction *qa_wb;
#endif

	QSize minimumSizeHint() const;
	QSize sizeHint() const;

	void setSearch(const QString &str);

protected:
	void setShortcuts();
	// reimplemented
	void keyPressEvent(QKeyEvent *);
	bool eventFilter( QObject *, QEvent * );
	Q3DragObject *dragObject();

signals:
	void showOffline(bool);
	void showAway(bool);
	void showHidden(bool);
	void showAgents(bool);
	void showSelf(bool);
	void showStatusMsg(bool);
	void searchInput(const QString&);

public slots:
	void setShowOffline(bool);
	void setShowAgents(bool);
	void setShowAway(bool);
	void setShowHidden(bool);
	void setShowSelf(bool);
	void setShowStatusMsg(bool);
	void optionsUpdate();
	void recalculateSize();

private slots:
	void qlv_singleclick(int, Q3ListViewItem *, const QPoint &, int);
	void qlv_doubleclick(Q3ListViewItem *);
	void qlv_contextPopup(Q3ListViewItem *, const QPoint &, int);
	void qlv_contextMenuRequested(Q3ListViewItem *, const QPoint &, int);
	void qlv_itemRenamed(Q3ListViewItem *, int, const QString &);
	void qlv_selectionChanged();
	void leftClickTimeOut();

	void doRecvEvent();
	void doRename();
	void doEnter();
	void doContext();
	void doSendMessage();
	void doOpenChat();
#ifdef WHITEBOARDING
	void doOpenWhiteboard();
#endif
	void doHistory();
	void doVCard();
	void doLogon();
	void doRemove();

	void doAssignAvatar();
	void doClearAvatar();

public:
	class Private;
	friend class Private;
private:
	Private *d;

	QPoint mousePressPos; // store pressed position, idea taken from Licq
	bool v_showOffline, v_showAgents, v_showAway, v_showHidden, v_showSelf, v_showStatusMsg;
	bool lcto_active; // double click active?
	QPoint lcto_pos;
	Q3ListViewItem *lcto_item;
	QSize lastSize;
	QString filterString_;

	friend class ContactProfile;
	void link(ContactProfile *);
	void unlink(ContactProfile *);
	bool allowResize() const;

	void openSubArea(ContactViewItemB *);
	void closeSubArea(ContactViewItemB *);
};



//------------------------------------------------------------------------------
// RichListViewItem: A RichText listview item
//------------------------------------------------------------------------------

#include <Q3StyleSheet>

class RichListViewStyleSheet : public Q3StyleSheet
{
public:
	static RichListViewStyleSheet* instance();
	virtual void scaleFont(QFont& font, int logicalSize) const;

private:
	RichListViewStyleSheet(QObject* parent=0, const char * name=0);
	static RichListViewStyleSheet* instance_;
};

class Q3SimpleRichText;
class RichListViewItemB : public Q3ListViewItem
{
public:
	RichListViewItemB( Q3ListView * parent );
	RichListViewItemB( Q3ListViewItem * parent );
	virtual void setText(int column, const QString& text);
	virtual void setup();
	virtual ~RichListViewItemB();
	int widthUsed();

	int indent;

protected:
	virtual void paintCell( QPainter * p, const QColorGroup & cg, int column
, int width, int align );
//private:
	int v_widthUsed;
	bool v_selected, v_active;
	bool v_rich;
	Q3SimpleRichText* v_rt;
};

// ContactViewItemB: an entry in the ContactView (profile, group, or contact)
class ContactViewItemB : public QObject, public RichListViewItemB
{
	Q_OBJECT
public:
	enum { Profile, Group, Contact };
	enum { gGeneral, gNotInList, gAgents, gPrivate, gUser };

	ContactViewItemB(const QString &profileName, ContactProfile *, ContactView *parent);
	ContactViewItemB(const QString &groupName, int groupType, ContactProfile *, ContactView *parent);
	ContactViewItemB(const QString &groupName, int groupType, ContactProfile *, ContactViewItemB *parent);
	ContactViewItemB(UserListItem *, ContactProfile *, ContactViewItemB *parent);
	ContactViewItemB(UserListItem *, ContactProfile *, ContactView *parent);
	~ContactViewItemB();

	virtual void setup();

	void clicked(const QPoint &p, const QPoint &glob);

	ContactProfile *contactProfile() const;
	int type() const;
	int groupType() const;
	const QString & groupName() const;
	const QString & groupInfo() const;
	UserListItem *u() const;
	int status() const;
	bool isAgent() const;
	bool isAlerting() const;
	bool isAnimatingNick() const;
	int parentGroupType() const; // use with contacts: returns grouptype of parent group

	void setContact(UserListItem *);
	void setProfileName(const QString &);
	void setProfileState(int);
	void setProfileSSL(bool);
	void setGroupName(const QString &);
	void setGroupInfo(const QString &);
	void setAnimateNick();
	void setAlert(const PsiIcon *);
	void clearAlert();
	void setIcon(const PsiIcon *, bool alert = false);

	void resetStatus();
	void resetName(bool forceNoStatusMsg = false); // use this to cancel a rename
	void resetGroupName();

	void updatePosition();
	void optionsUpdate();

	// reimplemented functions
	int rtti() const;
	void paintFocus(QPainter *, const QColorGroup &, const QRect &);
	void paintBranches(QPainter *, const QColorGroup &, int, int, int);
	void paintCell(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
	void paintCellContact(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
	void setOpen(bool o);
	void insertItem(Q3ListViewItem * newChild);
	void startRename(int col);
	void takeItem(Q3ListViewItem * item);
	int compare(Q3ListViewItem *, int, bool) const;
	bool acceptDrop(const QMimeSource *) const;

	void expandArea();
	void closeArea();

	void doRename();

public slots:
	void resetAnim();
	void iconUpdated();
	void animateNick();
	void stopAnimateNick();

protected:
	void dragEntered();
	void dragLeft();
	void dropped(QDropEvent *);
	void cancelRename(int);

private:
	int type_;
	class Private;
	Private *d;

	void cacheValues();
	int rankGroup(int groupType) const;
	void drawGroupIcon();
};

#endif
