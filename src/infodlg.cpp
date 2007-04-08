/*
 * infodlg.cpp - handle vcard
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

#include <QFileDialog>

#include "infodlg.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qtabwidget.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qbuffer.h>
#include <QPixmap>
#include "xmpp.h"
#include "msgmle.h"
#include "userlist.h"
#include "xmpp_vcard.h"
#include "xmpp_tasks.h"
#include "psiaccount.h"
#include "busywidget.h"
#include "iconset.h"
#include "common.h"
#include "vcardfactory.h"
#include "iconwidget.h"
#include "contactview.h"


using namespace XMPP;
		
class InfoDlg::Private
{
public:
	Private() {}

	int type;
	Jid jid;
	VCard vcard;
	PsiAccount *pa;
	BusyWidget *busy;
	bool te_edited;
	int actionType;
	JT_VCard *jt;
	bool cacheVCard;
	QByteArray photo;
	QList<QString> infoRequested;
};

InfoDlg::InfoDlg(int type, const Jid &j, const VCard &vcard, PsiAccount *pa, QWidget *parent, bool cacheVCard)
:QDialog(parent, Qt::WDestructiveClose)
{
  	if ( option.brushedMetal )
		setAttribute(Qt::WA_MacMetalStyle);
	setupUi(this);
	d = new Private;
	setModal(false);
	d->type = type;
	d->jid = j;
	d->vcard = vcard;
	d->pa = pa;
	d->te_edited = false;
	d->jt = 0;
	d->pa->dialogRegister(this, j);
	d->cacheVCard = cacheVCard;
	d->busy = busy;

	te_desc->setTextFormat(Qt::PlainText);

	setWindowTitle(d->jid.full());
#ifndef Q_WS_MAC
	setWindowIcon(IconsetFactory::icon("psi/vCard"));
#endif

	connect(pb_refresh, SIGNAL(clicked()), this, SLOT(doRefresh()));
	connect(pb_refresh, SIGNAL(clicked()), this, SLOT(updateStatus()));
	connect(te_desc, SIGNAL(textChanged()), this, SLOT(textChanged()));
	connect(pb_open, SIGNAL(clicked()), this, SLOT(selectPhoto()));
	connect(pb_clear, SIGNAL(clicked()), this, SLOT(clearPhoto()));
	connect(pb_close, SIGNAL(clicked()), this, SLOT(close()));

	if(d->type == Self) {
		connect(pb_submit, SIGNAL(clicked()), this, SLOT(doSubmit()));
	}
	else {
		// Hide buttons
		pb_submit->hide();
		pb_open->hide();
		pb_clear->hide();
		setReadOnly(true);
	}

	// Add a status tab
	connect(d->pa->client(), SIGNAL(resourceAvailable(const Jid &, const Resource &)), SLOT(contactAvailable(const Jid &, const Resource &)));
	connect(d->pa->client(), SIGNAL(resourceUnavailable(const Jid &, const Resource &)), SLOT(contactUnavailable(const Jid &, const Resource &)));
	connect(d->pa,SIGNAL(updateContact(const Jid&)),SLOT(contactUpdated(const Jid&)));
	te_status->setReadOnly(true);
	te_status->setTextFormat(Qt::RichText);
	updateStatus();
	foreach(UserListItem* u, d->pa->findRelevant(j)) {
		foreach(UserResource r, u->userResourceList()) {
			requestClientVersion(d->jid.withResource(r.name()));
		}
	}

	setData(d->vcard);
}

InfoDlg::~InfoDlg()
{
	d->pa->dialogUnregister(this);
	delete d;
}

/**
 * Redefined so the window does not close when changes are not saved. 
 */
void InfoDlg::closeEvent ( QCloseEvent * e ) {

	// don't close if submitting
	if(d->busy->isActive() && d->actionType == 1) {
		e->ignore();
		return;
	}

	if(d->type == Self && edited()) {
		int n = QMessageBox::information(this, tr("Warning"), tr("You have not published your account information changes.\nAre you sure you want to discard them?"), tr("Close and discard"), tr("Don't close"));
		if(n != 0) {
			e->ignore();
			return;
		}
	}

	// cancel active transaction (refresh only)
	if(d->busy->isActive() && d->actionType == 0) {
		delete d->jt;
		d->jt = 0;
	}

	e->accept();
}

