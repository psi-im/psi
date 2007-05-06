/*
 * mucconfigdlg.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef MUCCONFIG_H
#define MUCCONFIG_H

#include <QDialog>

#include "ui_mucconfig.h"
#include "xmpp_muc.h"

class QScrollArea;
class XDataWidget;
class MUCManager;
class MUCAffiliationsModel;
class MUCAffiliationsProxyModel;
namespace XMPP {
	class XData;
}

using namespace XMPP;

class MUCConfigDlg : public QDialog
{
	Q_OBJECT
		
public:
	MUCConfigDlg(MUCManager*, QWidget*);
	~MUCConfigDlg();

	void setRoleAffiliation(MUCItem::Role, MUCItem::Affiliation);

protected:
	void setRole(MUCItem::Role);
	void setAffiliation(MUCItem::Affiliation);
	void refreshGeneral();
	void refreshAffiliations();
	void removePendingRequest(MUCItem::Affiliation);

protected slots:
	void add();
	void apply();
	void destroy();
	void currentTabChanged(int);
	void applyFilter(const QString&);

	void getConfiguration_success( const XData&);
	void getConfiguration_error(int, const QString&);
	void setConfiguration_success();
	void setConfiguration_error(int, const QString&);
	void setItems_success();
	void setItems_error(int, const QString&);
	void getItemsByAffiliation_success(MUCItem::Affiliation, const QList<MUCItem>&);
	void getItemsByAffiliation_error(MUCItem::Affiliation, int, const QString&);
	void destroy_success();
	void destroy_error(int, const QString&);

private:
	Ui::MUCConfig ui_;
	MUCItem::Role role_;
	MUCItem::Affiliation affiliation_;
	MUCManager* manager_;
	QScrollArea* data_container_;
	XDataWidget* data_;
	MUCAffiliationsModel* affiliations_model_;
	MUCAffiliationsProxyModel* affiliations_proxy_model_;
	QList<MUCItem::Affiliation> pending_requests_;
};

#endif
