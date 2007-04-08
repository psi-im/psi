/*
 * mucaffiliationsmodel.h
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

#ifndef MUCAFFILIATIONSMODEL_H
#define MUCAFFILIATIONSMODEL_H

#include <QStandardItemModel>
#include <QList>
#include <QMap>

#include "xmpp_muc.h"

class QMimeData;

class MUCAffiliationsModel : public QStandardItemModel
{
	Q_OBJECT

public:
	MUCAffiliationsModel();
	
	virtual Qt::ItemFlags flags(const QModelIndex &index) const;
	virtual Qt::DropActions supportedDropActions() const;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
	QStringList mimeTypes () const;
	QMimeData* mimeData(const QModelIndexList &indexes) const;

	void resetAffiliationLists();
	void setAffiliationListEnabled(XMPP::MUCItem::Affiliation, bool = true);
	QModelIndex affiliationListIndex(XMPP::MUCItem::Affiliation);
	void addItems(const QList<XMPP::MUCItem>&);
	QList<XMPP::MUCItem> changes() const;
	
protected:
	enum AffiliationListIndex { 
		Owners = 0, Admins = 1, Members = 2, Outcast = 3 , Unknown = 4 };
	
	void resetAffiliationList(XMPP::MUCItem::Affiliation);
	static QString affiliationlistindexToString(AffiliationListIndex);
	AffiliationListIndex affiliationToIndex(XMPP::MUCItem::Affiliation);
	static XMPP::MUCItem::Affiliation indexToAffiliation(int);

private:
	QList<XMPP::MUCItem> items_;
	QMap<AffiliationListIndex,bool> enabled_;
};


#endif
