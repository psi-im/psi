/*
 * activecontactsmenu.cpp
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

#include "activecontactsmenu.h"

#include "psiaccount.h"
#include "psicon.h"
#include "psicontact.h"
#include "psiiconset.h"

class ActiveContactAction : public QAction {
    Q_OBJECT
public:
    ActiveContactAction(const QString &jid, const QString &nick, const QIcon &icon, PsiAccount *pa, QMenu *parent) :
        QAction(icon, nick, parent), pa_(pa), jid_(jid)
    {
        parent->addAction(this);
        connect(this, SIGNAL(triggered()), SLOT(actionActivated()));
    }

private slots:
    void actionActivated() { pa_->actionDefault(Jid(jid_)); }

private:
    PsiAccount *pa_;
    QString     jid_;
};

ActiveContactsMenu::ActiveContactsMenu(PsiCon *psi, QWidget *parent) : QMenu(parent), psi_(psi)
{
    for (PsiAccount *pa : psi_->contactList()->accounts()) {
        if (!pa->enabled())
            continue;

        QList<PsiContact *> list = pa->activeContacts();
        for (PsiContact *pc : list) {
            new ActiveContactAction(pc->jid().full(), pc->name(),
                                    PsiIconset::instance()->statusPtr(pc->jid(), pc->status())->icon(), pa, this);
        }
    }
}

#include "activecontactsmenu.moc"
