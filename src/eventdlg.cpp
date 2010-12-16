/*
 * eventdlg.cpp - dialog for sending / receiving messages and events
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

#include "eventdlg.h"

#include <QLabel>
#include <QComboBox>
#include <QLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QStringList>
#include <QTimer>
#include <QCursor>
#include <QIcon>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QResizeEvent>
#include <QShowEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QList>
#include <QVBoxLayout>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QList>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QTextDocumentFragment>

#include "psievent.h"
#include "psicon.h"
#include "psiaccount.h"
#include "textutil.h"
#include "psiiconset.h"
#include "jidutil.h"
#include "psioptions.h"
#include "msgmle.h"
#include "accountscombobox.h"
#include "common.h"
#include "xmpp_htmlelement.h"
#include "userlist.h"
#include "iconwidget.h"
#include "fancylabel.h"
#include "iconselect.h"
#include "iconlabel.h"
#include "iconwidget.h"
#include "icontoolbutton.h"
#include "psitooltip.h"
#include "serverinfomanager.h"
#include "alerticon.h"
#include "shortcutmanager.h"
#include "httpauthmanager.h"
#include "psicontactlist.h"
#include "accountlabel.h"
#include "xdata_widget.h"
#include "desktoputil.h"
#ifdef HAVE_PGPUTIL
#include "pgputil.h"
#endif
#include "psirichtext.h"

static QString findJid(const QString &s, int x, int *p1, int *p2)
{
	// scan backward for the beginning of a Jid
	int n1 = x;
	if(n1 >= (int)s.length())
		n1 = s.length()-1;
	for(; n1 >= 0; --n1) {
		if(s.at(n1) == ',') {
			++n1;
			break;
		}
	}
	if(n1 < 0)
		n1 = 0;
	// go forward, skipping whitespace
	for(; n1 < (int)s.length(); ++n1) {
		if(!s.at(n1).isSpace())
			break;
	}

	// now find the end of the Jid
	int n2 = n1;
	for(; n2 < (int)s.length(); ++n2) {
		if(s.at(n2) == ',')
			break;
	}
	--n2;
	// scan backwards from the end, skipping whitespace
	for(; n2 > n1; --n2) {
		if(!s.at(n2).isSpace())
			break;
	}
	++n2;

	*p1 = n1;
	*p2 = n2;

	return s.mid(n1, n2-n1);
}

//----------------------------------------------------------------------------
// ELineEdit - a line edit that handles advanced Jid entry
//----------------------------------------------------------------------------
// hack hack hack hack
#ifdef __GNUC__
#warning "Disabled QLineEditPrivate (see below)"
#endif
//struct QLineEditPrivate
//{
//	// qt 3.3.1
//   /*QLineEdit *q;
//   QString text;
//   int cursor;
//   int cursorTimer;
//   QPoint tripleClick;
//   int tripleClickTimer;
//   uint frame : 1;
//   uint cursorVisible : 1;
//   uint separator : 1;
//   uint readOnly : 1;
//   uint modified : 1;
//   uint direction : 5;
//   uint dragEnabled : 1;
//   uint alignment : 3;
//   uint echoMode : 2;
//   uint textDirty : 1;
//   uint selDirty : 1;
//   uint validInput : 1;
//   int ascent;
//   int maxLength;
//   int menuId;
//   int hscroll;*/
//
//	char pad[sizeof(QLineEdit *) + sizeof(QString) + (sizeof(int)*2) + sizeof(QPoint) + sizeof(int) + 3 + (sizeof(int)*3)];
//	int hscroll;
//};

ELineEdit::ELineEdit(EventDlg *parent, const char *name)
:QLineEdit(parent)
{
	setObjectName(name);
	setAcceptDrops(true);
}

void ELineEdit::dragEnterEvent(QDragEnterEvent *e)
{
	if (e->mimeData()->hasText())
		e->accept();
	else
		e->ignore();
}

void ELineEdit::dropEvent(QDropEvent *e)
{
	QString str = e->mimeData()->text();

	if(!str.isEmpty()) {
		Jid jid(str);
		if(!jid.isValid())
			setText(str);
		else {
			EventDlg *e = (EventDlg *)parent();
			QString name = e->jidToString(jid);

			bool hasComma = false, hasText = false;
			int len = text().length();
			while ( --len >= 0 ) {
				QChar c = text().at( len );
				if ( c == ',' ) {
					hasComma = true;
					break;
				}
				else if ( !c.isSpace() ) {
					hasText = true;
					break;
				}
			}

			if ( hasComma || !hasText )
				setText(text() + ' ' + name);
			else
				setText(text() + ", " + name);
		}
		setCursorPosition(text().length());

		repaint();
	}
}

void ELineEdit::keyPressEvent(QKeyEvent *e)
{
	QLineEdit::keyPressEvent(e);
	if(e->text().length() == 1 && e->text()[0].isLetterOrNumber())
		tryComplete();
}

#ifdef __GNUC__
#warning "eventdlg.cpp: Disabled right click on JID"
#endif

//Q3PopupMenu *ELineEdit::createPopupMenu()
//{
//	EventDlg *e = (EventDlg *)parent();
//	int xoff = mapFromGlobal(QCursor::pos()).x();
//	int x = characterAt(d->hscroll + xoff, 0);
//	QString str = text();
//	int p1, p2;
//	QString j = findJid(str, x, &p1, &p2);
//	if(j.isEmpty())
//		return QLineEdit::createPopupMenu();
//
//	UserResourceList list = e->getResources(j);
//
//	setCursorPosition(p1);
//	setSelection(p1, p2-p1);
//
//	url = list;
//	url.sort();
//
//	int n = 100;
//	Q3PopupMenu *rm = new Q3PopupMenu(this); //= new QPopupMenu(pm);
//	connect(rm, SIGNAL(activated(int)), SLOT(resourceMenuActivated(int)));
//
//	rm->insertItem(tr("Recipient Default"), n++);
//
//	if(!list.isEmpty()) {
//		rm->addSeparator();
//
//		for(UserResourceList::ConstIterator it = url.begin(); it != url.end(); ++it) {
//			const UserResource &r = *it;
//			QString name;
//			if(r.name().isEmpty())
//				name = QObject::tr("[blank]");
//			else
//				name = r.name();
//
//			rm->insertItem(PsiIconset::instance()->status(r.status()), name, n++);
//		}
//	}
//
//	//pm->insertItem("Change Resource", rm, -1, 0);
//	//pm->insertSeparator(1);
//
//	return rm;
//}

void ELineEdit::resourceMenuActivated(int x)
{
	if(x < 100)
		return;

	QString name;
	if(x == 100)
		name = "";
	else {
		int n = 101;
		for(UserResourceList::ConstIterator it = url.begin(); it != url.end(); ++it) {
			if(n == x) {
				name = (*it).name();
				break;
			}
			++n;
		}
	}
	url.clear();

	changeResource(name);
}


//----------------------------------------------------------------------------
// AttachView
//----------------------------------------------------------------------------
class AttachViewItem : public QListWidgetItem
{
public:
	AttachViewItem(const QString &_url, const QString &_desc, AttachView *par)
	:QListWidgetItem(par)
	{
		type = 0;
		url = _url;
		desc = _desc;

		setIcon(IconsetFactory::icon("psi/www").icon());
		setText(url + " (" + desc + ')');
		// setMultiLinesEnabled(true);
	}

	AttachViewItem(const QString &_gc, const QString& from, const QString& reason, const QString& _password, AttachView *par)
	:QListWidgetItem(par)
	{
		type = 1;
		gc = _gc;
		password = _password;

		setIcon(IconsetFactory::icon("psi/groupChat").icon());
		QString text;
		if (!from.isEmpty())
			text = QObject::tr("Invitation to %1 from %2").arg(gc).arg(from);
		else
			text = QObject::tr("Invitation to %1").arg(gc);
		if (!reason.isEmpty()) {
			text += QString(" (%1)").arg(reason);
		}
		setText(text);
		// setMultiLinesEnabled(true);
	}

