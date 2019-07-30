/*
 * registrationdlg.h
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2001-2002  Justin Karneges
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef REGISTRATIONDLG_H
#define REGISTRATIONDLG_H

#include <QDialog>

namespace XMPP {
    class Form;
    class Jid;
    class XData;
}

class JT_XRegister;
class PsiAccount;
class QDomElement;

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
