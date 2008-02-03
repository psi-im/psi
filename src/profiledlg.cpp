/*
 * profiledlg.cpp - dialogs for manipulating profiles
 * Copyright (C) 2001-2003  Justin Karneges
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

#include "profiledlg.h"
#include "applicationinfo.h"
#include "iconset.h"
#include "psioptions.h"

#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QPushButton>
#include <Q3Button>
#include <Q3GroupBox>
#include <Q3ListBox>
#include <QInputDialog>
#include <QFile>
#include <QFileInfo>
#include <Q3ButtonGroup>
#include <QPixmap>
#include "profiles.h"
#include "common.h"
#include "iconwidget.h"

#include <qpainter.h>
class StretchLogoLabel : public QLabel
{
public:
	StretchLogoLabel(QPixmap pix, QWidget *label, const char *name = 0)
	: QLabel((QWidget*)label->parent(), name)
	{
		setPixmap(pix);
		setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
		replaceWidget(label, this);
	}

	void paintEvent(QPaintEvent *event)
	{
		QPainter *p = new QPainter(this);
		p->drawTiledPixmap(0, 0, width(), height(), *pixmap());
		delete p;
		QLabel::paintEvent(event);
	}
};

ProfileOpenDlg::ProfileOpenDlg(const QString &def, const VarList &_langs, const QString &curLang, QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(true);
	setWindowTitle(CAP(caption()));

	langs = _langs;

	// insert the logo
	QPixmap logo = (QPixmap)IconsetFactory::icon("psi/psiLogo").pixmap();
	lb_logo->setPixmap(logo);
	lb_logo->setFixedSize(logo.width(), logo.height());
	lb_logo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	//setFixedWidth(logo->width());

	QImage logoImg = logo.convertToImage();

	QPixmap tmp;
	tmp.convertFromImage( logoImg.copy(0, 0, 1, logoImg.height()) );
	StretchLogoLabel *stretch = new StretchLogoLabel(tmp, lb_left);

	tmp.convertFromImage( logoImg.copy(logoImg.width()-1, 0, 1, logoImg.height()) );
	stretch = new StretchLogoLabel(tmp, lb_right);

	// setup signals
	connect(pb_open, SIGNAL(clicked()), SLOT(accept()));
	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	connect(pb_profiles, SIGNAL(clicked()), SLOT(manageProfiles()));
	connect(cb_lang, SIGNAL(activated(int)), SLOT(langChange(int)));

	int x = 0;
	langSel = x;
	for(VarList::ConstIterator it = langs.begin(); it != langs.end(); ++it) {
		cb_lang->insertItem((*it).data());
		if((curLang.isEmpty() && x == 0) || (curLang == (*it).key())) {
			cb_lang->setCurrentItem(x);
			langSel = x;
		}
		++x;
	}

	// QWhatsThis helpers
	cb_profile->setWhatsThis(
		tr("Select a profile to open from this list."));
	cb_lang->setWhatsThis(
		tr("Select a language you would like Psi to use from this "
		"list.  You can download extra language packs from the Psi homepage."));
	ck_auto->setWhatsThis(
		tr("Automatically open this profile when Psi is started.  Useful if "
		"you only have one profile."));

	reload(def);
}

ProfileOpenDlg::~ProfileOpenDlg()
{
}

void ProfileOpenDlg::reload(const QString &choose)
{
	QStringList list = getProfilesList();

	cb_profile->clear();

	if(list.count() == 0) {
		gb_open->setEnabled(FALSE);
		pb_open->setEnabled(FALSE);
		pb_profiles->setFocus();
	}
	else {
		int x = 0;
		for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
			cb_profile->insertItem(*it);
			if((choose.isEmpty() && x == 0) || (choose == *it)) {
				cb_profile->setCurrentItem(x);
			}
			++x;
		}

		gb_open->setEnabled(TRUE);
		pb_open->setEnabled(TRUE);
		pb_open->setFocus();
	}
}

void ProfileOpenDlg::manageProfiles()
{
	ProfileManageDlg *w = new ProfileManageDlg(cb_profile->currentText(), this);
	w->exec();
	QString last = w->lbx_profiles->text(w->lbx_profiles->currentItem());
	delete w;

	reload(last);
}

void ProfileOpenDlg::langChange(int x)
{
	if(x == langSel)
		return;
	langSel = x;

	VarList::Iterator it = langs.findByNum(x);
	newLang = (*it).key();
	done(10);
}


ProfileManageDlg::ProfileManageDlg(const QString &choose, QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(true);
	setWindowTitle(CAP(caption()));

	// setup signals
	connect(pb_new, SIGNAL(clicked()), SLOT(slotProfileNew()));
	connect(pb_rename, SIGNAL(clicked()), SLOT(slotProfileRename()));
	connect(pb_delete, SIGNAL(clicked()), SLOT(slotProfileDelete()));
	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	connect(lbx_profiles, SIGNAL(highlighted(int)), SLOT(updateSelection()));

	// load the listing
	QStringList list = getProfilesList();
	int x = 0;
	for(QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {
		lbx_profiles->insertItem(*it);
		if(*it == choose)
			lbx_profiles->setCurrentItem(x);
		++x;
	}

	updateSelection();
}

void ProfileManageDlg::slotProfileNew()
{
	QString name;

	ProfileNewDlg *w = new ProfileNewDlg(this);
	int r = w->exec();
	if(r == QDialog::Accepted) {
		name = w->name;

		lbx_profiles->insertItem(name);
		lbx_profiles->setCurrentItem(lbx_profiles->count()-1);
	}
	delete w;

	if(r == QDialog::Accepted) {
		close();
	}
}

void ProfileManageDlg::slotProfileRename()
{
	int x = lbx_profiles->currentItem();
	if(x == -1)
		return;

	QString oldname = lbx_profiles->text(x);
	QString name;

	while(1) {
		bool ok = FALSE;
		name = QInputDialog::getText(CAP(tr("Rename Profile")), tr("Please enter a new name for the profile.  Keep it simple.\nOnly use letters or numbers.  No punctuation or spaces."), QLineEdit::Normal, name, &ok, this);
		if(!ok)
			return;

		if(profileExists(name)) {
			QMessageBox::information(this, CAP(tr("Rename Profile")), tr("There is already another profile with this name.  Please choose another."));
			continue;
		}
		else if(!profileRename(oldname, name)) {
			QMessageBox::information(this, CAP(tr("Rename Profile")), tr("Unable to rename the profile.  Please do not use any special characters."));
			continue;
		}
		break;
	}

	lbx_profiles->changeItem(name, x);
}

void ProfileManageDlg::slotProfileDelete()
{
	int x = lbx_profiles->currentItem();
	if(x == -1)
		return;
	QString name = lbx_profiles->text(x);
	QString path = ApplicationInfo::profilesDir() + "/" + name;

	// prompt first
	int r = QMessageBox::warning(this,
		CAP(tr("Delete Profile")),
		tr(
		"<qt>Are you sure you want to delete the \"<b>%1</b>\" profile?  "
		"This will delete all of the profile's message history as well as associated settings!</qt>"
		).arg(name),
		tr("No, I changed my mind"),
		tr("Delete it!"));

	if(r != 1)
		return;

	r = QMessageBox::information(this,
		CAP(tr("Delete Profile")),
		tr(
		"<qt>As a precaution, you are being asked one last time if this is what you really want.  "
		"The following folder will be deleted!<br><br>\n"
		"&nbsp;&nbsp;<b>%1</b><br><br>\n"
		"Proceed?"
		).arg(path),
		tr("&No"),
		tr("&Yes"));

	if(r == 1) {
		if(!profileDelete(path)) {
			QMessageBox::critical(this, CAP("Error"), tr("Unable to delete the folder completely.  Ensure you have the proper permission."));
			return;
		}

		lbx_profiles->removeItem(x);
	}
}

void ProfileManageDlg::updateSelection()
{
	int x = lbx_profiles->currentItem();

	if(x == -1) {
		pb_rename->setEnabled(FALSE);
		pb_delete->setEnabled(FALSE);
	}
	else {
		pb_rename->setEnabled(TRUE);
		pb_delete->setEnabled(TRUE);
	}
}


ProfileNewDlg::ProfileNewDlg(QWidget *parent)
:QDialog(parent)
{
	setupUi(this);
	setModal(true);
	setWindowTitle(CAP(caption()));

	bg_defAct->setButton(bg_defAct->id((Q3Button *)rb_chat));
	le_name->setFocus();

	connect(pb_create, SIGNAL(clicked()), SLOT(slotCreate()));
	connect(pb_close, SIGNAL(clicked()), SLOT(reject()));
	connect(le_name, SIGNAL(textChanged(const QString &)), SLOT(nameModified()));

	nameModified();
}

void ProfileNewDlg::slotCreate()
{
	name = le_name->text();

	if(profileExists(name)) {
		QMessageBox::information(this, CAP(tr("New Profile")), tr("There is already an existing profile with this name.  Please choose another."));
		return;
	}

	if(!profileNew(name)) {
		QMessageBox::information(this, CAP(tr("New Profile")), tr("Unable to create the profile.  Please do not use any special characters."));
		return;
	}

	// save config
	PsiOptions o;
	
	if (!o.newProfile()) {
		qWarning("ERROR: Failed to new profile default options");
	}
	
	
	o.setOption("options.messages.default-outgoing-message-type" ,bg_defAct->selected() == (Q3Button *)rb_message ? "message": "chat");
	o.setOption("options.ui.emoticons.use-emoticons" ,ck_useEmoticons->isChecked());
	o.save(pathToProfile(name) + "/options.xml");
	
	accept();
}

void ProfileNewDlg::nameModified()
{
	pb_create->setEnabled(!le_name->text().isEmpty());
}
