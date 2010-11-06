/*
 * registrationdlg.h
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

#ifndef REGISTRATIONDLG_H
#define REGISTRATIONDLG_H

#include <QDialog>

class QDomElement;
class JT_XRegister;
class PsiAccount;
namespace XMPP {
	class Jid;
	class Form;
	class XData;
}

class RegistrationDlg : public QDialog
{
	Q_OBJECT

public:
	RegistrationDlg(const XMPP::Jid &, PsiAccount *);
	~RegistrationDlg();

public slots:
	void done(int);

private slots:
	void doRegGet();
	void doRegSet();
	void jt_finished();

private:
	class Private;
	Private *d;

	void setData(JT_XRegister* jt);
	void updateData(JT_XRegister* jt);
	void setInstructions(const QString& jid, const QString& instructions);
	void processXData(const XMPP::XData& form);
	void processLegacyForm(const XMPP::Form& form);
};

#endif