	int rtti() const
	{
		return 9100;
	}

	QString url, desc;
	QString gc, password;
	int type;
};

AttachView::AttachView(QWidget* parent)
	: QListWidget(parent)
{
	v_readOnly = false;
	// addColumn(tr("Attachments"));
	// setResizeMode(QListWidget::AllColumns);

	connect(this, SIGNAL(itemDoubleClicked(QListWidgetItem *)), SLOT(qlv_doubleClicked(QListWidgetItem *)));
};

AttachView::~AttachView()
{
}

void AttachView::setReadOnly(bool b)
{
	v_readOnly = b;
}

void AttachView::urlAdd(const QString &url, const QString &desc)
{
	new AttachViewItem(url, desc, this);
	childCountChanged();
}

void AttachView::gcAdd(const QString &gc, const QString& from, const QString& reason, const QString& password)
{
	new AttachViewItem(gc, from, reason, password, this);
	childCountChanged();
}

void AttachView::contextMenuEvent(QContextMenuEvent* e)
{
	AttachViewItem* i = !selectedItems().isEmpty() ? static_cast<AttachViewItem*>(selectedItems().first()) : 0;
	if(!i)
		return;

	QAction* goToUrlAction = 0;
	QAction* copyLocationAction = 0;
	QAction* joinGroupChatAction = 0;
	QAction* removeAction = 0;

	QMenu pm(this);
	if(i->type == 0) {
		goToUrlAction = pm.addAction(tr("Go to &URL..."));
		copyLocationAction = pm.addAction(tr("Copy location"));
	}
	else {
		joinGroupChatAction = pm.addAction(tr("Join &Groupchat..."));
	}
	pm.addSeparator();
	removeAction = pm.addAction(tr("Remove"));

	if(v_readOnly) {
		removeAction->setEnabled(false);
	}

	QAction* n = pm.exec(e->globalPos());
	if(!n)
		return;

	if(n == goToUrlAction) {
		goURL(i->url);
	}
	else if(n == joinGroupChatAction) {
		actionGCJoin(i->gc, i->password);
	}
	else if(n == copyLocationAction) {
		QApplication::clipboard()->setText(i->url, QClipboard::Clipboard);
		if(QApplication::clipboard()->supportsSelection())
			QApplication::clipboard()->setText(i->url, QClipboard::Selection);
	}
	else if(n == removeAction) {
		takeItem(row(i));
		delete i;
		childCountChanged();
	}
}

void AttachView::qlv_doubleClicked(QListWidgetItem *lvi)
{
	AttachViewItem *i = (AttachViewItem *)lvi;
	if(!i)
		return;

	if(i->type == 0)
		goURL(i->url);
	else
		actionGCJoin(i->gc, i->password);
}

void AttachView::goURL(const QString &_url)
{
	if(_url.isEmpty())
		return;

	QString url = _url;
	if(url.indexOf("://") == -1)
		url.insert(0, "http://");

	DesktopUtil::openUrl(url);
}

UrlList AttachView::urlList() const
{
	UrlList list;

	for (int index = 0; index < count(); ++index) {
		AttachViewItem* i = static_cast<AttachViewItem*>(item(index));
		list += Url(i->url, i->desc);
	}

	return list;
}

void AttachView::addUrlList(const UrlList &list)
{
	for(QList<Url>::ConstIterator it = list.begin(); it != list.end(); ++it) {
		const Url &u = *it;
		urlAdd(u.url(), u.desc());
	}
}


//----------------------------------------------------------------------------
// AddUrlDlg
//----------------------------------------------------------------------------
AddUrlDlg::AddUrlDlg(QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/www").icon());
#endif
	setModal(true);

	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	connect(pb_ok, SIGNAL(clicked()), SLOT(accept()));
}

AddUrlDlg::~AddUrlDlg()
{
}


//----------------------------------------------------------------------------
// EventDlg - a window to read and write events
//----------------------------------------------------------------------------
class EventDlg::Private : public QObject
{
	Q_OBJECT
public:
	Private(EventDlg *d) {
		dlg = d;
		nextAnim_ = 0;
	}

	~Private() {
		setNextAnim(0);
	}

	void setNextAnim(PsiIcon *anim) {
		if (nextAnim_) {
			delete nextAnim_;
			nextAnim_ = 0;
		}

		if (anim)
			nextAnim_ = new AlertIcon(anim);
	}

	PsiIcon *nextAnim() const {
		return nextAnim_;
	}

	EventDlg *dlg;
	PsiCon *psi;
	PsiAccount *pa;

	bool composing;

	QLabel *lb_identity;
	AccountsComboBox *cb_ident;
	QComboBox *cb_type;
	AccountLabel *lb_ident;
	QLabel *lb_time;
	IconLabel *lb_status;
	ELineEdit *le_to;
	QLineEdit *le_from, *le_subj;
	QLabel *lb_count;
	IconToolButton *tb_url, *tb_info, *tb_history, *tb_pgp, *tb_icon;
	IconLabel *lb_pgp;
	bool enc;
	int transid;
	IconButton *pb_next;
	IconButton *pb_close, *pb_quote, *pb_deny, *pb_send, *pb_reply, *pb_chat, *pb_auth, *pb_http_confirm, *pb_http_deny;
	IconButton *pb_form_submit, *pb_form_cancel;
	ChatView *mle;
	AttachView *attachView;
	QTimer *whois;
	QString lastWhois;
	Jid jid, realJid;
	QString thread;
	QStringList completionList;
	PsiIcon *anim;
	int nextAmount;
	QWidget *w_http_id;
	QLineEdit *le_http_id;
	PsiHttpAuthRequest httpAuthRequest;
	QWidget *xdata_form;
	XDataWidget* xdata;
	QLabel* xdata_instruction;
	RosterExchangeItems rosterExchangeItems;

	bool urlOnShow;

	Message m;
	QStringList sendLeft;

private:
	PsiIcon *nextAnim_;

private slots:
	void ensureEditPosition() {
		QTextCursor cursor = mle->textCursor();
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		cursor.clearSelection();
		mle->setTextCursor(cursor);
	}

public slots:
	void addEmoticon(const PsiIcon *icon) {
		addEmoticon(icon->defaultText());
	}

	void addEmoticon(QString text) {
		if (!dlg->isActiveWindow()) {
			return;
		}

		PsiRichText::addEmoticon(mle, text);
	}

	void updateCounter() {
		lb_count->setNum(mle->getPlainText().length());
	}
};

EventDlg::EventDlg(const QString &to, PsiCon *psi, PsiAccount *pa)
	: AdvancedWidget<QWidget>(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private(this);
	d->composing = true;
	d->psi = psi;
	d->pa = 0;
	d->psi->dialogRegister(this);

	d->anim = 0;
	d->nextAmount = 0;
	d->urlOnShow = false;

	setAccount(pa);

	d->whois = new QTimer;
	connect(d->whois, SIGNAL(timeout()), SLOT(doWhois()));

	init();

	d->cb_ident->setAccount(pa);

	d->pb_send->show();
	d->le_to->setText(expandAddresses(to, false));
	d->le_to->setCursorPosition(0);

	if(PsiOptions::instance()->getOption("options.ui.message.auto-grab-urls-from-clipboard").toBool()) {
		// url in clipboard?
		QClipboard *cb = QApplication::clipboard();
		QString text = cb->text(QClipboard::Clipboard);
		if(text.isEmpty() && cb->supportsSelection()) {
			text = cb->text(QClipboard::Selection);
		}
		if(!text.isEmpty()) {
			if(text.left(7) == "http://") {
				d->attachView->urlAdd(text, "");
				cb->clear(QClipboard::Selection);
				cb->clear(QClipboard::Clipboard);
			}
		}
	}

	updateIdentity(pa);

	X11WM_CLASS("event");

	if(d->le_to->text().isEmpty())
		d->le_to->setFocus();
	else
		d->mle->setFocus();

	if(d->tb_pgp) {
		UserListItem *u = d->pa->findFirstRelevant(d->jid);
		if(u && u->isSecure(d->jid.resource()))
			d->tb_pgp->setChecked(true);
	}
}

