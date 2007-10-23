/*
 * chatdlg.cpp - dialog for handling chats
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

#include "chatdlg.h"

#include <QLabel>
#include <QCursor>
#include <Q3DragObject>
#include <QLineEdit>
#include <QToolButton>
#include <QLayout>
#include <QSplitter>
#include <QToolBar>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>
#include <QColor>
#include <Qt>
#include <QCloseEvent>
#include <QList>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QDropEvent>
#include <QList>
#include <QMessageBox>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QTextDocument> // for Qt::escape()

#include "profiles.h"
#include "psiaccount.h"
#include "common.h"
#include "userlist.h"
#include "stretchwidget.h"
#include "psiiconset.h"
#include "iconwidget.h"
#include "textutil.h"
#include "xmpp_message.h"
#include "xmpp_htmlelement.h"
#include "fancylabel.h"
#include "msgmle.h"
#include "iconselect.h"
#include "pgputil.h"
#include "psicon.h"
#include "iconlabel.h"
#include "capsmanager.h"
#include "iconaction.h"
#include "avatars.h"
#include "jidutil.h"
#include "tabdlg.h"
#include "psioptions.h"
#include "psitooltip.h"
#include "shortcutmanager.h"
#include "psicontactlist.h"
#include "accountlabel.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

//----------------------------------------------------------------------------
// ChatDlg
//----------------------------------------------------------------------------
class ChatDlg::Private : public QObject
{
	Q_OBJECT
public:
	Private(ChatDlg *d) {
		dlg = d;
	}

	ChatDlg *dlg;
	Jid jid;
	PsiAccount *pa;
	QString dispNick;
	int status;
	QString statusString;

	QMenu *pm_settings;
	IconAction *act_clear, *act_history, *act_info, *act_pgp, *act_icon, *act_file, *act_compact, *act_voice;
	QAction *act_send, *act_scrollup, *act_scrolldown, *act_close;

	int pending;
	bool keepOpen, warnSend;

	QTimer *selfDestruct;

	QString key;
	int transid;
	Message m;
	bool lastWasEncrypted;
	bool smallChat;

	// Message Events & Chat States
	QTimer *composingTimer;
	bool isComposing;
	bool sendComposingEvents;
	QString eventId;
	ChatState contactChatState;
	ChatState lastChatState;

	QDateTime lastMsgTime;

signals:
	// Signals if user (re)started/stopped composing
	void composing(bool);

public slots:
	void setContactToolTip(QString text) {
		dlg->ui_.lb_status->setToolTip(text);
		dlg->ui_.avatar->setToolTip(text);
	}

	void addEmoticon(const PsiIcon *icon) {
		if ( !dlg->isActiveWindow() ) {
			return;
		}

		QString text = icon->defaultText();

		if (!text.isEmpty()) {
			dlg->ui_.mle->chatEdit()->insert(text + " ");
		}
	}

	void addEmoticon(QString text) {
		if ( !pa->psi()->isChatActiveWindow(dlg) ) {
			return;
		}
		dlg->ui_.mle->chatEdit()->insert( text + " " );
	}

	void updateLastMsgTime(QDateTime t)
	{
		if (t.date() != lastMsgTime.date()) {
			dlg->ui_.log->appendText(QString("<font color=\"#00A000\">") + QString(" *** %1</font>").arg(t.date().toString(Qt::ISODate)));
		}
		lastMsgTime = t;
	}

	void updateCounter() {
		dlg->ui_.lb_count->setNum(dlg->ui_.mle->chatEdit()->text().length());
	}

	// Records that the user is composing
	void setComposing() {
		if (!composingTimer) {
			/* User (re)starts composing */
			composingTimer = new QTimer(this);
			connect(composingTimer, SIGNAL(timeout()), SLOT(composingTimeout()));
			composingTimer->start(2000); // FIXME: magic number
			emit composing(true);
		}
		isComposing = true;
	}

	// Checks if the user is still composing
	void composingTimeout() {
		if (!isComposing) {
			// User stopped composing
			delete composingTimer;
			composingTimer = 0;
			emit composing(false);
		}
		isComposing = false; // Reset composing
	}

	void resetComposing() {
		if (composingTimer) {
			delete composingTimer;
			composingTimer = 0;
			isComposing = false;
		}
	}
};

ChatDlg::ChatDlg(const Jid &jid, PsiAccount *pa, TabManager *tabManager)
	: Tabbable(jid, pa, tabManager), highlightersInstalled_(false)
{
  	if ( option.brushedMetal ) {
		setAttribute(Qt::WA_MacMetalStyle);
	}
	d = new Private(this);
	d->jid = jid;
	d->pa = pa;

	d->pending = 0;
	d->keepOpen = false;
	d->warnSend = false;
	d->selfDestruct = 0;
	d->transid = -1;
	d->key = "";
	d->lastWasEncrypted = false;

	setAcceptDrops(TRUE);

	ui_.setupUi(this);
	ui_.lb_ident->setAccount(d->pa);
	ui_.lb_ident->setShowJid(false);

	PsiToolTip::install(ui_.lb_status);
	ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));

	ui_.tb_emoticons->setIcon(IconsetFactory::icon("psi/smile").icon());

	connect(ui_.mle, SIGNAL(textEditCreated(QTextEdit*)), SLOT(chatEditCreated()));
	chatEditCreated();

