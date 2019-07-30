/*
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef CALLDLG_H
#define CALLDLG_H

#include <QDialog>

namespace XMPP {
    class Jid;
}

class AvCall;
class PsiAccount;

class CallDlg : public QDialog
{
    Q_OBJECT

public:
    CallDlg(PsiAccount *pa, QWidget *parent = nullptr);
    ~CallDlg();

    void setOutgoing(const XMPP::Jid &jid);
    void setIncoming(AvCall *sess);

private:
    class Private;
    Private *d;
};

#endif