EventDlg::EventDlg(const Jid &j, PsiAccount *pa, bool unique)
	: AdvancedWidget<QWidget>(0)
{
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private(this);
	d->composing = false;
	d->psi = pa->psi();
	d->pa = pa;
	d->jid = j;
	d->realJid = j;

	if(unique)
		d->pa->dialogRegister(this, j);
	else
		d->pa->dialogRegister(this);

	d->anim = 0;
	d->nextAmount = 0;
	d->urlOnShow = false;

	init();

	d->le_from->setText(expandAddresses(d->jid.full(), false));
	d->le_from->setCursorPosition(0);

	doWhois();

	d->pb_next->show();

	d->pb_close->setFocus();

	X11WM_CLASS("event");

	setTime(QDateTime::currentDateTime(), true);
}

EventDlg::~EventDlg()
{
	if(d->composing) {
		delete d->whois;
		d->psi->dialogUnregister(this);
	}
	else {
		d->pa->dialogUnregister(this);
	}
	delete d;
}

void EventDlg::init()
{
	QVBoxLayout *vb1 = new QVBoxLayout(this);
	vb1->setMargin(4);
	vb1->setSpacing(4);

	// first row
	QHBoxLayout *hb1 = new QHBoxLayout;
	hb1->setSpacing(4);
	vb1->addLayout(hb1);
	d->lb_identity = new QLabel(tr("Identity:"), this);
	hb1->addWidget(d->lb_identity);

	d->enc = false;
	d->transid = -1;

	if(d->composing) {
		d->lb_ident = 0;
		d->cb_ident = d->psi->accountsComboBox(this);
		connect(d->cb_ident, SIGNAL(activated(PsiAccount *)), SLOT(updateIdentity(PsiAccount *)));
		hb1->addWidget(d->cb_ident);
	}
	else {
		d->cb_ident = 0;
		d->lb_ident = new AccountLabel(this);
		d->lb_ident->setAccount(d->pa);
		d->lb_ident->setSizePolicy(QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed ));
		hb1->addWidget(d->lb_ident);
	}
	connect(d->psi, SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
	updateIdentityVisibility();

	// second row
	QHBoxLayout *hb2 = new QHBoxLayout;
	hb2->setSpacing(4);
	vb1->addLayout(hb2);

	d->lb_status = new IconLabel(this);
	PsiToolTip::install(d->lb_status);
	d->lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));

	QLabel *l;
	if(d->composing) {
		l = new QLabel(tr("To:"), this);
		hb2->addWidget(l);
		hb2->addWidget(d->lb_status);
		d->le_to = new ELineEdit(this);
		connect(d->le_to, SIGNAL(textChanged(const QString &)), SLOT(to_textChanged(const QString &)));
		connect(d->le_to, SIGNAL(changeResource(const QString &)), SLOT(to_changeResource(const QString &)));
		connect(d->le_to, SIGNAL(tryComplete()), SLOT(to_tryComplete()));
		hb2->addWidget(d->le_to);
	}
	else {
		l = new QLabel(tr("From:"), this);
		hb2->addWidget(l);
		hb2->addWidget(d->lb_status);
		d->le_from = new QLineEdit(this);
		d->le_from->setReadOnly(true);
		hb2->addWidget(d->le_from);
	}

	if(d->composing) {
		l = new QLabel(tr("Type:"), this);
		hb2->addWidget(l);
		d->cb_type = new QComboBox(this);
		d->cb_type->addItem(tr("Normal"));
		d->cb_type->addItem(tr("Chat"));
		hb2->addWidget(d->cb_type);
	}
	else {
		l = new QLabel(tr("Time:"), this);
		hb2->addWidget(l);
		d->lb_time = new QLabel(this);
		d->lb_time->setFrameStyle( QFrame::Panel | QFrame::Sunken );
		hb2->addWidget(d->lb_time);
	}

	// icon select
	//connect(d->psi->iconSelectPopup(), SIGNAL(iconSelected(const PsiIcon *)), d, SLOT(addEmoticon(const PsiIcon *)));
	connect(d->psi->iconSelectPopup(), SIGNAL(textSelected(QString)), d, SLOT(addEmoticon(QString)));

	d->tb_icon = new IconToolButton(this);
	d->tb_icon->setPsiIcon(IconsetFactory::iconPtr("psi/smile"));
	d->tb_icon->setMenu(d->psi->iconSelectPopup());
	d->tb_icon->setPopupMode(QToolButton::InstantPopup);
	// d->tb_icon->setPopupDelay(1);
	d->tb_icon->setToolTip(tr("Select icon"));
	if ( !d->composing )
		d->tb_icon->setEnabled(false);

	// message length counter
	d->le_subj = new QLineEdit(this);
	d->lb_count = new QLabel(this);
	d->lb_count->setToolTip(tr("Message length"));
	d->lb_count->setFixedWidth(40);
	d->lb_count->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	d->lb_count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	d->lb_count->setNum(0);

	if(d->composing) {
		d->tb_pgp = new IconToolButton(this);
		d->tb_pgp->setCheckable(true);
		d->tb_pgp->setToolTip(tr("Toggle encryption"));
		d->lb_pgp = 0;
	}
	else {
		d->lb_pgp = new IconLabel(this);
		d->lb_pgp->setPsiIcon(IconsetFactory::iconPtr("psi/cryptoNo"));
		d->tb_pgp = 0;
	}

	d->tb_url = new IconToolButton(this);
	connect(d->tb_url, SIGNAL(clicked()), SLOT(addUrl()));
	d->tb_url->setToolTip(tr("Add URL"));
	d->tb_info = new IconToolButton(this);
	connect(d->tb_info, SIGNAL(clicked()), SLOT(doInfo()));
	d->tb_info->setToolTip(tr("User info"));
	d->tb_history = new IconToolButton(this);
	connect(d->tb_history, SIGNAL(clicked()), SLOT(doHistory()));
	d->tb_history->setToolTip(tr("Message history"));

	QList<IconToolButton*> toolButtons;
	toolButtons << d->tb_url << d->tb_info << d->tb_history;
   	if (PsiOptions::instance()->getOption("options.pgp.enable").toBool())
		toolButtons << d->tb_pgp;
	toolButtons << d->tb_icon;
	foreach (IconToolButton *toolButton, toolButtons)
		if (toolButton)
			toolButton->setFocusPolicy(Qt::NoFocus);

	QHBoxLayout *hb3 = new QHBoxLayout;
	hb3->setSpacing(4);
	vb1->addLayout(hb3);