#ifdef Q_WS_MAC
	connect(ui_.log, SIGNAL(selectionChanged()), SLOT(logSelectionChanged()));
#endif

	d->act_clear = new IconAction (tr("Clear chat window"), "psi/clearChat", tr("Clear chat window"), 0, this);
	connect( d->act_clear, SIGNAL( activated() ), SLOT( doClearButton() ) );

	connect(pa->psi()->iconSelectPopup(), SIGNAL(textSelected(QString)), d, SLOT(addEmoticon(QString)));
	d->act_icon = new IconAction( tr( "Select icon" ), "psi/smile", tr( "Select icon" ), 0, this );
	d->act_icon->setMenu( pa->psi()->iconSelectPopup() );
	ui_.tb_emoticons->setMenu( pa->psi()->iconSelectPopup());

	d->act_voice = new IconAction( tr( "Voice Call" ), "psi/voice", tr( "Voice Call" ), 0, this );
	connect(d->act_voice, SIGNAL(activated()), SLOT(doVoice()));
	d->act_voice->setEnabled(false);
	
	d->act_file = new IconAction( tr( "Send file" ), "psi/upload", tr( "Send file" ), 0, this );
	connect( d->act_file, SIGNAL( activated() ), SLOT( doFile() ) );

	d->act_pgp = new IconAction( tr( "Toggle encryption" ), "psi/cryptoNo", tr( "Toggle encryption" ), 0, this, 0, true );
	ui_.tb_pgp->setDefaultAction(d->act_pgp);

	d->act_info = new IconAction( tr( "User info" ), "psi/vCard", tr( "User info" ), 0, this );
	connect( d->act_info, SIGNAL( activated() ), SLOT( doInfo() ) );

	d->act_history = new IconAction( tr( "Message history" ), "psi/history", tr( "Message history" ), 0, this );
	connect( d->act_history, SIGNAL( activated() ), SLOT( doHistory() ) );
	
	d->act_compact = new IconAction( tr( "Toggle Compact/Full size" ), "psi/compact", tr( "Toggle Compact/Full size" ), 0, this );
	connect( d->act_compact, SIGNAL( activated() ), SLOT( toggleSmallChat() ) );

	d->act_send = new QAction(this);
	addAction(d->act_send);
	connect(d->act_send,SIGNAL(activated()), SLOT(doSend()));
	d->act_close = new QAction(this);
	addAction(d->act_close);
	connect(d->act_close,SIGNAL(activated()), SLOT(close()));
	d->act_scrollup = new QAction(this);
	addAction(d->act_scrollup);
	connect(d->act_scrollup,SIGNAL(activated()), SLOT(scrollUp()));
	d->act_scrolldown = new QAction(this);
	addAction(d->act_scrolldown);
	connect(d->act_scrolldown,SIGNAL(activated()), SLOT(scrollDown()));

	setShortcuts();

	ui_.toolbar->setWindowTitle(tr("Chat toolbar"));
	ui_.toolbar->setIconSize(QSize(16, 16));
	ui_.toolbar->addAction(d->act_clear);
	ui_.toolbar->addWidget(new StretchWidget(ui_.toolbar));
	ui_.toolbar->addAction(d->act_icon);
	ui_.toolbar->addAction(d->act_file);
	if (PsiOptions::instance()->getOption("options.pgp.enable").toBool()) {
		ui_.toolbar->addAction(d->act_pgp);
	}
	ui_.toolbar->addAction(d->act_info);
	ui_.toolbar->addAction(d->act_history);
	if (d->pa->voiceCaller()) {
		ui_.toolbar->addAction(d->act_voice);
	}

	PsiToolTip::install(ui_.avatar);
	
	updateAvatar();
	connect(d->pa->avatarFactory(),SIGNAL(avatarChanged(const Jid&)), this, SLOT(updateAvatar(const Jid&)));

	d->pm_settings = new QMenu(this);
	connect(d->pm_settings, SIGNAL(aboutToShow()), SLOT(buildMenu()));
	ui_.tb_actions->setMenu(d->pm_settings);

	connect(d->pa->capsManager(), SIGNAL(capsChanged(const Jid&)), SLOT(capsChanged(const Jid&)));
	
	QList<int> list;
	list << 324;
	list << 96;
	ui_.splitter->setSizes(list);

	d->status = -1;
	d->pa->dialogRegister(this, d->jid);

	// Message events
	d->contactChatState = StateNone;
	d->lastChatState = StateNone;
	d->sendComposingEvents = false;
	d->isComposing = false;
	d->composingTimer = 0;
	// SyntaxHighlighters modify the QTextEdit in a QTimer::singleShot(0, ...) call
	// so we need to install our hooks after it fired for the first time
	QTimer::singleShot(10, this, SLOT(initComposing()));
	connect(d, SIGNAL(composing(bool)), SLOT(updateIsComposing(bool)));

	updateContact(d->jid, true);

	d->smallChat = option.smallChats;
	X11WM_CLASS("chat");
	setLooks();

	updatePGP();
	connect(d->pa, SIGNAL(pgpKeyChanged()), SLOT(updatePGP()));
	connect(d->pa, SIGNAL(encryptedMessageSent(int, bool, int)), SLOT(encryptedMessageSent(int, bool, int)));
	ui_.mle->chatEdit()->setFocus();
	resize(PsiOptions::instance()->getOption("options.ui.chat.size").toSize());

	UserListItem *u = d->pa->findFirstRelevant(d->jid);
	if(u && u->isSecure(d->jid.resource())) {
		d->act_pgp->setChecked(true);
	}
	
	connect(d->pa->psi(), SIGNAL(accountCountChanged()), this, SLOT(updateIdentityVisibility()));
}