void InfoDlg::jt_finished()
{
	d->busy->stop();
	pb_refresh->setEnabled(true);
	pb_submit->setEnabled(true);
	pb_close->setEnabled(true);
	fieldsEnable(true);

	if(d->jt->success()) {
		if(d->actionType == 0) {
			d->vcard = d->jt->vcard();
			setData(d->vcard);
		}
		else if(d->actionType == 1) {
			d->vcard = d->jt->vcard();
			if ( d->cacheVCard )
				VCardFactory::instance()->setVCard(d->jid, d->vcard);
			setData(d->vcard);
		}

		if(d->jid.compare(d->pa->jid(), false)) {
			if (!d->vcard.nickName().isEmpty())
				d->pa->setNick(d->vcard.nickName());
			else
				d->pa->setNick(d->pa->jid().user());
		}

		if(d->actionType == 1)
			QMessageBox::information(this, tr("Success"), tr("Your account information has been published."));
	}
	else {
		if(d->actionType == 0) {
			if(d->type == Self)
				QMessageBox::critical(this, tr("Error"), tr("Unable to retrieve your account information.  Perhaps you haven't entered any yet."));
			else
				QMessageBox::critical(this, tr("Error"), tr("Unable to retrieve information about this contact.\nReason: %1").arg(d->jt->statusString()));
		}
		else {
			QMessageBox::critical(this, tr("Error"), tr("Unable to publish your account information.\nReason: %1").arg(d->jt->statusString()));
		}
	}

	d->jt = 0;
}

void InfoDlg::setData(const VCard &i)
{
	le_fullname->setText( i.fullName() );
	le_nickname->setText( i.nickName() );
	le_bday->setText( i.bdayStr() );

	QString email;
	if ( !i.emailList().isEmpty() )
		email = i.emailList()[0].userid;
	le_email->setText( email );

	le_homepage->setText( i.url() );

	QString phone;
	if ( !i.phoneList().isEmpty() )
		phone = i.phoneList()[0].number;
	le_phone->setText( phone );

	VCard::Address addr;
	if ( !i.addressList().isEmpty() )
		addr = i.addressList()[0];
	le_street->setText( addr.street );
	le_ext->setText( addr.extaddr );
	le_city->setText( addr.locality );
	le_state->setText( addr.region );
	le_pcode->setText( addr.pcode );
	le_country->setText( addr.country );

	le_orgName->setText( i.org().name );

	QString unit;
	if ( !i.org().unit.isEmpty() )
		unit = i.org().unit[0];
	le_orgUnit->setText( unit );

	le_title->setText( i.title() );
	le_role->setText( i.role() );
	te_desc->setText( i.desc() );
	
	if ( !i.photo().isEmpty() ) {
		//printf("There is a picture...\n");
		d->photo = i.photo();
		updatePhoto();
	}
	else
		clearPhoto();

	setEdited(false);
}

void InfoDlg::updatePhoto() 
{
	int max_width = label_photo->width() - 20; // FIXME: Ugly magic number
	int max_height = label_photo->height() - 20; // FIXME: Ugly magic number
	
	QImage img(d->photo);
	QImage img_scaled;
	if (img.width() > max_width || img.height() > max_height) {
		img_scaled = img.scaled(max_width, max_height,Qt::KeepAspectRatio);
	}
	else {
		img_scaled = img;
	}
	label_photo->setPixmap(QPixmap(img_scaled));
}

void InfoDlg::fieldsEnable(bool x)
{
	le_fullname->setEnabled(x);
	le_nickname->setEnabled(x);
	le_bday->setEnabled(x);
	le_email->setEnabled(x);
	le_homepage->setEnabled(x);
	le_phone->setEnabled(x);
	pb_open->setEnabled(x);
	pb_clear->setEnabled(x);

	le_street->setEnabled(x);
	le_ext->setEnabled(x);
	le_city->setEnabled(x);
	le_state->setEnabled(x);
	le_pcode->setEnabled(x);
	le_country->setEnabled(x);

	le_orgName->setEnabled(x);
	le_orgUnit->setEnabled(x);
	le_title->setEnabled(x);
	le_role->setEnabled(x);
	te_desc->setEnabled(x);

	setEdited(false);
}

void InfoDlg::setEdited(bool x)
{
	le_fullname->setEdited(x);
	le_nickname->setEdited(x);
	le_bday->setEdited(x);
	le_email->setEdited(x);
	le_homepage->setEdited(x);
	le_phone->setEdited(x);
	le_street->setEdited(x);
	le_ext->setEdited(x);
	le_city->setEdited(x);
	le_state->setEdited(x);
	le_pcode->setEdited(x);
	le_country->setEdited(x);
	le_orgName->setEdited(x);
	le_orgUnit->setEdited(x);
	le_title->setEdited(x);
	le_role->setEdited(x);

	d->te_edited = x;
}

