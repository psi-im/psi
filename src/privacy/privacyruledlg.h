/*
 * privacyruledlg.h
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PRIVACYRULEDLG_H
#define PRIVACYRULEDLG_H

#include "ui_privacyrule.h"

#include <QDialog>

class PrivacyListItem;

class PrivacyRuleDlg : public QDialog
{
    Q_OBJECT

public:
    PrivacyRuleDlg();

    void setRule(const PrivacyListItem&);
    PrivacyListItem rule() const;

protected slots:
    void type_selected(const QString&);

private:
    Ui::PrivacyRule ui_;
};

#endif // PRIVACYRULEDLG_H