ChatDlg::~ChatDlg()
{
	d->pa->dialogUnregister(this);

	delete d;
}

void ChatDlg::initComposing()
{
	highlightersInstalled_ = true;
	chatEditCreated();
}

void ChatDlg::setShortcuts()
{
	d->act_clear->setShortcuts(ShortcutManager::instance()->shortcuts("chat.clear"));
	d->act_info->setShortcuts(ShortcutManager::instance()->shortcuts("common.user-info"));
	d->act_history->setShortcuts(ShortcutManager::instance()->shortcuts("common.history"));
	//d->act_send->setShortcuts(ShortcutManager::instance()->shortcuts("chat.send"));
	d->act_scrollup->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-up"));
	d->act_scrolldown->setShortcuts(ShortcutManager::instance()->shortcuts("common.scroll-down"));

	//if(!option.useTabs) {
	//	d->act_close->setShortcuts(ShortcutManager::instance()->shortcuts("common.close"));
	//}
}

void ChatDlg::contextMenuEvent(QContextMenuEvent *)
{
	d->pm_settings->exec(QCursor::pos());
}

void ChatDlg::scrollUp()
{
	ui_.log->verticalScrollBar()->setValue(ui_.log->verticalScrollBar()->value() - ui_.log->verticalScrollBar()->pageStep()/2);
}

void ChatDlg::scrollDown()
{
	ui_.log->verticalScrollBar()->setValue(ui_.log->verticalScrollBar()->value() + ui_.log->verticalScrollBar()->pageStep()/2);
}

// FIXME: This should be unnecessary, since these keys are all registered as
// actions in the constructor. Somehow, Qt ignores this sometimes (i think
// just for actions that have no modifier). 
void ChatDlg::keyPressEvent(QKeyEvent *e)
{
	QKeySequence key = e->key() + ( e->modifiers() & ~Qt::KeypadModifier );
	if(!option.useTabs && ShortcutManager::instance()->shortcuts("common.close").contains(key)) {
		close();
	} else if(ShortcutManager::instance()->shortcuts("chat.send").contains(key)) {
		doSend();
	} else if(ShortcutManager::instance()->shortcuts("common.scroll-up").contains(key)) {
		scrollUp();
	} else if(ShortcutManager::instance()->shortcuts("common.scroll-down").contains(key)) {
		scrollDown();
	} else {
		e->ignore();
	}
}

void ChatDlg::resizeEvent(QResizeEvent *e)
{
	if (option.keepSizes) {
		PsiOptions::instance()->setOption("options.ui.chat.size", e->size());
	}
}

void ChatDlg::closeEvent(QCloseEvent *e)
{
	if (readyToHide()) {
		e->accept();
	} else {
		e->ignore();
	}
}

/**
 * Runs all the gumph necessary before hiding a chat.
 * (checking new messages, setting the autodelete, cancelling composing etc)
 * \return ChatDlg is ready to be hidden.
 */
bool ChatDlg::readyToHide()
{
	// really lame way of checking if we are encrypting
	if(!ui_.mle->chatEdit()->isEnabled()) {
		return false;
	}

	if(d->keepOpen) {
		int n = QMessageBox::information(this, tr("Warning"), tr("A new chat message was just received.\nDo you still want to close the window?"), tr("&Yes"), tr("&No"));
		if(n != 0) {
			return false;
		}
	}

	// destroy the dialog if delChats is dcClose
	if(option.delChats == dcClose) {
		setAttribute(Qt::WA_DeleteOnClose);
	}
	else {
		if(option.delChats == dcHour) {
			setSelfDestruct(60);
		} else if(option.delChats == dcDay) {
			setSelfDestruct(60 * 24);
		}
	}

	// Reset 'contact is composing' & cancel own composing event
	d->resetComposing();
	setChatState(StateGone);
	if (d->contactChatState == StateComposing || d->contactChatState == StateInactive) {
		setContactChatState(StatePaused);
	}

	if(d->pending > 0) {
		d->pending = 0;
		messagesRead(d->jid);
		updateCaption();
	}
	doFlash(false);

	ui_.mle->chatEdit()->setFocus();
	return true;
}

