/*
 * passdialog.cpp
 * Copyright (C) 2009-2010  Virnik
 * Copyright (C) 2011  Evgeny Khryukin
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

#include "passdialog.h"

PassDialog::PassDialog(const QString &jid, QWidget *parent) : QDialog(parent)
{
    m_ui.setupUi(this);
    setWindowTitle(tr("Wrong Account Password"));
    m_ui.labelAcc->setText(tr("Please enter your password for %1:").arg(jid));
}

bool PassDialog::savePassword() const { return m_ui.cb_savePassword->isChecked(); }

void PassDialog::setSavePassword(bool save) { m_ui.cb_savePassword->setChecked(save); }

QString PassDialog::password() const { return m_ui.le_password->text(); }