//	if(d->composing /* && config->showsubject */) {
	if(PsiOptions::instance()->getOption("options.ui.message.show-subjects").toBool()) {
		// third row
		l = new QLabel(tr("Subject:"), this);
		hb3->addWidget(l);
		hb3->addWidget(d->le_subj);
		hb3->addWidget(d->lb_count);
		hb3->addWidget(d->tb_icon);
		hb3->addWidget(d->tb_url);
		hb3->addWidget(d->tb_info);
		hb3->addWidget(d->tb_history);

		if(!d->composing) {
			d->le_subj->setReadOnly(true);
			d->tb_url->setEnabled(false);
			hb3->addWidget(d->lb_pgp);
		} else
			hb3->addWidget(d->tb_pgp);

	} else {
		d->le_subj->hide();
		hb2->addWidget(d->lb_count);
		hb2->addWidget(d->tb_icon);
		hb2->addWidget(d->tb_url);
		hb2->addWidget(d->tb_info);
		hb2->addWidget(d->tb_history);
		if(d->composing)
			hb2->addWidget(d->tb_pgp);
		else
			hb2->addWidget(d->lb_pgp);
	}

	// text area
	d->mle = new ChatView(this);
	d->mle->setDialog(this);
	d->mle->setReadOnly(false);
	d->mle->setUndoRedoEnabled(true);
	d->mle->setMinimumHeight(50);
	vb1->addWidget(d->mle);

	connect(d->mle, SIGNAL(textChanged()), d, SLOT(updateCounter()));

	if (d->composing) {
		d->mle->setAcceptRichText(false);
	}
	else {
		d->mle->setReadOnly(true);
		d->mle->setUndoRedoEnabled(false);
	}

	// attachment view
	d->attachView = new AttachView(this);
	d->attachView->setFixedHeight(80);
	d->attachView->hide();
	connect(d->attachView, SIGNAL(childCountChanged()), SLOT(showHideAttachView()));
	connect(d->attachView, SIGNAL(actionGCJoin(const QString &, const QString&)), SLOT(actionGCJoin(const QString &, const QString&)));
	vb1->addWidget(d->attachView);

	if(!d->composing)
		d->attachView->setReadOnly(true);
	else
		QTimer::singleShot(0, d, SLOT(ensureEditPosition()));

	// http auth transaction id
	d->w_http_id = new QWidget(this);
	QHBoxLayout *hb_http_id = new QHBoxLayout(d->w_http_id);
	hb_http_id->setMargin(0);
	hb_http_id->setSpacing(4);
	d->le_http_id = new QLineEdit(d->w_http_id);
	l = new QLabel(tr("Transaction &identifier:"), d->w_http_id);
	l->setBuddy(d->le_http_id);
	hb_http_id->addWidget(l);
	hb_http_id->addWidget(d->le_http_id);

	vb1->addWidget(d->w_http_id);
	d->w_http_id->hide();

	// data form
	d->xdata = new XDataWidget(this);
	d->xdata_form = new QWidget(this);
	QVBoxLayout *vb_xdata = new QVBoxLayout(d->xdata_form);
	d->xdata_instruction = new QLabel(d->xdata_form);
	vb_xdata->addWidget(d->xdata_instruction);
	vb_xdata->addWidget(d->xdata);
	vb1->addWidget(d->xdata_form);
	d->xdata_form->hide();

	// bottom row
	QHBoxLayout *hb4 = new QHBoxLayout;
	vb1->addLayout(hb4);
	d->pb_close = new IconButton(this);
	d->pb_close->setText(tr("&Close"));
	connect(d->pb_close, SIGNAL(clicked()), SLOT(close()));
	d->pb_close->setMinimumWidth(80);
	hb4->addWidget(d->pb_close);
	hb4->addStretch(1);
	d->pb_next = new IconButton(this);
	connect(d->pb_next, SIGNAL(clicked()), SLOT(doReadNext()));
	d->pb_next->setText(tr("&Next"));
	d->pb_next->hide();
	d->pb_next->setMinimumWidth(96);
	d->pb_next->setEnabled(false);
	hb4->addWidget(d->pb_next);
	d->pb_quote = new IconButton(this);
	d->pb_quote->setText(tr("&Quote"));
	connect(d->pb_quote, SIGNAL(clicked()), SLOT(doQuote()));
	d->pb_quote->hide();
	d->pb_quote->setMinimumWidth(96);
	hb4->addWidget(d->pb_quote);
	d->pb_deny = new IconButton(this);
	d->pb_deny->setText(tr("&Deny"));
	connect(d->pb_deny, SIGNAL(clicked()), SLOT(doDeny()));
	d->pb_deny->hide();
	d->pb_deny->setMinimumWidth(96);
	hb4->addWidget(d->pb_deny);
	d->pb_auth = new IconButton(this);
	d->pb_auth->setText(tr("&Add/Auth"));
	connect(d->pb_auth, SIGNAL(clicked()), SLOT(doAuth()));
	d->pb_auth->setPsiIcon(IconsetFactory::iconPtr("psi/addContact"));
	d->pb_auth->hide();
	d->pb_auth->setMinimumWidth(96);
	hb4->addWidget(d->pb_auth);
	d->pb_send = new IconButton(this);
	d->pb_send->setText(tr("&Send"));
	connect(d->pb_send, SIGNAL(clicked()), SLOT(doSend()));
	d->pb_send->hide();
	d->pb_send->setMinimumWidth(96);
	hb4->addWidget(d->pb_send);
	d->pb_chat = new IconButton(this);
	d->pb_chat->setText(tr("&Chat"));
	connect(d->pb_chat, SIGNAL(clicked()), SLOT(doChat()));
	d->pb_chat->hide();
	d->pb_chat->setMinimumWidth(96);
	hb4->addWidget(d->pb_chat);
	d->pb_reply = new IconButton(this);
	d->pb_reply->setText(tr("&Reply"));
	connect(d->pb_reply, SIGNAL(clicked()), SLOT(doReply()));
	d->pb_reply->hide();
	d->pb_reply->setMinimumWidth(96);
	hb4->addWidget(d->pb_reply);

	d->pb_http_confirm = new IconButton(this);
	d->pb_http_confirm->setText(tr("C&onfirm"));
	connect(d->pb_http_confirm, SIGNAL(clicked()), SLOT(doHttpConfirm()));
	d->pb_http_confirm->hide();
	d->pb_http_confirm->setMinimumWidth(96);
	hb4->addWidget(d->pb_http_confirm);

	d->pb_http_deny = new IconButton(this);
	d->pb_http_deny->setText(tr("&Deny"));
	connect(d->pb_http_deny, SIGNAL(clicked()), SLOT(doHttpDeny()));
	d->pb_http_deny->hide();
	d->pb_http_deny->setMinimumWidth(96);
	hb4->addWidget(d->pb_http_deny);
	
	// data form submit button
	d->pb_form_submit = new IconButton(this);
	d->pb_form_submit->setText(tr("&Submit"));
	connect(d->pb_form_submit, SIGNAL(clicked()), SLOT(doFormSubmit()));
	d->pb_form_submit->hide();
	d->pb_form_submit->setMinimumWidth(96);
	hb4->addWidget(d->pb_form_submit);

	// data form cancel button
	d->pb_form_cancel = new IconButton(this);
	d->pb_form_cancel->setText(tr("&Cancel"));
	connect(d->pb_form_cancel, SIGNAL(clicked()), SLOT(doFormCancel()));
	d->pb_form_cancel->hide();
	d->pb_form_cancel->setMinimumWidth(96);
	hb4->addWidget(d->pb_form_cancel);

	if (d->composing)
		setTabOrder(d->le_to, d->le_subj);
	else
		setTabOrder(d->le_from, d->le_subj);
	setTabOrder(d->le_subj, d->mle);

	updatePGP();
	connect(d->pa, SIGNAL(pgpKeyChanged()), SLOT(updatePGP()));
	connect(d->pa, SIGNAL(encryptedMessageSent(int, bool, int, const QString &)), SLOT(encryptedMessageSent(int, bool, int, const QString &)));

	bool use = PsiOptions::instance()->getOption("options.ui.remember-window-sizes").toBool();
	if (PsiOptions::instance()->getOption("options.ui.message.size").toSize().isValid() && use) {
		resize(PsiOptions::instance()->getOption("options.ui.message.size").toSize());
	} else {
		resize(defaultSize());
	}
	optionsUpdate();

	//ShortcutManager::connect("common.close", this, SLOT(close()));
	ShortcutManager::connect("common.user-info", this, SLOT(doInfo()));
	ShortcutManager::connect("common.history", this, SLOT(doHistory()));
	//ShortcutManager::connect("message.send", this, SLOT(doSend()));
}