void ChatDlg::capsChanged(const Jid& j)
{
	if (d->jid.compare(j,false)) {
		QString resource = d->jid.resource();
		UserListItem *ul = d->pa->findFirstRelevant(d->jid);
		if (resource.isEmpty() && ul && !ul->userResourceList().isEmpty()) {
			resource = (*(ul->userResourceList().priority())).name();
		}
		d->act_voice->setEnabled(!d->pa->capsManager()->isEnabled() || (ul && ul->isAvailable() && d->pa->capsManager()->features(d->jid.withResource(resource)).canVoice()));
	}
}



void ChatDlg::hideEvent(QHideEvent *)
{
	if (isMinimized()) {
		d->resetComposing();
		setChatState(StateInactive);
	}
}

void ChatDlg::showEvent(QShowEvent *)
{
	setSelfDestruct(0);
}

void ChatDlg::windowActivationChange(bool oldstate)
{
	QWidget::windowActivationChange(oldstate);

	// if we're bringing it to the front, get rid of the '*' if necessary
	if (isActiveTab()) {
		activated();
	}
}

void ChatDlg::logSelectionChanged()
{
#ifdef Q_WS_MAC
	// A hack to only give the message log focus when text is selected
	if (ui_.log->hasSelectedText()) {
		ui_.log->setFocus();
	} else {
		ui_.mle->chatEdit()->setFocus();
	}
#endif
}

void ChatDlg::activated()
{
	if(d->pending > 0) {
		d->pending = 0;
		messagesRead(d->jid);
		updateCaption();
	}
	doFlash(false);

	if(option.showCounter && !d->smallChat) {
		ui_.lb_count->show();
	} else {
		ui_.lb_count->hide();
	}
	ui_.mle->chatEdit()->setFocus();
}


void ChatDlg::dropEvent(QDropEvent* event)
{
	QStringList l;
	if (d->pa->loggedIn() && Q3UriDrag::decodeLocalFiles(event,l) && !l.isEmpty()) {
		d->pa->actionSendFiles(d->jid,l);
	}
}

void ChatDlg::dragEnterEvent(QDragEnterEvent* event)
{
	QStringList l;
	event->accept(d->pa->loggedIn() && Q3UriDrag::canDecode(event) && Q3UriDrag::decodeLocalFiles(event,l) && !l.isEmpty());
}


Jid ChatDlg::jid() const
{
	return d->jid;
}

void ChatDlg::setJid(const Jid &jid)
{
	if(!jid.compare(d->jid)) {
		d->pa->dialogUnregister(this);
		d->jid = jid;
		d->pa->dialogRegister(this, d->jid);
		updateContact(d->jid, false);
	}
}

const QString& ChatDlg::getDisplayName()
{
	return d->dispNick;
}

QSize ChatDlg::defaultSize()
{
	return QSize(320, 280);
}

void ChatDlg::updateContact(const Jid &jid, bool fromPresence)
{
	// if groupchat, only update if the resource matches
	if(d->pa->findGCContact(jid) && !d->jid.compare(jid)) {
		return;
	}

	if(d->jid.compare(jid, false)) {
		QString rname = d->jid.resource();
		QList<UserListItem*> ul = d->pa->findRelevant(jid);
		UserListItem *u = 0;
		int status = -1;
		QString statusString, key;
		if(!ul.isEmpty()) {
			u = ul.first();
			if(rname.isEmpty()) {
				// use priority
				if(!u->isAvailable()) {
					status = STATUS_OFFLINE;
				} else {
					const UserResource &r = *u->userResourceList().priority();
					status = makeSTATUS(r.status());
					statusString = r.status().status();
					key = r.publicKeyID();

					// Chat state
					//d->sendChatState = d->sendChatState || d->pa->capsManager()->features(jid.withResource(r.name())).canChatState();
				}
			}
			else {
				// use specific
				UserResourceList::ConstIterator rit = u->userResourceList().find(rname);
				if(rit != u->userResourceList().end()) {
					status = makeSTATUS((*rit).status());
					statusString = (*rit).status().status();
					key = (*rit).publicKeyID();

					// Chat state
					//d->sendChatState = d->sendChatState || d->pa->capsManager()->features(d->jid).canChatState();
				}
				else {
					status = STATUS_OFFLINE;
					statusString = u->lastUnavailableStatus().status();
					key = "";
					d->contactChatState = StateNone;
				}
			}
		}

		bool statusChanged = false;
		if(d->status != status || d->statusString != statusString) {
			statusChanged = true;
			d->status = status;
			d->statusString = statusString;
		}

		if(statusChanged) {
			if(status == -1 || !u) {
				ui_.lb_status->setPsiIcon(IconsetFactory::iconPtr("status/noauth"));
			} else {
				ui_.lb_status->setPsiIcon(PsiIconset::instance()->statusPtr(jid, status));
			}
		}

		if(u) {
			d->setContactToolTip(u->makeTip(true, false));
		} else {
			d->setContactToolTip(QString());
		}

		if(u) {
			QString name;
			QString j;
			if(rname.isEmpty()) {
				j = JIDUtil::toString(u->jid(),true);
			} else {
				j = JIDUtil::toString(u->jid().userHost(),false) + '/' + rname;
			}

			if(!u->name().isEmpty()) {
				name = u->name() + QString(" <%1>").arg(j);
			} else {
				name = j;
			}

			ui_.le_jid->setText(name);
			ui_.le_jid->setCursorPosition(0);
			ui_.le_jid->setToolTip(name);

			d->dispNick = JIDUtil::nickOrJid(u->name(), u->jid().full());
			updateCaption();

			d->key = key;
			updatePGP();

			if(fromPresence && statusChanged) {
				QString msg = tr("%1 is %2").arg(Qt::escape(d->dispNick)).arg(status2txt(d->status));
				if(!statusString.isEmpty()) {
					QString ss = TextUtil::linkify(TextUtil::plain2rich(statusString));
					if(option.useEmoticons) {
						ss = TextUtil::emoticonify(ss);
					}
					if( PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() ) {
						ss = TextUtil::legacyFormat(ss);
					}
					msg += QString(" [%1]").arg(ss);
				}
				appendSysMsg(msg);
			}
		}

		// Update capabilities
		capsChanged(d->jid);
		
		// Reset 'is composing' event if the status changed
		if (statusChanged && d->contactChatState != StateNone) {
			if (d->contactChatState == StateComposing || d->contactChatState == StateInactive) {
				setContactChatState(StatePaused);
			}
		}
	}
}

