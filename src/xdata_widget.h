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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef XDATAWIDGET_H
#define XDATAWIDGET_H

#include <qwidget.h>
#include <QList>

#include "xmpp_xdata.h"

class XDataField;

using namespace XMPP;

class XDataWidget : public QWidget
{
public:
	XDataWidget(QWidget *parent = 0, const char *name = 0);
	~XDataWidget();

	XData::FieldList fields() const;
	void setFields(const XData::FieldList &);

private:
	typedef QList<XDataField*> XDataFieldList;
	XDataFieldList fields_;
};

#endif