bool InfoDlg::edited()
{
	bool x = false;

	if(le_fullname->edited()) x = true;
	if(le_nickname->edited()) x = true;
	if(le_bday->edited()) x = true;
	if(le_email->edited()) x = true;
	if(le_homepage->edited()) x = true;
	if(le_phone->edited()) x = true;
	if(le_street->edited()) x = true;
	if(le_ext->edited()) x = true;
	if(le_city->edited()) x = true;
	if(le_state->edited()) x = true;
	if(le_pcode->edited()) x = true;
	if(le_country->edited()) x = true;
	if(le_orgName->edited()) x = true;
	if(le_orgUnit->edited()) x = true;
	if(le_title->edited()) x = true;
	if(le_role->edited()) x = true;
	if(d->te_edited) x = true;

	return x;
}

void InfoDlg::setReadOnly(bool x)
{
	le_fullname->setReadOnly(x);
	le_nickname->setReadOnly(x);
	le_bday->setReadOnly(x);
	le_email->setReadOnly(x);
	le_homepage->setReadOnly(x);
	le_phone->setReadOnly(x);
	le_street->setReadOnly(x);
	le_ext->setReadOnly(x);
	le_city->setReadOnly(x);
	le_state->setReadOnly(x);
	le_pcode->setReadOnly(x);
	le_country->setReadOnly(x);
	le_orgName->setReadOnly(x);
	le_orgUnit->setReadOnly(x);
	le_title->setReadOnly(x);
	le_role->setReadOnly(x);
	te_desc->setReadOnly(x);
}

void InfoDlg::doRefresh()
{
	if(!d->pa->checkConnected(this))
		return;
	if(!pb_refresh->isEnabled())
		return;
	if(d->busy->isActive())
		return;

	pb_submit->setEnabled(false);
	pb_refresh->setEnabled(false);
	fieldsEnable(false);

	d->actionType = 0;
	d->busy->start();

	d->jt = VCardFactory::instance()->getVCard(d->jid, d->pa->client()->rootTask(), this, SLOT(jt_finished()), d->cacheVCard);
}

void InfoDlg::doSubmit()
{
	if(!d->pa->checkConnected(this))
		return;
	if(!pb_submit->isEnabled())
		return;
	if(d->busy->isActive())
		return;

	VCard submit_vcard = makeVCard();

	pb_submit->setEnabled(false);
	pb_refresh->setEnabled(false);
	pb_close->setEnabled(false);
	fieldsEnable(false);

	d->actionType = 1;
	d->busy->start();

	d->jt = new JT_VCard(d->pa->client()->rootTask());
	connect(d->jt, SIGNAL(finished()), SLOT(jt_finished()));
	d->jt->set(submit_vcard);
	d->jt->go(true);
}

VCard InfoDlg::makeVCard()
{
	VCard v;

	v.setFullName( le_fullname->text() );
	v.setNickName( le_nickname->text() );
	v.setBdayStr( le_bday->text() );

	if ( !le_email->text().isEmpty() ) {
		VCard::Email email;
		email.internet = true;
		email.userid = le_email->text();

		VCard::EmailList list;
		list << email;
		v.setEmailList( list );
	}

	v.setUrl( le_homepage->text() );

	if ( !le_phone->text().isEmpty() ) {
		VCard::Phone p;
		p.home = true;
		p.voice = true;
		p.number = le_phone->text();

		VCard::PhoneList list;
		list << p;
		v.setPhoneList( list );
	}
	
	if ( !d->photo.isEmpty() ) {
		//printf("Adding a pixmap to the vCard...\n");
		v.setPhoto( d->photo );
	}

	if ( !le_street->text().isEmpty() ||
	     !le_ext->text().isEmpty()    ||
	     !le_city->text().isEmpty()   ||
	     !le_state->text().isEmpty()  ||
	     !le_pcode->text().isEmpty()  ||
	     !le_country->text().isEmpty() )
	{
		VCard::Address addr;
		addr.home = true;
		addr.street = le_street->text();
		addr.extaddr = le_ext->text();
		addr.locality = le_city->text();
		addr.region = le_state->text();
		addr.pcode = le_pcode->text();
		addr.country = le_country->text();

		VCard::AddressList list;
		list << addr;
		v.setAddressList( list );
	}

	VCard::Org org;

	org.name = le_orgName->text();

	if ( !le_orgUnit->text().isEmpty() )
		org.unit << le_orgUnit->text();

	v.setOrg( org );

	v.setTitle( le_title->text() );
	v.setRole( le_role->text() );
	v.setDesc( te_desc->text() );

	return v;
}

