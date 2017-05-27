/*
 * adduserdlg.cpp - dialog for adding contacts
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "adduserdlg.h"

#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QStringList>
#include <QLineEdit>
#include <QCheckBox>
#include "xmpp_tasks.h"
#include "psiaccount.h"
#include "psiiconset.h"
#include "busywidget.h"
#include "common.h"
#include "iconwidget.h"
#include "tasklist.h"
#include "xmpp_vcard.h"
#include "vcardfactory.h"
#include "infodlg.h"

class AddUserDlg::Private
{
public:
	Private() {}

	PsiAccount *pa;
	BusyWidget *busy;
	QStringList services;
	JT_Gateway *jt;
	TaskList *tasks;
};

AddUserDlg::AddUserDlg(const QStringList &services, const QStringList &names, const QStringList &groups, PsiAccount *pa)
	: QDialog(0)
{
	init(groups, pa);
	d->services = services;


	QStringList::ConstIterator it1 = services.begin();
	QStringList::ConstIterator it2 = names.begin();
	for(; it1 != services.end(); ++it1, ++it2)
		cb_service->addItem(PsiIconset::instance()->status(*it1, STATUS_ONLINE).icon(), *it2);
	connect(cb_service, SIGNAL(activated(int)), SLOT(serviceActivated(int)));

	connect(le_transPrompt, SIGNAL(textChanged(const QString &)), SLOT(le_transPromptChanged(const QString &)));
	pb_transGet->setEnabled(false);

}

AddUserDlg::AddUserDlg(const XMPP::Jid &jid, const QString &nick, const QString &group, const QStringList &groups, PsiAccount *pa)
{
	init(groups, pa);

	le_jid->setText(jid.full());	// TODO: do we want to encourage adding jids with resource?
	le_nick->setText(nick);

	QStringList suggestedGroups = groups.filter(group, Qt::CaseInsensitive);
	if (suggestedGroups.size() > 0) {
		cb_group->lineEdit()->setText(suggestedGroups[0]);
	} else {
		cb_group->lineEdit()->setText(group);
	}

	QSize s(te_info->width(), w_serviceTranslation->sizeHint().height());
	w_serviceTranslation->hide();
	w_serviceTranslation->setEnabled(false);
	te_info->hide();

	resize(size() - s);
}

void AddUserDlg::init(const QStringList &groups, PsiAccount *pa)
{
  	setupUi(this);
	setModal(false);
	setAttribute(Qt::WA_DeleteOnClose);
	d = new Private;
	d->pa = pa;
	d->pa->dialogRegister(this);
	d->jt = 0;
	d->tasks = new TaskList;
	connect(d->tasks, SIGNAL(started()),  busy, SLOT(start()));
	connect(d->tasks, SIGNAL(finished()), busy, SLOT(stop()));

	setWindowTitle(CAP(windowTitle()));
	setWindowIcon(IconsetFactory::icon("psi/addContact").icon());
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);

	d->busy = busy;

	QString str = tr("<None>");
	cb_group->addItem(str);
	QStringList temp=groups;
	temp.sort();
	cb_group->addItems(temp);
	str = tr("Hidden");
	if (!groups.contains(str)) {
		cb_group->addItem(str);
	}
	cb_group->setAutoCompletion(true);

	pb_add->setDefault(true);
	connect(pb_add, SIGNAL(clicked()), SLOT(ok()));
	connect(pb_close, SIGNAL(clicked()), SLOT(cancel()));
	connect(pb_transGet, SIGNAL(clicked()), SLOT(getTransID()));

	connect(tb_vCard, SIGNAL(clicked()), SLOT(getVCardActivated()));
	connect(tb_resolveNick, SIGNAL(clicked()), SLOT(resolveNickActivated()));

	connect(le_jid, SIGNAL(textChanged(QString)), SLOT(jid_Changed()));

	ck_authreq->setChecked(true);
	ck_close->setChecked(true);

	le_jid->setFocus();
}

AddUserDlg::~AddUserDlg()
{
	delete d->tasks;
	d->pa->dialogUnregister(this);
	delete d;
}

Jid AddUserDlg::jid() const
{
	return Jid(le_jid->text().trimmed());
}

void AddUserDlg::cancel()
{
	le_jid->setText("");
	le_nick->setText("");
	cb_group->setCurrentIndex(0);
	reject();
}

void AddUserDlg::ok()
{
	if (!d->pa->checkConnected()) {
		return;
	}

	if(le_jid->text().isEmpty()) {
		QMessageBox::information(this, tr("Add User: Error"), tr("Please fill in the XMPP address of the person you wish to add."));
		return;
	}
	if(!jid().isValid()) {
		QMessageBox::information(this, tr("Add User: Error"), tr("The XMPP address you entered is not valid!\nMake sure you enter a fully qualified XMPP address."));
		return;
	}

	QString gname = cb_group->currentText();
	QStringList list;
	if(gname != tr("<None>")) {
		list += gname;
	}

	add(jid(), le_nick->text(), list, ck_authreq->isChecked());

	QMessageBox::information(this, tr("Add User: Success"), tr("Added %1 to your roster.").arg(jid().full()));
	le_jid->setText("");
	le_nick->setText("");
	if(ck_close->isChecked()) {
		cb_group->setCurrentIndex(0);
		accept();
	} else {
		le_jid->setFocus();
	}
}

void AddUserDlg::serviceActivated(int x)
{
	if(d->jt) {
		delete d->jt;
		d->jt = 0;
		d->busy->stop();
	}
	gb_trans->setEnabled(false);
	le_transPrompt->setText("");

	// XMPP entry
	if(x == 0)
		return;
	--x;

	if(x >= 0 && x < (int)d->services.count()) {
		d->jt = new JT_Gateway(d->pa->client()->rootTask());
		connect(d->jt, SIGNAL(finished()), SLOT(jt_getFinished()));
		d->jt->get(Jid(d->services[x]));
		d->jt->go(true);
		d->tasks->append( d->jt );
	}
}

void AddUserDlg::le_transPromptChanged(const QString &str)
{
	pb_transGet->setEnabled(!str.isEmpty());
}

void AddUserDlg::getTransID()
{
	cb_service->setEnabled(false);
	le_transPrompt->setEnabled(false);
	pb_transGet->setEnabled(false);

	d->jt = new JT_Gateway(d->pa->client()->rootTask());
	connect(d->jt, SIGNAL(finished()), SLOT(jt_setFinished()));
	d->jt->set(Jid(d->services[cb_service->currentIndex()-1]), le_transPrompt->text());
	d->jt->go(true);
	d->tasks->append( d->jt );
}

void AddUserDlg::jt_getFinished()
{
	JT_Gateway *jt = d->jt;
	d->jt = 0;

	if(jt->success()) {
		gb_trans->setEnabled(true);
		lb_transDesc->setText(jt->desc());
	}
	else {
		errorGateway(cb_service->currentText(), jt->statusString());
	}
}

void AddUserDlg::jt_setFinished()
{
	cb_service->setEnabled(true);
	le_transPrompt->setEnabled(true);
	pb_transGet->setEnabled(true);

	JT_Gateway *jt = d->jt;
	d->jt = 0;

	if(jt->success()) {
		QString jid = jt->translatedJid().full();
		if (jid.isEmpty()) {
			jid = jt->prompt();
			// we used to read only prompt() in the past,
			// and many gateways still send it
		}
		le_jid->setText(jid);
		le_nick->setText(le_transPrompt->text());
		le_transPrompt->setText("");
		le_jid->setCursorPosition(0);
		le_nick->setCursorPosition(0);

		le_nick->setFocus();
		le_nick->selectAll();
	}
	else {
		errorGateway(cb_service->currentText(), jt->statusString());
		le_transPrompt->setFocus();
	}
}

void AddUserDlg::errorGateway(const QString &str, const QString &err)
{
	QMessageBox::information(this, CAP(tr("Error")), tr("<qt>\n"
		"There was an error getting the Service ID translation information from \"%1\".<br>"
		"Reason: %2<br>"
		"<br>"
		"The service may not support this feature.  In this case you "
		"will need to enter the XMPP address manually for the contact you wish "
		"to add.  Examples:<br>"
		"<br>"
		"&nbsp;&nbsp;xmppUser@somehost.com<br>"
		"&nbsp;&nbsp;aolUser@[XMPP address of AIM Transport]<br>"
		"&nbsp;&nbsp;1234567@[XMPP address of ICQ Transport]<br>"
		"&nbsp;&nbsp;joe%hotmail.com@[XMPP address of MSN Transport]<br>"
		"&nbsp;&nbsp;yahooUser@[XMPP address of Yahoo Transport]<br>"
		"</qt>"
		).arg(str).arg(QString(err).replace('\n', "<br>")));
}

void AddUserDlg::getVCardActivated()
{
	const VCard vcard = VCardFactory::instance()->vcard(jid());

	InfoDlg *w = new InfoDlg(InfoWidget::Contact, jid(), vcard, d->pa, 0, false);
	w->show();

	// automatically retrieve info if it doesn't exist
	if(!vcard)
		w->infoWidget()->doRefresh();
}

void AddUserDlg::resolveNickActivated()
{
	JT_VCard *jt = VCardFactory::instance()->getVCard(jid(), d->pa->client()->rootTask(), this, SLOT(resolveNickFinished()), false);
	d->tasks->append( jt );
}

void AddUserDlg::resolveNickFinished()
{
	JT_VCard *jt = (JT_VCard *)sender();

	if(jt->success()) {
		QString nickname;
		const XMPP::VCard vcard = jt->vcard();
		if ( !vcard.nickName().isEmpty() ) {
			nickname = vcard.nickName();
		} else if ( !vcard.fullName().isEmpty() ) {
			nickname = vcard.fullName();
		} else {
			nickname = vcard.givenName();
			if ( nickname.isEmpty() ) {
				nickname = vcard.middleName();
			} else if ( !vcard.middleName().isEmpty() ) {
				nickname += " " + vcard.middleName();
			}
			if ( nickname.isEmpty() ) {
				nickname = vcard.familyName();
			} else if ( !vcard.familyName().isEmpty() ) {
				nickname += " " + vcard.familyName();
			}
		}

		if ( nickname.isEmpty() ) {
			nickname = jt->jid().bare();
		}
		le_nick->setText(nickname);
	}
}

/**
 * Called when the Jid changes to enable the vcard and nick resolution buttons.
 */
void AddUserDlg::jid_Changed()
{
	bool enableVCardButtons = jid().isValid();
	tb_vCard->setEnabled(enableVCardButtons);
	tb_resolveNick->setEnabled(enableVCardButtons);
}