bool EventDlg::messagingEnabled()
{
	return PsiOptions::instance()->getOption("options.ui.message.enabled").toBool();
}

void EventDlg::setAccount(PsiAccount *pa)
{
	if(d->pa)
		disconnect(d->pa, SIGNAL(updatedActivity()), this, SLOT(accountUpdatedActivity()));

	d->pa = pa;
	connect(d->pa, SIGNAL(updatedActivity()), this, SLOT(accountUpdatedActivity()));
}

void EventDlg::updateIdentity(PsiAccount *pa)
{
	if(!pa) {
		close();
		return;
	}

	setAccount(pa);

	buildCompletionList();
	doWhois(true);
}

void EventDlg::updateIdentityVisibility()
{
	bool visible = d->psi->contactList()->enabledAccounts().count() > 1;
	if (d->cb_ident)
		d->cb_ident->setVisible(visible);
	if (d->lb_ident)
		d->lb_ident->setVisible(visible);
	d->lb_identity->setVisible(visible);
}

void EventDlg::accountUpdatedActivity()
{
	// TODO: act on account activity change
}

QString EventDlg::text() const
{
	return d->mle->getPlainText();
}

void EventDlg::setHtml(const QString &s)
{
	d->mle->clear();
	d->mle->appendText(s);
}

void EventDlg::setSubject(const QString &s)
{
	d->le_subj->setText(s);
}

void EventDlg::setThread(const QString &t)
{
	d->thread = t;
}

void EventDlg::setUrlOnShow()
{
	d->urlOnShow = true;
}

PsiAccount *EventDlg::psiAccount()
{
	return d->pa;
}

QStringList EventDlg::stringToList(const QString &s, bool enc) const
{
	QStringList list;

	int x1, x2;
	x1 = 0;
	x2 = 0;
	while(1) {
		// scan along for a comma
		bool found = false;
		for(int n = x1; n < (int)s.length(); ++n) {
			if(s.at(n) == ',') {
				found = true;
				x2 = n;
				break;
			}
		}
		if(!found)
			x2 = s.length();

		QString c = s.mid(x1, (x2-x1));
		QString j;
		if(enc)
			j = findJidInString(c);
		else
			j = c;
		if(j.isEmpty())
			j = c;

		j = j.trimmed();
		//printf("j=[%s]\n", j.latin1());
		if(!j.isEmpty())
			list += j;

		if(!found)
			break;
		x1 = x2+1;
	}

	return list;
}

QString EventDlg::findJidInString(const QString &s) const
{
	int a = s.indexOf('<');
	if(a != -1) {
		++a;
		int b = s.indexOf('>', a);
		if(b != -1)
			return JIDUtil::decode822(s.mid(a, b-a));
	}
	return "";
}

QString EventDlg::expandAddresses(const QString &in, bool enc) const
{
	//printf("in: [%s]\n", in.latin1());
	QString str;
	QStringList list = stringToList(in, enc);
	bool first = true;
	for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
		if(!first)
			str += ", ";
		first = false;

		Jid j(*it);
		QList<UserListItem*> ul = d->pa->findRelevant(j);
		if(ul.isEmpty()) {
			str += j.full();
			continue;
		}
		UserListItem *u = ul.first();

		Jid jid;
		if(j.resource().isEmpty())
			jid = u->jid().full();
		else
			jid = u->jid().withResource(j.resource());

		QString name;
		if(!u->name().isEmpty())
			name += u->name() + QString(" <%1>").arg(JIDUtil::encode822(jid.full()));
		else
			name = JIDUtil::encode822(jid.full());
		str += name;
	}

	//printf("expanding: [%s]\n", str.latin1());
	return str;
}

void EventDlg::to_textChanged(const QString &)
{
	d->whois->start(250);
}

void EventDlg::to_changeResource(const QString &r)
{
	QString str = d->le_to->text();
	int start = d->le_to->selectionStart();
	// int len = d->le_to->selectedText().length();
	if(start == -1) {
		//printf("bad selection\n");
		return;
	}
	//printf("selection: [%d,%d]\n", start, len);

	int p1, p2;
	QString s = findJid(str, start, &p1, &p2);
	QString j = findJidInString(s);
	if(j.isEmpty())
		j = s;
	Jid jid(j);
	if(!jid.isValid()) {
		//printf("invalid jid\n");
		return;
	}
	//printf("s=[%s], j=[%s], p: [%d,%d]\n", s.latin1(), j.latin1(), p1, p2);

	QString js = jidToString(jid, r);
	//printf("js=[%s]\n", js.latin1());
	/*str.remove(start, len);
	str.insert(start, js);
	d->le_to->deselect();
	d->le_to->setCursorPosition(0);
	d->le_to->setText(str);
	//d->le_to->setCursorPosition(start + js.length());*/
	d->le_to->insert(js);
}

void EventDlg::to_tryComplete()
{
	if(!PsiOptions::instance()->getOption("options.ui.message.use-jid-auto-completion").toBool())
		return;

	QString str = d->le_to->text();
	int x = d->le_to->cursorPosition();

	int p1, p2;
	QString s = findJid(str, x, &p1, &p2);
	if(s.length() < 1 || x != p2)
		return;

	for(QStringList::ConstIterator it = d->completionList.begin(); it != d->completionList.end(); ++it) {
		QString name = *it;
		if(s.length() > name.length())
			continue;

		bool ok = true;
		int n;
		for(n = 0; n < (int)s.length(); ++n) {
			if(s.at(n).toLower() != name.at(n).toLower()) {
				ok = false;
				break;
			}
		}
		name = name.mid(n);
		if(ok) {
			d->le_to->insert(name);
			d->le_to->setCursorPosition(x);
			d->le_to->setSelection(x, name.length());
			break;
		}
	}
}

void EventDlg::buildCompletionList()
{
	d->completionList.clear();

	d->completionList += d->pa->jid().full();

	foreach(UserListItem* u, *d->pa->userList()) {
		QString j = u->jid().full();
		if(!u->name().isEmpty())
			d->completionList += u->name() + " <"+j+'>';
		d->completionList += j;
	}
}

QString EventDlg::jidToString(const Jid &jid, const QString &r) const
{
	QString name;

	QList<UserListItem*> ul = d->pa->findRelevant(jid);
	if(!ul.isEmpty()) {
		UserListItem *u = ul.first();

		QString j;
		if(r.isEmpty())
			j = u->jid().full();
		else
			j = Jid(u->jid().bare()).withResource(r).full();

		if(!u->name().isEmpty())
			name = u->name() + QString(" <%1>").arg(JIDUtil::encode822(j));
		else
			name = JIDUtil::encode822(j);
	}
	else
		name = JIDUtil::encode822(jid.full());

	return name;
}

void EventDlg::doWhois(bool force)
{
	QString str;
	if(d->composing) {
		str = d->le_to->text();
		if(str == d->lastWhois && !force)
			return;
	}
	else {
		str = d->le_from->text();
	}

	//printf("whois: [%s]\n", str.latin1());
	d->lastWhois = str;
	QStringList list = stringToList(str);
	bool found = false;
	if(list.count() == 1) {
		Jid j(list[0]);
		d->jid = j;
		QList<UserListItem*> ul = d->pa->findRelevant(j);
		if(!ul.isEmpty()) {
			d->tb_info->setEnabled(true);
			d->tb_history->setEnabled(true);
			found = true;
		}
		updateContact(d->jid);
	}
	if(!found) {
		d->jid = "";

		d->lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
		d->tb_info->setEnabled(false);
		d->tb_history->setEnabled(false);
		setWindowTitle(tr("Send Message"));
		d->lb_status->setToolTip(QString());
	}
}