void InfoDlg::textChanged()
{
	d->te_edited = true;
}

/**
 * Opens a file browser dialog, and if selected, calls the setPreviewPhoto with the consecuent path.
 * \see setPreviewPhoto(const QString& path)
*/
void InfoDlg::selectPhoto()
{
	while(1) {
		if(option.lastPath.isEmpty())
			option.lastPath = QDir::homeDirPath();
		QString str = QFileDialog::getOpenFileName(this, tr("Choose a file"), option.lastPath, tr("Images (*.png *.xpm *.jpg *.PNG *.XPM *.JPG)"));
		if(!str.isEmpty()) {
			QFileInfo fi(str);
			if(!fi.exists()) {
				QMessageBox::information(this, tr("Error"), tr("The file specified does not exist."));
				continue;
			}
			option.lastPath = fi.dirPath();
			//printf(QDir::convertSeparators(fi.filePath()));
			
			// put the image in the preview box
			setPreviewPhoto(str);
		}
		break;
	}
	
}

/**
 * Loads the image from the requested URL, and inserts the resized image into the preview box.
 * \param path image file to load
*/
void InfoDlg::setPreviewPhoto(const QString& path)
{
	QFile photo_file(path);
	if (!photo_file.open(QIODevice::ReadOnly))
		return;
	
	QByteArray photo_data = photo_file.readAll();
	QImage photo_image(photo_data);
	if(!photo_image.isNull()) {
		d->photo = photo_data;
		updatePhoto();
		d->te_edited = true;
	}
}

/**
 * Clears the preview image box and marks the te_edited signal in the private.
*/
void InfoDlg::clearPhoto()
{
	// this will cause the pixmap disappear
	label_photo->setText(tr("Picture not\navailable"));
	d->photo = QByteArray();
	
	// the picture changed, so notify there are some changes done
	d->te_edited = true;
}

/**
 * Updates the status info of the contact
 */
void InfoDlg::updateStatus()
{
	UserListItem *u = d->pa->find(d->jid);
	if(u) {
		te_status->setText(u->makeDesc());
	}
	else {
		te_status->clear();
	}
}

/**
 * Sets the visibility of the status tab
 */
void InfoDlg::setStatusVisibility(bool visible)
{
	// Add/remove tab if necessary
	int index = tabwidget->indexOf(tab_status);
	if(index==-1) {
		if(visible)
			tabwidget->addTab(tab_status,tr("Status"));
	}
	else if(!visible) {
		tabwidget->removeTab(index);
	}
}

void InfoDlg::requestClientVersion(const Jid& j)
{
	d->infoRequested += j.full();
	JT_ClientVersion *jcv = new JT_ClientVersion(d->pa->client()->rootTask());
	connect(jcv, SIGNAL(finished()), SLOT(clientVersionFinished()));
	jcv->get(j);
	jcv->go(true);
}

void InfoDlg::clientVersionFinished()
{
	JT_ClientVersion *j = (JT_ClientVersion *)sender();
	if(j->success()) {
		foreach(UserListItem* u, d->pa->findRelevant(j->jid())) {
			UserResourceList::Iterator rit = u->userResourceList().find(j->jid().resource());
			bool found = (rit == u->userResourceList().end()) ? false: true;
			if(!found)
				continue;

			(*rit).setClient(j->name(),j->version(),j->os());
			d->pa->contactProfile()->updateEntry(*u);
			updateStatus();
		}
	}
}

void InfoDlg::contactAvailable(const Jid &j, const Resource &r)
{
	if (d->jid.compare(j,false)) {
		if (!d->infoRequested.contains(j.withResource(r.name()).full()))
			requestClientVersion(j.withResource(r.name()));
	}
}

void InfoDlg::contactUnavailable(const Jid &j, const Resource &r)
{
	if (d->jid.compare(j,false)) {
		d->infoRequested.remove(j.withResource(r.name()).full());
	}
}

void InfoDlg::contactUpdated(const XMPP::Jid & j)
{
	if (d->jid.compare(j,false)) {
		updateStatus();
	}
}
