/*
 * psipopup.cpp - the Psi passive popup class
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psipopup.h"
#include "fancypopup.h"
#include "fancylabel.h"
#include "userlist.h"
#include "alerticon.h"
#include "psievent.h"
#include "psicon.h"
#include "textutil.h"
#include "psiaccount.h"
#include "psiiconset.h"
#include "iconlabel.h"
#include "psioptions.h"
#include "coloropt.h"
#include "avatars.h"

#include <QApplication>
#include <QLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QList>
#include <QTextDocument>
#include <QtPlugin>

/**
 * Limits number of popups that could be displayed
 * simultaneously on screen. Old popups momentally
 * disappear when new ones appear.
 */
static int MaxPopups = 5;

/**
 * Holds a list of Psi Popups.
 */
static QList<PsiPopup *> *psiPopupList = 0;

//----------------------------------------------------------------------------
// PsiPopup::Private
//----------------------------------------------------------------------------

class PsiPopup::Private : public QObject
{
	Q_OBJECT
public:
	Private(PsiPopup *p);
	~Private();

	void init(const PsiIcon *titleIcon, const QString& titleText, PsiAccount *_acc);
	QBoxLayout *createContactInfo(const QPixmap *avatar, const PsiIcon *icon, const QString& text);

private slots:
	void popupDestroyed();
	void popupClicked(int);

public:
	PsiCon *psi;
	PsiAccount *account;
	FancyPopup *popup;
	PsiPopup *psiPopup;
	QString id;
	PopupManager::PopupType popupType;
	Jid jid;
	Status status;
	PsiEvent::Ptr event;
	PsiIcon *titleIcon;
	bool display;
	bool doAlertIcon;
};

PsiPopup::Private::Private(PsiPopup *p)
	: psi(0)
	, account(0)
	, popup(0)
	, psiPopup(p)
	, popupType(PopupManager::AlertNone)
	, titleIcon(0)
	, doAlertIcon(false)
{
}

PsiPopup::Private::~Private()
{
	if ( psiPopupList )
		psiPopupList->removeAll(psiPopup);

	if ( popup )
		delete popup;
	if ( titleIcon )
		delete titleIcon;
	popup = 0;
}

void PsiPopup::Private::init(const PsiIcon *_titleIcon, const QString& titleText, PsiAccount *acc)
{
	if(acc)
		psi = acc->psi();
	account = acc;
	display = true;

	if ( !psiPopupList )
		psiPopupList = new QList<PsiPopup *>();

	if ( psiPopupList->count() >= MaxPopups && MaxPopups > 0 )
		delete psiPopupList->first();

	FancyPopup *lastPopup = 0;
	if ( psiPopupList->count() && psiPopupList->last() )
		lastPopup = psiPopupList->last()->popup();

	if ( doAlertIcon )
		titleIcon = new AlertIcon(_titleIcon);
	else if(_titleIcon)
		titleIcon = new PsiIcon(*_titleIcon);

	FancyPopup::setHideTimeout(psiPopup->duration());
	FancyPopup::setBorderColor(ColorOpt::instance()->color("options.ui.look.colors.passive-popup.border"));

	popup = new FancyPopup(titleText, titleIcon, lastPopup, false);
	connect(popup, SIGNAL(clicked(int)), SLOT(popupClicked(int)));
	connect(popup, SIGNAL(destroyed()), SLOT(popupDestroyed()));

	// create id
	if ( _titleIcon )
		id += _titleIcon->name();
	id += titleText;
}

void PsiPopup::Private::popupDestroyed()
{
	popup = 0;
	psiPopup->deleteLater();
}

void PsiPopup::Private::popupClicked(int button)
{
	if ( button == (int)Qt::LeftButton ) {
		if ( event )
			psi->processEvent(event, UserAction);
		else if ( account ) {
			// FIXME: it should work in most cases, but
			// maybe it's better to fix UserList::find()?
			Jid j( jid.bare() );
			account->actionDefault( j );
		}
	}
	else if ( event && event->account() && button == (int)Qt::RightButton ) {
		event->account()->psi()->removeEvent(event);
	}

	popup->deleteLater();
}