void ChatDlg::doVoice()
{
	aVoice(d->jid);
}

void ChatDlg::updateAvatar(const Jid& j)
{
	if (j.compare(d->jid,false)) {
		updateAvatar();
	}
}

void ChatDlg::updateAvatar()
{
	QString res;
	QString client;

	if (!PsiOptions::instance()->getOption("options.ui.chat.avatars.show").toBool()) {
		ui_.avatar->hide();
		return;
	}

	UserListItem *ul = d->pa->findFirstRelevant(d->jid);
	if (ul && !ul->userResourceList().isEmpty()) {
		UserResourceList::Iterator it = ul->userResourceList().find(d->jid.resource());
		if(it == ul->userResourceList().end()) {
			it = ul->userResourceList().priority();
		}

		res = (*it).name();
		client = (*it).clientName();
	}
	//QPixmap p = d->pa->avatarFactory()->getAvatar(d->jid.withResource(res),client);
	QPixmap p = d->pa->avatarFactory()->getAvatar(d->jid.withResource(res));
	if (p.isNull()) {
		ui_.avatar->hide();
	}
	else {
		int size = PsiOptions::instance()->getOption("options.ui.chat.avatars.size").toInt();
		ui_.avatar->setPixmap(p.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		ui_.avatar->show();
	}
}


void ChatDlg::setLooks()
{
	// update the font
	QFont f;
	f.fromString(option.font[fChat]);
	ui_.log->setFont(f);
	ui_.mle->chatEdit()->setFont(f);

	ui_.splitter->optionsChanged();
	ui_.mle->optionsChanged();

	ui_.tb_pgp->hide();
	if (d->smallChat) {
		ui_.lb_status->hide();
		ui_.le_jid->hide();
		ui_.tb_actions->hide();
		ui_.tb_emoticons->hide();
		ui_.toolbar->hide();
	}
	else {
		ui_.lb_status->show();
		ui_.le_jid->show();
		if (PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool()) {
			ui_.toolbar->show();
			ui_.tb_actions->hide();
			ui_.tb_emoticons->hide();
		}
		else {
			ui_.toolbar->hide();
			ui_.tb_emoticons->setVisible(option.useEmoticons);
			ui_.tb_actions->show();
		}
	}
	updateIdentityVisibility();

	if ( option.showCounter && !d->smallChat ) {
		ui_.lb_count->show();
	} else {
		ui_.lb_count->hide();
	}

	// update contact info
	d->status = -2; // sick way of making it redraw the status
	updateContact(d->jid, false);

	// toolbuttons
	QIcon i;
	i.setPixmap(IconsetFactory::icon("psi/cryptoNo").impix(),  QIcon::Automatic, QIcon::Normal, QIcon::Off);
	i.setPixmap(IconsetFactory::icon("psi/cryptoYes").impix(), QIcon::Automatic, QIcon::Normal, QIcon::On);
	d->act_pgp->setPsiIcon( 0 );
	d->act_pgp->setIcon( i );

	// update the widget icon
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/start-chat").icon());
#endif

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);
}

void ChatDlg::optionsUpdate()
{
	if (option.oldSmallChats!=option.smallChats) {
		d->smallChat=option.smallChats;
	}

	setLooks();
	setShortcuts();

	if(isHidden()) {
		if(option.delChats == dcClose) {
			deleteLater();
			return;
		}
		else {
			if(option.delChats == dcHour) {
				setSelfDestruct(60);
			} else if(option.delChats == dcDay) {
				setSelfDestruct(60 * 24);
			} else {
				setSelfDestruct(0);
			}
		}
	}
}

void ChatDlg::updateIdentityVisibility()
{
	if (!d->smallChat) {
		bool visible = d->pa->psi()->contactList()->enabledAccounts().count() > 1;
		ui_.lb_ident->setVisible(visible);
	}
	else 
		ui_.lb_ident->setVisible(false);
}