UserResourceList EventDlg::getResources(const QString &s) const
{
	UserResourceList list;

	QString j = findJidInString(s);
	if(j.isEmpty())
		j = s;
	Jid jid(j);
	if(!jid.isValid())
		return list;

	QList<UserListItem*> ul = d->pa->findRelevant(jid);
	if(!ul.isEmpty()) {
		UserListItem *u = ul.first();
		if(u->isAvailable())
			return u->userResourceList();
	}

	return list;
}

void EventDlg::optionsUpdate()
{
	// update the font
	QFont f;
	f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.message").toString());
	d->mle->setFont(f);

	// update status icon
	doWhois(true);

	if ( PsiOptions::instance()->getOption("options.ui.message.show-character-count").toBool() && d->composing )
		d->lb_count->show();
	else
		d->lb_count->hide();

	if ( PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool() )
		d->tb_icon->show();
	else
		d->tb_icon->hide();

	// tool buttons: not required
	d->tb_url->setPsiIcon(IconsetFactory::iconPtr("psi/www"));
	d->tb_info->setPsiIcon(IconsetFactory::iconPtr("psi/vCard"));
	d->tb_history->setPsiIcon(IconsetFactory::iconPtr("psi/history"));
	if(d->tb_pgp) {
		QIcon i;
		i.addPixmap(IconsetFactory::icon("psi/cryptoNo").impix(),  QIcon::Normal, QIcon::Off);
		i.addPixmap(IconsetFactory::icon("psi/cryptoYes").impix(), QIcon::Normal, QIcon::On);
		d->tb_pgp->setPsiIcon(0);
		d->tb_pgp->setIcon(i);
	}
	if(d->lb_pgp)
		d->lb_pgp->setPsiIcon(IconsetFactory::iconPtr(d->enc ? "psi/cryptoYes" : "psi/cryptoNo"));

	// update the readnext icon
	if(d->nextAmount > 0)
		d->pb_next->forceSetPsiIcon(d->nextAnim());

	// update the widget icon
#ifndef Q_WS_MAC
	if(d->composing) {
		setWindowIcon(IconsetFactory::icon("psi/sendMessage").icon());
	}
	else {
		if(d->anim)
			setWindowIcon(d->anim->icon());
	}
#endif
}

QSize EventDlg::defaultSize()
{
	return QSize(420, 280);
}

void EventDlg::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);

	if(d->urlOnShow) {
		d->urlOnShow = false;
		QTimer::singleShot(1, this, SLOT(addUrl()));
	}
}

void EventDlg::resizeEvent(QResizeEvent *e)
{
	if(PsiOptions::instance()->getOption("options.ui.remember-window-sizes").toBool()) {
		PsiOptions::instance()->setOption("options.ui.message.size", e->size());
	}
}

void EventDlg::keyPressEvent(QKeyEvent *e)
{
	// FIXMEKEY
	QKeySequence key = e->key() + ( e->modifiers() & ~Qt::KeypadModifier);
	if(ShortcutManager::instance()->shortcuts("common.close").contains(key))
		close();
	else if(ShortcutManager::instance()->shortcuts("message.send").contains(key))
		doSend();
	else
		e->ignore();
}

void EventDlg::closeEvent(QCloseEvent *e)
{
	// really lame way of checking if we are encrypting
	if(!d->mle->isEnabled())
		return;

	e->accept();
}

void EventDlg::doSend()
{
	if(!d->composing)
		return;

	if(!d->pb_send->isEnabled())
		return;

	if(!d->pa->checkConnected(this))
		return;

	if(d->mle->getPlainText().isEmpty() && d->attachView->count() == 0) {
		QMessageBox::information(this, tr("Warning"), tr("Please type in a message first."));
		return;
	}

	QStringList list = stringToList(d->le_to->text());
	if(list.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("No recipients have been specified!"));
		return;
	}

	Message m;
	if(d->cb_type->currentIndex() == 0)
		m.setType("");
	else
		m.setType("chat");

	m.setBody(d->mle->getPlainText());
	m.setSubject(d->le_subj->text());
	m.setUrlList(d->attachView->urlList());
	m.setTimeStamp(QDateTime::currentDateTime());
	m.setThread(d->thread);
	if(d->tb_pgp->isChecked())
		m.setWasEncrypted(true);
	d->m = m;

	d->enc = false;
	if(d->tb_pgp->isChecked()) {
		d->le_to->setEnabled(false);
		d->mle->setEnabled(false);
		d->enc = true;
		d->sendLeft = list;

		trySendEncryptedNext();
	}
	else {
		if (list.count() > 1 && !d->pa->serverInfoManager()->multicastService().isEmpty() && PsiOptions::instance()->getOption("options.enable-multicast").toBool()) {
				m.setTo(d->pa->serverInfoManager()->multicastService());
				foreach(QString recipient, list) {
					m.addAddress(Address(XMPP::Address::To, Jid(recipient)));
				}
				d->pa->dj_sendMessage(m, false);
		}
		else {
			for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
				m.setTo(Jid(*it));
				d->pa->dj_sendMessage(m, false);
			}
		}
		doneSend();
	}
}

void EventDlg::doneSend()
{
	close();
}

void EventDlg::doReadNext()
{
	aReadNext(d->realJid);
}

void EventDlg::doChat()
{
	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;

	Jid j(list[0]);
	aChat(j);
}

void EventDlg::doReply()
{
	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;
	Jid j(list[0]);
	aReply(j, "", d->le_subj->text(), d->thread);
}

void EventDlg::doQuote()
{
	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;
	Jid j(list[0]);

	QString body = TextUtil::rich2plain(d->mle->getHtml());
	aReply(j, body, d->le_subj->text(), d->thread);
}

void EventDlg::doDeny()
{
	if(!d->pa->checkConnected(this))
		return;

	if (d->rosterExchangeItems.isEmpty()) {
		QStringList list = stringToList(d->le_from->text());
		if(list.isEmpty())
			return;
		Jid j(list[0]);
		aDeny(j);
	}
	else {
		d->rosterExchangeItems.clear();
	}
	close();
}

void EventDlg::doAuth()
{
	if(!d->pa->checkConnected(this))
		return;

	if (d->rosterExchangeItems.isEmpty()) {
		QStringList list = stringToList(d->le_from->text());
		if(list.isEmpty())
			return;
		Jid j(list[0]);
		aAuth(j);
	}
	else {
		aRosterExchange(d->rosterExchangeItems);
		d->rosterExchangeItems.clear();
	}
	d->pb_auth->setEnabled(false);
	closeAfterReply();
}

/*!
	Executed when user wants to confirm a HTTP request.
*/
void EventDlg::doHttpConfirm()
{
	if(!d->pa->checkConnected(this))
		return;

	if(!d->httpAuthRequest.hasId()) {
		const QString id = d->le_http_id->text();
		if(id.isEmpty()) {
			QMessageBox::information(this, tr("Warning"), tr("Please type in a transaction identifier first."));
			d->le_http_id->setFocus();
			return;
		}
		else {
			d->httpAuthRequest.setId(id);
		}
	}

	aHttpConfirm(d->httpAuthRequest);

	d->le_http_id->setEnabled(false);
	d->pb_http_confirm->setEnabled(false);
	d->pb_http_deny->setEnabled(false);
	closeAfterReply();
}

