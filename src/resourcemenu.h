/*
 * resourcemenu.h - helper class for displaying contact's resources
 * Copyright (C) 2001-2019  Psi Team
 * Copyright (C) 2006-2010  Michail Pishchagin
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

#ifndef RESOURCEMENU_H
#define RESOURCEMENU_H

#include <QMenu>
#include <QPointer>

#include "xmpp_jid.h"

class PsiContact;
class UserResource;

class ResourceMenu : public QMenu
{
    Q_OBJECT
public:
    ResourceMenu(QWidget *parent);
    ResourceMenu(const QString& title, PsiContact* contact, QWidget* parent);

    bool activeChatsMode() const;
    void setActiveChatsMode(bool activeChatsMode);

    void addResource(const UserResource &r);
    void addResource(int status, QString name);

signals:
    void resourceActivated(PsiContact* contact, const XMPP::Jid& jid);
    void resourceActivated(QString resource);

private slots:
    void actionActivated();
    void contactUpdated();

private:
    QPointer<PsiContact> contact_;
    bool activeChatsMode_;
};

#endif
