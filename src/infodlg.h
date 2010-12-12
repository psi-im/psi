/*
 * infodlg.h - handle vcard
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

#ifndef INFODLG_H
#define INFODLG_H

#include "ui_info.h"

namespace XMPP
{
	class Jid;
	class VCard;
	class Resource;
}

using namespace XMPP;

class PsiAccount;

class InfoDlg : public QDialog
{
	Q_OBJECT
public:
	enum { Self, Contact };
	InfoDlg(int type, const XMPP::Jid &, const XMPP::VCard &, PsiAccount *, QWidget *parent=0, bool cacheVCard = true);
	~InfoDlg();

protected:
	// reimplemented
	//void closeEvent(QCloseEvent *);
	void showEvent ( QShowEvent * event );
	bool updatePhoto();

public slots:
	void doRefresh();
	void updateStatus();
	void closeEvent ( QCloseEvent * e );
	void setStatusVisibility(bool visible);

private slots:
	void contactAvailable(const Jid &, const Resource &);
	void contactUnavailable(const Jid &, const Resource &);
	void contactUpdated(const Jid &);
	void clientVersionFinished();
	void requestLastActivityFinished();
	void jt_finished();
	void doSubmit();
	void doDisco();
	void doShowCal();
	void doUpdateFromCalendar(const QDate &);
	void doClearBirthDate();
	void textChanged();
	void selectPhoto();
	void clearPhoto();
	void showPhoto();
	void goHomepage();

private:
	class Private;
	Private *d;
	Ui::Info ui_;
	QPushButton* pb_refresh_;
	QPushButton* pb_close_;
	QPushButton* pb_submit_;

	void setData(const XMPP::VCard &);
	XMPP::VCard makeVCard();
	void fieldsEnable(bool);
	void setReadOnly(bool);
	bool edited();
	void setEdited(bool);
	void setPreviewPhoto(const QString& str);
	void requestClientVersion(const XMPP::Jid& j);
	void requestLastActivity();
};

#endif