/*!
	Executed when user wants to deny a HTTP request.
*/
void EventDlg::doHttpDeny()
{
	if(!d->pa->checkConnected(this))
		return;

	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;
	Jid j(list[0]);

	aHttpDeny(d->httpAuthRequest);

	d->le_http_id->setEnabled(false);
	d->pb_http_confirm->setEnabled(false);
	d->pb_http_deny->setEnabled(false);
	closeAfterReply();
}

/*!
	Executed when user wants to submit a completed form
*/
void EventDlg::doFormSubmit()
{
	//get original sender
	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;
	Jid j(list[0]);
	
	//populate the data fields
	XData data;
	data.setFields(d->xdata->fields());

	//ensure that the user completed all required fields
	if(!data.isValid()) {
		QMessageBox::information(this, tr("Warning"), tr("Please complete all required fields (marked with a '*')."));
		d->xdata_form->setFocus();
		return;
	}

	data.setType(XData::Data_Submit);
	aFormSubmit(data, d->thread, j);

	d->pb_form_submit->setEnabled(false);
	d->pb_form_cancel->setEnabled(false);

	closeAfterReply();
}

/*!
	Executed when user wants to cancel a form
*/
void EventDlg::doFormCancel()
{
	//get original sender
	QStringList list = stringToList(d->le_from->text());
	if(list.isEmpty())
		return;
	Jid j(list[0]);

	XData data;
	data.setType(XData::Data_Cancel);
	aFormCancel(data, d->thread, j);

	d->pb_form_submit->setEnabled(false);
	d->pb_form_cancel->setEnabled(false);

	closeAfterReply();
}

void EventDlg::doHistory()
{
	d->pa->actionHistory(d->jid.withResource(""));
}

void EventDlg::doInfo()
{
	d->pa->actionInfo(d->jid);
}

void EventDlg::closeAfterReply()
{
	if(d->nextAmount == 0)
		close();
}

void EventDlg::addUrl()
{
	AddUrlDlg *w = new AddUrlDlg(this);
	int n = w->exec();
	if(n != QDialog::Accepted) {
		delete w;
		return;
	}

	QString url = w->le_url->text();
	QString desc = w->le_desc->text();
	delete w;

	d->attachView->urlAdd(url, desc);
}

void EventDlg::showHideAttachView()
{
	if(d->attachView->count()) {
		if(d->attachView->isHidden())
			d->attachView->show();
	}
	else {
		if(!d->attachView->isHidden())
			d->attachView->hide();
	}
}

void EventDlg::updateContact(const Jid &jid)
{
	if(d->jid.compare(jid, false)) {
		QString rname = d->jid.resource();
		QList<UserListItem*> ul = d->pa->findRelevant(d->jid);
		UserListItem *u = 0;
		int status = -1;
		if(!ul.isEmpty()) {
			u = ul.first();
			if(rname.isEmpty()) {
				// use priority
				if(!u->isAvailable())
					status = STATUS_OFFLINE;
				else
					status = makeSTATUS((*u->userResourceList().priority()).status());
			}
			else {
				// use specific
				UserResourceList::ConstIterator rit = u->userResourceList().find(rname);
				if(rit != u->userResourceList().end())
					status = makeSTATUS((*rit).status());
				else
					status = STATUS_OFFLINE;
			}
		}

		if(status == -1 || !u)
			d->lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
		else
			d->lb_status->setPsiIcon(PsiIconset::instance()->statusPtr(jid, status));

		if(u)
			d->lb_status->setToolTip(u->makeTip(true, false));
		else
			d->lb_status->setToolTip(QString());

		if(u)
			setWindowTitle(JIDUtil::nickOrJid(u->name(), u->jid().full()));
	}
}

void EventDlg::setTime(const QDateTime &t, bool late)
{
	QString str;
	//str.sprintf("<nobr>%02d/%02d %02d:%02d:%02d</nobr>", t.date().month(), t.date().day(), t.time().hour(), t.time().minute(), t.time().second());
	str = QString("<nobr>") + t.toString(Qt::LocalDate) + "</nobr>";
	if(late)
		str = QString("<font color=\"red\">") + str + "</font>";

	d->lb_time->setText(str);
}