void ChatDlg::updatePGP()
{
	if(d->pa->hasPGP()) {
		d->act_pgp->setEnabled(true);
	}
	else {
		d->act_pgp->setChecked(false);
		d->act_pgp->setEnabled(false);
	}
	ui_.tb_pgp->setVisible(d->pa->hasPGP() && !d->smallChat && !PsiOptions::instance()->getOption("options.ui.chat.central-toolbar").toBool());
}

void ChatDlg::doInfo()
{
	aInfo(d->jid);
}

void ChatDlg::doHistory()
{
	aHistory(d->jid);
}

void ChatDlg::doFile()
{
	aFile(d->jid);
}

void ChatDlg::doClearButton()
{
	int n = QMessageBox::information(this, tr("Warning"), tr("Are you sure you want to clear the chat window?\n(note: does not affect saved history)"), tr("&Yes"), tr("&No"));
	if(n == 0) {
		doClear();
	}
}

void ChatDlg::doClear()
{
	ui_.log->setText("");
}

void ChatDlg::setKeepOpenFalse()
{
	d->keepOpen = false;
}

void ChatDlg::setWarnSendFalse()
{
	d->warnSend = false;
}

void ChatDlg::setSelfDestruct(int minutes)
{
	if(minutes <= 0) {
		if(d->selfDestruct) {
			delete d->selfDestruct;
			d->selfDestruct = 0;
		}
		return;
	}

	if(!d->selfDestruct) {
		d->selfDestruct = new QTimer(this);
		connect(d->selfDestruct, SIGNAL(timeout()), SLOT(deleteLater()));
	}

	d->selfDestruct->start(minutes * 60000);
}

void ChatDlg::updateCaption()
{
	QString cap = "";

	if(d->pending > 0) {
		cap += "* ";
		if(d->pending > 1) {
			cap += QString("[%1] ").arg(d->pending);
		}
	}
	cap += d->dispNick;

	if (d->contactChatState == StateComposing) {
		cap = tr("%1 (Composing ...)").arg(cap);
	} else if (d->contactChatState == StateInactive) {
		cap = tr("%1 (Inactive)").arg(cap);
	}

	setWindowTitle(cap);

	emit captionChanged(cap);
	emit unreadEventUpdate(d->pending);
}

void ChatDlg::doSend()
{
	if(!ui_.mle->chatEdit()->isEnabled()) {
		return;
	}

	if(ui_.mle->chatEdit()->text().isEmpty()) {
		return;
	}

	if(ui_.mle->chatEdit()->text() == "/clear") {
		ui_.mle->chatEdit()->setText("");
		doClear();
		return;
	}

	if(!d->pa->loggedIn()) {
		return;
	}

	if(d->warnSend) {
		d->warnSend = false;
		int n = QMessageBox::information(this, tr("Warning"), tr(
			"<p>Encryption was recently disabled by the remote contact.  "
			"Are you sure you want to send this message without encryption?</p>"
			), tr("&Yes"), tr("&No"));
		if(n != 0) {
			return;
		}
	}

	Message m(d->jid);
	m.setType("chat");
	m.setBody(ui_.mle->chatEdit()->text());
	m.setTimeStamp(QDateTime::currentDateTime());
	if(d->act_pgp->isChecked()) {
		m.setWasEncrypted(true);
	}
	d->m = m;

 	// Request events
 	if (option.messageEvents) {
		// Only request more events when really necessary
		if (d->sendComposingEvents) {
			m.addEvent(ComposingEvent);
		}
		m.setChatState(StateActive);
 	}

	// Update current state
	setChatState(StateActive);
	
	if(d->act_pgp->isChecked()) {
		ui_.mle->chatEdit()->setEnabled(false);
		d->transid = d->pa->sendMessageEncrypted(m);
		if(d->transid == -1) {
			ui_.mle->chatEdit()->setEnabled(true);
			ui_.mle->chatEdit()->setFocus();
			return;
		}
	}
	else {
		aSend(m);
		doneSend();
	}
}

void ChatDlg::doneSend()
{
	appendMessage(d->m, true);
	disconnect(ui_.mle->chatEdit(), SIGNAL(textChanged()), d, SLOT(setComposing()));
	ui_.mle->chatEdit()->setText("");

	connect(ui_.mle->chatEdit(), SIGNAL(textChanged()), d, SLOT(setComposing()));
	// Reset composing timer
	d->resetComposing();
}

void ChatDlg::encryptedMessageSent(int x, bool b, int e)
{
	if(d->transid == -1) {
		return;
	}
	if(d->transid != x) {
		return;
	}
	d->transid = -1;
	if(b) {
		doneSend();
	} else {
		QMessageBox::critical(this, tr("Error"), tr("There was an error trying to send the message encrypted.\nReason: %1.").arg(PGPUtil::instance().messageErrorString((QCA::SecureMessage::Error) e)));
	}
	ui_.mle->chatEdit()->setEnabled(true);
	ui_.mle->chatEdit()->setFocus();
}