QBoxLayout *PsiPopup::Private::createContactInfo(const QPixmap *avatar, const PsiIcon *icon, const QString& text)
{
	QHBoxLayout *dataBox = new QHBoxLayout();


	if (avatar && !avatar->isNull()) {
		int size = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.avatar-size").toInt();
		QLabel *avatarLabel = new QLabel(popup);
		avatarLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
		avatarLabel->setPixmap(avatar->scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		dataBox->addWidget(avatarLabel);
		dataBox->addSpacing(5);
	}

	if (icon) {
		IconLabel *iconLabel = new IconLabel(popup);
		iconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
		iconLabel->setPsiIcon(icon);
		dataBox->addWidget(iconLabel);

		dataBox->addSpacing(5);
	}

	QLabel *textLabel = new QLabel(popup);
	QFont font;
	font.fromString( PsiOptions::instance()->getOption("options.ui.look.font.passive-popup").toString() );
	textLabel->setFont(font);

	textLabel->setWordWrap(false);
	textLabel->setText(QString("<qt>%1</qt>").arg(clipText(text)));
	textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	dataBox->addWidget(textLabel);

	return dataBox;
}

//----------------------------------------------------------------------------
// PsiPopup
//----------------------------------------------------------------------------

PsiPopup::PsiPopup(QObject *parent)
	:QObject(parent)
{
	d = new Private(this);
}

PsiPopup::~PsiPopup()
{
	delete d;
}

void PsiPopup::popup(PsiAccount *acc, PopupManager::PopupType type, const Jid &j, const Resource &r, const UserListItem *item, const PsiEvent::Ptr &event)
{
	d->popupType = type;
	PsiIcon *icon = 0;
	QString text = title(type, &d->doAlertIcon, &icon);
	d->init(icon, text, acc);
	setData(j, r, item, event);
}

void PsiPopup::popup(PsiAccount *acc, PopupManager::PopupType type, const Jid &j, const PsiIcon *titleIcon, const QString &titleText,
		     const QPixmap *avatar, const PsiIcon *icon, const QString &text)
{
	d->popupType = type;
	d->init(titleIcon, titleText, acc);
	setJid(j);
	setData(avatar, icon, text);
}

void PsiPopup::setJid(const Jid &j)
{
	d->jid = j;
}

void PsiPopup::setData(const QPixmap *avatar, const PsiIcon *icon, const QString& text)
{
	if ( !d->popup ) {
		deleteLater();
		return;
	}

	d->popup->addLayout( d->createContactInfo(avatar, icon, text) );

	// update id
	if ( icon )
		d->id += icon->name();
	d->id += text;

	show();
}

void PsiPopup::setData(const Jid &j, const Resource &r, const UserListItem *u, const PsiEvent::Ptr &event)
{
	if ( !d->popup ) {
		deleteLater();
		return;
	}

	d->jid    = j;
	d->status = r.status();
	if(d->popupType != PopupManager::AlertComposing)
		d->event  = event;

	PsiIcon *icon = PsiIconset::instance()->statusPtr(j, r.status());

	QString jid = j.full();
	int jidLen = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.maximum-jid-length").toInt();
	if (jidLen > 0 && ((int)jid.length()) > jidLen)
		jid = jid.left(jidLen) + "...";

	QString status;
	int len = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.maximum-status-length").toInt();
	if (len != 0)
		status = r.status().status();
	if (len > 0)
		if ( ((int)status.length()) > len )
			status = status.left (len) + "...";

	QString name;
	if ( u && !u->name().isEmpty() ) {
		name = u->name();
	}
	else if (event && event->type() == PsiEvent::Auth) {
		name = event.staticCast<AuthEvent>()->nick();
	}
	else if (event && event->type() == PsiEvent::Message) {
		name = event.staticCast<MessageEvent>()->nick();
	}

	if (!name.isEmpty()) {
		if (!jidLen)
			name = "<nobr>" + TextUtil::escape(name) + "</nobr>";
		else
			name = "<nobr>" + TextUtil::escape(name) + " &lt;" + TextUtil::escape(jid) + "&gt;" + "</nobr>";
	}
	else
		name = "<nobr>&lt;" + TextUtil::escape(jid) + "&gt;</nobr>";

	QString statusString = TextUtil::plain2rich(status);
	if ( PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool() )
		statusString = TextUtil::emoticonify(statusString);
	if( PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool() )
		statusString = TextUtil::legacyFormat(statusString);

	if ( !statusString.isEmpty() )
		statusString = "<br>" + statusString;

	QString contactText = "<font size=\"+1\">" + name + "</font>" + statusString;

	// hack for duplicate "Contact Online"/"Status Change" popups
	foreach (PsiPopup *pp, *psiPopupList) {
		if ( d->jid.full() == pp->d->jid.full() && d->status.show() == pp->d->status.show() && d->status.status() == d->status.status() ) {
			if ( d->popupType == PopupManager::AlertStatusChange && pp->d->popupType == PopupManager::AlertOnline ) {
				d->display = false;
				deleteLater();
				break;
			}
		}
	}
	QPixmap avatar;
	if(d->account) {
		if(u && u->isPrivate())
			avatar = d->account->avatarFactory()->getMucAvatar(j);
		else
			avatar = d->account->avatarFactory()->getAvatar(j);
	}
	// show popup
	if ( d->popupType != PopupManager::AlertComposing && d->popupType != PopupManager::AlertHeadline &&
	     (d->popupType != PopupManager::AlertFile || !PsiOptions::instance()->getOption("options.ui.file-transfer.auto-popup").toBool()) )
	{
		if ((event && event->type() == PsiEvent::Message) && (PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.showMessage").toBool())) {
			const Message *jmessage = &event.staticCast<MessageEvent>()->message();
			QString message;

			if ( !jmessage->subject().isEmpty() )
				message += "<font color=\"red\"><b>" + tr("Subject:") + " " + jmessage->subject() + "</b></font><br>";
			message += TextUtil::plain2rich( jmessage->body() );

			if (!message.isEmpty()) {
				contactText += "<br/><font size=\"+1\">" + message + "</font>";
			}
		}
		setData(&avatar, icon, contactText);
	}
	else if ( d->popupType == PopupManager::AlertComposing ) {
		QString txt = "<font size=\"+1\">" + name + tr(" is typing...") + "</font>" ;
		setData(&avatar, icon, txt);
	}
	else if ( d->popupType == PopupManager::AlertHeadline ) {
		QVBoxLayout *vbox = new QVBoxLayout;
		vbox->addLayout( d->createContactInfo(&avatar, icon, contactText) );

		vbox->addSpacing(5);

		const Message *jmessage = &event.staticCast<MessageEvent>()->message();
		QString message;
		if ( !jmessage->subject().isEmpty() )
			message += "<font color=\"red\"><b>" + tr("Subject:") + ' ' + jmessage->subject() + "</b></font><br>";
		message += TextUtil::plain2rich( jmessage->body() );

		QLabel *messageLabel = new QLabel(d->popup);
		QFont font = messageLabel->font();
		font.setPointSize(common_smallFontSize);
		messageLabel->setFont(font);

		messageLabel->setWordWrap(true);
		messageLabel->setTextFormat(Qt::RichText);
		messageLabel->setText( clipText(TextUtil::linkify( message )) );
		messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		vbox->addWidget(messageLabel);

		// update id
		if ( icon )
			d->id += icon->name();
		d->id += contactText;
		d->id += message;

		d->popup->addLayout( vbox );
		show();
	}
	else {
		deleteLater();
	}
}

void PsiPopup::show()
{
	if ( !d->popup ) {
		deleteLater();
		return;
	}

	if ( !d->id.isEmpty() /*&& LEGOPTS.ppNoDupes*/ ) {
		foreach (PsiPopup *pp, *psiPopupList) {
			if ( d->id == pp->id() && pp->popup() ) {
				pp->popup()->restartHideTimer();

				d->display = false;
				break;
			}
		}
	}

	if ( d->display ) {
		psiPopupList->append( this );
		d->popup->show();
	}
	else {
		deleteLater();
	}
}

QString PsiPopup::id() const
{
	return d->id;
}

FancyPopup *PsiPopup::popup() const
{
	return d->popup;
}

void PsiPopup::deleteAll()
{
	if ( !psiPopupList )
		return;

	psiPopupList->clear();
	delete psiPopupList;
	psiPopupList = 0;
}

#include "psipopup.moc"

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN2(psipopup, PsiPopupPlugin)
#endif