void EventDlg::updateEvent(PsiEvent *e)
{
	// Default buttons setup
	d->pb_next->show();
	d->pb_chat->show();
	d->pb_reply->show();
	d->pb_quote->show();
	d->pb_close->show();
	d->mle->show();
	d->pb_auth->hide();
	d->pb_deny->hide();
	d->pb_form_submit->hide();
	d->pb_form_cancel->hide();
	d->pb_http_confirm->hide();
	d->pb_http_deny->hide();
	d->xdata_form->hide();

	PsiIcon *oldanim = d->anim;
	d->anim = PsiIconset::instance()->event2icon(e);

	if(d->anim != oldanim)
		setWindowIcon(d->anim->icon());

	d->le_from->setText(expandAddresses(e->from().full(), false));
	d->le_from->setCursorPosition(0);
	d->le_from->setToolTip(e->from().full());
	setTime(e->timeStamp(), e->late());

	d->enc = false;

	bool showHttpId = false;

	if (e->type() == PsiEvent::HttpAuth) {

		HttpAuthEvent *hae = (HttpAuthEvent *)e;
		const HttpAuthRequest &confirm = hae->request();

		QString body(tr(
				"Someone (maybe you) has requested access to the following resource:\n"
				"URL: %1\n"
				"Method: %2\n").arg(confirm.url()).arg(confirm.method()));

		if (!confirm.hasId()) {
			body += tr("\n"
				"If you wish to confirm this request, please provide transaction identifier and press Confirm button. Otherwise press Deny button.");

			showHttpId = true;
		}
		else {
			body += tr("Transaction identifier: %1\n"
				"\n"
				"If you wish to confirm this request, please press Confirm button. "
				"Otherwise press Deny button.").arg(confirm.id());
		}
		Message m(hae->message());
		m.setBody(body);
		hae->setMessage(m);

		d->httpAuthRequest = hae->request();
	}

	if (showHttpId) {
		d->le_http_id->clear();
		d->le_http_id->setEnabled(true);
		d->w_http_id->show();
		d->le_http_id->setFocus();
	}
	else {
		d->w_http_id->hide();
	}
	
	if(e->type() == PsiEvent::Message || e->type() == PsiEvent::HttpAuth) {
		MessageEvent *me = (MessageEvent *)e;
		const Message &m = me->message();

		d->enc = m.wasEncrypted();

		// HTTP auth request buttons
		if ( e->type() == PsiEvent::HttpAuth ) {
			d->pb_chat->hide();
			d->pb_reply->hide();
			d->pb_quote->hide();

			d->pb_http_confirm->setEnabled(true);
			d->pb_http_confirm->show();
			d->pb_http_deny->setEnabled(true);
			d->pb_http_deny->show();
		}

		bool xhtml = m.containsHTML() && PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !m.html().text().isEmpty();
		QString txt = xhtml ? m.html().toString("div") : TextUtil::plain2rich(m.body());

		// show subject line if the incoming message has one
		if(!m.subject().isEmpty() && !PsiOptions::instance()->getOption("options.ui.message.show-subjects").toBool())
			txt = "<p><font color=\"red\"><b>" + tr("Subject:") + " " + TextUtil::plain2rich(m.subject()) + "</b></font></p>" + (xhtml? "" : "<br>") + txt;

		if (!xhtml) {
			txt = TextUtil::linkify(txt);
			txt = TextUtil::emoticonify(txt);
			txt = TextUtil::legacyFormat(txt);
		}

		if ( e->type() == PsiEvent::HttpAuth )
			txt = "<big>[HTTP Request Confirmation]</big><br>" + txt;

		setHtml("<qt>" + txt + "</qt>");

		d->le_subj->setText(m.subject());
		d->le_subj->setCursorPosition(0);

		d->thread = m.thread();

		// Form buttons
		if (!m.getForm().fields().empty()) {
			d->pb_chat->hide();
			d->pb_reply->hide();
			d->pb_quote->hide();
			d->pb_close->hide();
			d->mle->hide();

			//show/enable controls we want
			d->pb_form_submit->show();
			d->pb_form_cancel->show();
			d->pb_form_submit->setEnabled(true);
			d->pb_form_cancel->setEnabled(true);
			//set title if specified
			const XData& form = m.getForm();
			if ( !form.title().isEmpty() )
				setWindowTitle( form.title() );
			
			//show data form
			d->xdata->setFields( form.fields() );
			d->xdata_form->show();
			
			//set instructions
			QString str = TextUtil::plain2rich( form.instructions() );
			d->xdata_instruction->setText(str);
		}

		d->attachView->clear();
		d->attachView->addUrlList(m.urlList());

		if(!m.mucInvites().isEmpty()) {
			MUCInvite i = m.mucInvites().first();
			d->attachView->gcAdd(m.from().full(),i.from().bare(),i.reason(),m.mucPassword());
		}
		else if(!m.invite().isEmpty())
			d->attachView->gcAdd(m.invite());
		showHideAttachView();
	}
	else if(e->type() == PsiEvent::Auth) {
		AuthEvent *ae = (AuthEvent *)e;
		QString type = ae->authType();

		d->le_subj->setText("");
		if(type == "subscribe") {
			QString body(tr("<big>[System Message]</big><br>This user wants to subscribe to your presence.  Click the button labelled \"Add/Auth\" to authorize the subscription.  This will also add the person to your contact list if it is not already there."));
			setHtml("<qt>" + body + "</qt>");

			d->pb_chat->show();
			d->pb_reply->hide();
			d->pb_quote->hide();

			d->pb_auth->setEnabled(true);
			d->pb_auth->show();
			d->pb_deny->show();

			d->pb_http_confirm->hide();
			d->pb_http_deny->hide();

		}
		else if(type == "subscribed") {
			QString body(tr("<big>[System Message]</big><br>You are now authorized."));
			setHtml("<qt>" + body + "</qt>");

			d->pb_auth->hide();
			d->pb_deny->hide();
			d->pb_chat->show();
			d->pb_reply->show();
			d->pb_quote->show();
			d->pb_http_confirm->hide();
			d->pb_http_deny->hide();
		}
		else if(type == "unsubscribed") {
			QString body(tr("<big>[System Message]</big><br>Your authorization has been removed!"));
			setHtml("<qt>" + body + "</qt>");

			d->pb_auth->hide();
			d->pb_deny->hide();
			d->pb_chat->show();
			d->pb_reply->show();
			d->pb_quote->show();
			d->pb_http_confirm->hide();
			d->pb_http_deny->hide();
		}
	}
	else if (e->type() == PsiEvent::RosterExchange) {
		RosterExchangeEvent *re = (RosterExchangeEvent *)e;
		int additions = 0, deletions = 0, modifications = 0;
		foreach(RosterExchangeItem item, re->rosterExchangeItems()) {
			switch(item.action()) {
				case RosterExchangeItem::Add:
					additions++;
					break;
				case RosterExchangeItem::Delete:
					deletions++;
					break;
				case RosterExchangeItem::Modify:
					modifications++;
					break;
			}
		}
		QString action;
		if (additions > 0) {
			if (additions > 1) 
				action += QString(tr("%1 additions")).arg(additions);
			else 
				action += QString(tr("1 addition"));
			if (deletions > 0 || modifications > 0)
				action += ", ";
		}
		if (deletions > 0) {
			if (deletions > 1) 
				action += QString(tr("%1 deletions")).arg(deletions);
			else 
				action += QString(tr("1 deletion"));
			if (modifications > 0)
				action += ", ";
		}
		if (modifications > 0) {
			if (modifications > 1) 
				action += QString(tr("%1 modifications")).arg(modifications);
			else 
				action += QString(tr("1 modification"));
		}
		
		d->le_subj->setText("");
		QString body = QString(tr("<big>[System Message]</big><br>This user wants to modify your roster (%1). Click the button labelled \"Add/Auth\" to authorize the modification.")).arg(action);
		setHtml("<qt>" + body + "</qt>");
		d->rosterExchangeItems = re->rosterExchangeItems();

		d->pb_chat->show();
		d->pb_reply->hide();
		d->pb_quote->hide();

		d->pb_auth->setEnabled(true);
		d->pb_auth->show();
		d->pb_deny->show();
	}

	d->mle->scrollToTop();

	if(d->lb_pgp)
		d->lb_pgp->setPsiIcon( IconsetFactory::iconPtr(d->enc ? "psi/cryptoYes" : "psi/cryptoNo") );

	if(!d->le_subj->text().isEmpty())
		d->le_subj->setToolTip(d->le_subj->text());
	else
		d->le_subj->setToolTip(QString());

	doWhois();
}

void EventDlg::updateReadNext(PsiIcon *nextAnim, int nextAmount)
{
	int oldAmount = d->nextAmount;

	d->setNextAnim(nextAnim);
	d->nextAmount = nextAmount;

	if(nextAmount == 0) {
		d->setNextAnim(0);
		d->pb_next->forceSetPsiIcon(0);
		d->pb_next->setEnabled(false);
		d->pb_next->setText(tr("&Next"));

		if(d->pb_reply->isVisibleTo(this) && d->pb_reply->isEnabled())
			d->pb_reply->setFocus();
		else if(d->pb_auth->isVisibleTo(this))
			d->pb_auth->setFocus();
		else if(d->w_http_id->isVisibleTo(this))
			d->le_http_id->setFocus();
		else if(d->pb_http_deny->isVisibleTo(this))
			d->pb_http_deny->setFocus();
	}
	else {
		d->pb_next->setEnabled(true);
		QString str(tr("&Next"));
		str += QString(" - %1").arg(nextAmount);
		d->pb_next->setText(str);

		d->pb_next->forceSetPsiIcon(d->nextAnim());

		if(d->nextAmount > oldAmount)
			d->pb_next->setFocus();
	}
}

void EventDlg::actionGCJoin(const QString &gc, const QString&)
{
	Jid j(gc);
	d->pa->actionJoin(j.withResource(""));
}

void EventDlg::updatePGP()
{
	if(d->tb_pgp) {
		d->tb_pgp->setEnabled(d->pa->hasPGP());
		if(!d->pa->hasPGP()) {
			d->tb_pgp->setChecked(false);
		}
	}
}

void EventDlg::trySendEncryptedNext()
{
	if(d->sendLeft.isEmpty())
		return;
	Message m = d->m;
	m.setTo(Jid(d->sendLeft.first()));
	d->transid = d->pa->sendMessageEncrypted(m);
	if(d->transid == -1) {
		d->le_to->setEnabled(true);
		d->mle->setEnabled(true);
		d->mle->setFocus();
		return;
	}
}

void EventDlg::encryptedMessageSent(int x, bool b, int e, const QString &dtext)
{
#ifdef HAVE_PGPUTIL
	if(d->transid == -1)
		return;
	if(d->transid != x)
		return;
	d->transid = -1;
	if(b) {
		// remove the item
		Jid j(d->sendLeft.takeFirst());

		//d->pa->toggleSecurity(j, d->enc);

		if(d->sendLeft.isEmpty()) {
			d->le_to->setEnabled(true);
			d->mle->setEnabled(true);
			doneSend();
		}
		else {
			trySendEncryptedNext();
			return;
		}
	}
	else {
		PGPUtil::showDiagnosticText(static_cast<QCA::SecureMessage::Error>(e), dtext);
	}

	d->le_to->setEnabled(true);
	d->mle->setEnabled(true);
	d->mle->setFocus();
#else
	Q_ASSERT(false);
#endif
}

#include "eventdlg.moc"