void ChatDlg::incomingMessage(const Message &m)
{
	if (m.body().isEmpty()) {
		/* Event message */
		if (m.containsEvent(CancelEvent)) {
			setContactChatState(StatePaused);
		} else if (m.containsEvent(ComposingEvent)) {
			setContactChatState(StateComposing);
		}
		
		if (m.chatState() != StateNone) {
			setContactChatState(m.chatState());
		}
	}
	else {
		// Normal message
		// Check if user requests event messages
		d->sendComposingEvents = m.containsEvent(ComposingEvent);
		if (!m.eventId().isEmpty()) {
			d->eventId = m.eventId();
		}
		if (m.containsEvents() || m.chatState() != StateNone) {
			setContactChatState(StateActive);
		} else {
			setContactChatState(StateNone);
		}
		appendMessage(m);
	}
}

void ChatDlg::appendSysMsg(const QString &str)
{
	QDateTime t = QDateTime::currentDateTime();
	d->updateLastMsgTime(t);
	QString timestr = ui_.log->formatTimeStamp(t);
	ui_.log->appendText(QString("<font color=\"#00A000\">[%1]").arg(timestr) + QString(" *** %1</font>").arg(str));
}

void ChatDlg::appendMessage(const Message &m, bool local)
{
	d->updateLastMsgTime(m.timeStamp());

	QString who, color;

	if(local) {
		who = d->pa->nick();
		color = "#FF0000";
	}
	else {
		who = d->dispNick;
		color = "#0000FF";
	}
	if(m.spooled()) {
		color = "#008000";
	}

	// figure out the encryption state
	bool encChanged = false;
	bool encEnabled = false;
	if(d->lastWasEncrypted != m.wasEncrypted()) {
		encChanged = true;
	}
	d->lastWasEncrypted = m.wasEncrypted();
	encEnabled = d->lastWasEncrypted;

	if(encChanged) {
		if(encEnabled) {
			appendSysMsg(QString("<icon name=\"psi/cryptoYes\"> ") + tr("Encryption Enabled"));
			if(!local) {
				d->act_pgp->setChecked(true);
			}
		}
		else {
			appendSysMsg(QString("<icon name=\"psi/cryptoNo\"> ") + tr("Encryption Disabled"));
			if(!local) {
				d->act_pgp->setChecked(false);

				// enable warning
				d->warnSend = true;
				QTimer::singleShot(3000, this, SLOT(setWarnSendFalse()));
			}
		}
	}

	QString timestr = ui_.log->formatTimeStamp(m.timeStamp());
	bool emote = false;

	QString me_cmd = "/me ";
	if(m.body().startsWith(me_cmd) || m.html().text().trimmed().startsWith(me_cmd)) {
		emote = true;
	}

	QString txt;

	if(m.containsHTML() && PsiOptions::instance()->getOption("options.html.chat.render").toBool() && !m.html().text().isEmpty()) {
		txt = m.html().toString("span");

		if(emote) {
			int cmd = txt.indexOf(me_cmd);
			txt = txt.remove(cmd, me_cmd.length());
		}
		// qWarning("html body:\n%s\n",qPrintable(txt));
	}
	else {
		txt = m.body();

		if(emote) {
			txt = txt.mid(me_cmd.length());
		}

		txt = TextUtil::plain2rich(txt);
		txt = TextUtil::linkify(txt);
		// qWarning("regular body:\n%s\n",qPrintable(txt));
	}

	if(option.useEmoticons) {
		txt = TextUtil::emoticonify(txt); 
	}
	if( PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() ) {
		txt = TextUtil::legacyFormat(txt);
	}
	who = Qt::escape(who);

	if(emote) {
		ui_.log->appendText(QString("<span style=\"color: %1\">").arg(color) + QString("[%1]").arg(timestr) + QString(" *%1 ").arg(who) + txt + "</span>");
	}
	else {
		if(option.chatSays) {
			ui_.log->appendText(QString("<p style=\"color: %1\">").arg(color) + QString("[%1] ").arg(timestr) + tr("%1 says:").arg(who) + "</p>" + txt);
		} else {
			ui_.log->appendText(QString("<span style=\"color: %1\">").arg(color) + QString("[%1] &lt;").arg(timestr) + who + QString("&gt;</span> ") + txt);
		}
	}
	if(!m.subject().isEmpty()) {
		ui_.log->appendText(QString("<b>") + tr("Subject:") + "</b> " + QString("%1").arg(TextUtil::plain2rich(m.subject())));
	}
	if(!m.urlList().isEmpty()) {
		UrlList urls = m.urlList();
		ui_.log->appendText(QString("<i>") + tr("-- Attached URL(s) --") + "</i>");
		for(QList<Url>::ConstIterator it = urls.begin(); it != urls.end(); ++it) {
			const Url &u = *it;
			ui_.log->appendText(QString("<b>") + tr("URL:") + "</b> " + QString("%1").arg( TextUtil::linkify(Qt::escape(u.url())) ));
			ui_.log->appendText(QString("<b>") + tr("Desc:") + "</b> " + QString("%1").arg(u.desc()));
		}
	}

	if(local) {
		deferredScroll();
	}

	// if we're not active, notify the user by changing the title
	if (!isActiveTab()) {
		++d->pending;
		updateCaption();
		if (PsiOptions::instance()->getOption("options.ui.flash-windows").toBool()) {
			doFlash(true);
		}
		if (option.raiseChatWindow) {
			if (option.useTabs) {
				TabDlg* tabSet = getManagingTabDlg();
				tabSet->selectTab(this);
				bringToFront(tabSet, false);
			}
			else {
				bringToFront(this, false);
			}
		}
	}
	//else {
	//	messagesRead(d->jid);
	//}

	if(!local) {
		d->keepOpen = true;
		QTimer::singleShot(1000, this, SLOT(setKeepOpenFalse()));
	}
}

