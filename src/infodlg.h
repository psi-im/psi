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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef INFODLG_H
#define INFODLG_H

#include "ui_info.h"
#include "ui_infodlg.h"

namespace XMPP
{
	class Jid;
	class VCard;
	class Resource;
}

using namespace XMPP;

class PsiAccount;

class InfoWidget : public QWidget
{
	Q_OBJECT
public:
	enum { Self, Contact, MucContact, MucAdm };
	InfoWidget(int type, const XMPP::Jid &, const XMPP::VCard &, PsiAccount *, QWidget *parent=0, bool cacheVCard = true);
	~InfoWidget();
	bool aboutToClose(); /* call this when you are going to close parent dialog */
	PsiAccount *account() const;
	const Jid &jid() const;

protected:
	// reimplemented
	//void closeEvent(QCloseEvent *);
	void showEvent ( QShowEvent * event );
	bool updatePhoto();

public slots:
	void doRefresh();
	void updateStatus();
	void setStatusVisibility(bool visible);
	void publish();

private slots:
	void contactAvailable(const Jid &, const Resource &);
	void contactUnavailable(const Jid &, const Resource &);
	void contactUpdated(const Jid &);
	void clientVersionFinished();
	void entityTimeFinished();
	void requestLastActivityFinished();
	void jt_finished();
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
	//QPushButton* pb_refresh_;
	//QPushButton* pb_close_;
	//QPushButton* pb_submit_;

	void setData(const XMPP::VCard &);
	XMPP::VCard makeVCard();
	void fieldsEnable(bool);
	void setReadOnly(bool);
	bool edited();
	void setEdited(bool);
	void setPreviewPhoto(const QString& str);
	void requestResourceInfo(const XMPP::Jid& j);
	void requestLastActivity();

signals:
	void busy();
	void released();
};

class InfoDlg : public QDialog
{
	Q_OBJECT
public:
	InfoDlg(int type, const XMPP::Jid &, const XMPP::VCard &, PsiAccount *, QWidget *parent=0, bool cacheVCard = true);
	inline InfoWidget *infoWidget() const { return iw; }

protected:
	void closeEvent(QCloseEvent * e);

private slots:
	void doDisco();
	void doBusy();
	void release();

private:
	Ui::InfoDlg ui_;
	InfoWidget *iw;
};

#endif
