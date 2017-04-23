/*
 * xdata_widget.h - a class for displaying jabber:x:data forms
 * Copyright (C) 2003-2004  Michail Pishchagin
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

#ifndef XDATAWIDGET_H
#define XDATAWIDGET_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QVBoxLayout>

#include "xmpp_stanza.h"
#include "xmpp_xdata.h"
#include "xmpp_jid.h"

class PsiCon;
class XDataField;

namespace XMPP {
	class Client;
}

class XDataWidget : public QWidget
{
	Q_OBJECT

public:
	XDataWidget(PsiCon *psi, QWidget *parent, XMPP::Client* client, XMPP::Jid owner);
	~XDataWidget();

	PsiCon *psi() const;
	XMPP::Client* client() const;
	QString registrarType() const;
	XMPP::Jid owner() const;
	XMPP::Stanza::Error consistencyError() const;

	void setForm(const XMPP::XData&, bool withInstructions = true);

	XMPP::XData::FieldList fields() const;
	XDataField* fieldByVar(const QString &) const;

protected slots:
	void linkActivated(const QString&);

private:
	void setInstructions(const QString&);
	void setFields(const XMPP::XData::FieldList &);

private:
	typedef QList<XDataField*> XDataFieldList;
	XDataFieldList fields_;
	QString registrarType_;
	QVBoxLayout* layout_;
	PsiCon *psi_;
	XMPP::Client* client_;
	XMPP::Jid owner_;
	bool consistent_;
	XMPP::Stanza::Error consistencyError_;
};

#endif