void ChatDlg::deferredScroll()
{
	QTimer::singleShot(250, this, SLOT(slotScroll()));
}

void ChatDlg::slotScroll()
{
	ui_.log->scrollToBottom();
}

void ChatDlg::updateIsComposing(bool b)
{
	setChatState(b ? StateComposing : StatePaused);
}

void ChatDlg::setChatState(ChatState state)
{
	if (option.messageEvents && (d->sendComposingEvents || (d->contactChatState != StateNone))) {
		// Don't send to offline resource
		QList<UserListItem*> ul = d->pa->findRelevant(d->jid);
		if(ul.isEmpty()) {
			d->sendComposingEvents = false;
			d->lastChatState = StateNone;
			return;
		}

		UserListItem *u = ul.first();
		if(!u->isAvailable()) {
			d->sendComposingEvents = false;
			d->lastChatState = StateNone;
			return;
		}

		// Transform to more privacy-enabled chat states if necessary
		if (!option.inactiveEvents && (state == StateGone || state == StateInactive)) { 
			state = StatePaused;
		}

		if (d->lastChatState == StateNone && (state != StateActive && state != StateComposing && state != StateGone)) {
			//this isn't a valid transition, so don't send it, and don't update laststate
			return;
		}
			
		// Check if we should send a message
		if (state == d->lastChatState || state == StateActive || (d->lastChatState == StateActive && state == StatePaused) ) {
			d->lastChatState = state;
			return;
		}

		// Build event message
		Message m(d->jid);
		if (d->sendComposingEvents) {
			m.setEventId(d->eventId);
			if (state == StateComposing) {
				m.addEvent(ComposingEvent);
			}
			else if (d->lastChatState == StateComposing) {
				m.addEvent(CancelEvent);
			}
		}
		if (d->contactChatState != StateNone) {
			if (d->lastChatState != StateGone) {
				if ((state == StateInactive && d->lastChatState == StateComposing) || (state == StateComposing && d->lastChatState == StateInactive)) {
					// First go to the paused state
					Message tm(d->jid);
					m.setType("chat");
					m.setChatState(StatePaused);
					d->pa->dj_sendMessage(m, false);
				}
				m.setChatState(state);
			}
		}
		
		// Send event message
		if (m.containsEvents() || m.chatState() != StateNone) {
			m.setType("chat");
			d->pa->dj_sendMessage(m, false);
		}

		// Save last state
		if (d->lastChatState != StateGone || state == StateActive) {
			d->lastChatState = state;
		}
	}
}

void ChatDlg::setContactChatState(ChatState state)
{
	d->contactChatState = state;
	if (state == StateGone) {
		appendSysMsg(tr("%1 ended the conversation").arg(Qt::escape(d->dispNick)));
	}
	else {
		// Activate ourselves
		if (d->lastChatState == StateGone) {
			setChatState(StateActive);
		}
	}
	emit contactStateChanged( state );
	updateCaption();
}

void ChatDlg::toggleSmallChat()
{
	d->smallChat = !d->smallChat;
	setLooks();
}

void ChatDlg::toggleEncryption()
{
	d->act_pgp->setChecked( !d->act_pgp->isChecked() );
}

void ChatDlg::buildMenu()
{
	// Dialog menu
	d->pm_settings->clear();
	d->pm_settings->addAction(d->act_compact);
	d->pm_settings->addAction(d->act_clear);
	d->pm_settings->insertSeparator();

	d->pm_settings->addAction(d->act_icon);
	d->pm_settings->addAction(d->act_file);
	if (d->pa->voiceCaller()) {
		d->act_voice->addTo( d->pm_settings );
	}
	d->pm_settings->addAction(d->act_pgp);
	d->pm_settings->insertSeparator();

	d->pm_settings->addAction(d->act_info);
	d->pm_settings->addAction(d->act_history);
}

bool ChatDlg::eventFilter(QObject *obj, QEvent *event)
{
	if (ui_.log->handleCopyEvent(obj, event, ui_.mle->chatEdit())) {
		return true;
	}
	
	return QWidget::eventFilter(obj, event);
}

void ChatDlg::chatEditCreated()
{
	ui_.log->setDialog(this);
	ui_.mle->chatEdit()->setDialog(this);

	connect(ui_.mle->chatEdit(), SIGNAL(textChanged()), d, SLOT(updateCounter()));
	ui_.mle->chatEdit()->installEventFilter(this);
	if (highlightersInstalled_) {
		connect(ui_.mle->chatEdit(), SIGNAL(textChanged()), d, SLOT(setComposing()));
	}
}

#include "chatdlg.moc"
