/*
 * psipopup.h - the Psi passive popup class
 * Copyright (C) 2001-2019  Psi Team
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSIPOPUP_H
#define PSIPOPUP_H

#ifndef QT_STATICPLUGIN
#define QT_STATICPLUGIN
#endif

#include "psievent.h"
#include "psipopupinterface.h"

class FancyPopup;

class PsiPopup : public QObject, public PsiPopupInterface
{
    Q_OBJECT

public:
    PsiPopup(QObject* parent = nullptr);
    ~PsiPopup();

    virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const Resource& r, const UserListItem* = nullptr, const PsiEvent::Ptr& = PsiEvent::Ptr());
    virtual void popup(PsiAccount* account, PopupManager::PopupType type, const Jid& j, const PsiIcon* titleIcon, const QString& titleText,
           const QPixmap* avatar, const PsiIcon* icon, const QString& text);

    static void deleteAll();

private:
    void setData(const Jid &, const Resource &, const UserListItem * = nullptr, const PsiEvent::Ptr &event = PsiEvent::Ptr());
    void setData(const QPixmap *avatar, const PsiIcon *icon, const QString& text);
    void setJid(const Jid &j);
    QString id() const;
    FancyPopup *popup() const;
    void show();

private:
    class Private;
    Private *d;
    friend class Private;
};

class PsiPopupPlugin : public QObject, public PsiPopupPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.psi-im.Psi.PsiPopupPluginInterface")
    Q_INTERFACES(PsiPopupPluginInterface)

public:
    virtual ~PsiPopupPlugin() { PsiPopup::deleteAll(); }
    virtual QString name() const { return "Classic"; }
    virtual PsiPopupInterface* popup(QObject* p) { return new PsiPopup(p); }
};

#endif
