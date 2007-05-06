/*
 * discodlg.h - main dialog for the Service Discovery protocol
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef DISCODLG_H
#define DISCODLG_H

#include <QDialog>

#include "ui_disco.h"
#include "xmpp_jid.h"

class PsiAccount;
class QString;

using namespace XMPP;

class DiscoDlg : public QDialog, public Ui::Disco
{
	Q_OBJECT
public:
	DiscoDlg(PsiAccount *, const Jid &, const QString &node = QString::null);
	~DiscoDlg();

	void doDisco(QString host = QString::null, QString node = QString::null);

signals:
	void featureActivated(QString feature, Jid jid, QString node);

public:
	class Private;
	friend class Private;
private:
	Private *d;
};


#endif

