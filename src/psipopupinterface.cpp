/*
 * psipopuinterface.cpp
 * Copyright (C) 2012  Evgeny Khryukin
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

#include "psipopupinterface.h"

#include "common.h"
#include "psiiconset.h"
#include "psioptions.h"

QString PsiPopupInterface::clipText(QString text)
{
    int len = PsiOptions::instance()->getOption("options.ui.notifications.passive-popups.maximum-text-length").toInt();
    if (len > 0) {
        // richtext will give us trouble here
        if (int(text.length()) > len) {
            text = text.left(len);

            // delete last unclosed tag
            /*if ( text.find("</") > text.find(">") ) {

                text = text.left( text.find("</") );
            }*/

            text += "...";
        }
    }

    return text;
}

QString PsiPopupInterface::title(PopupManager::PopupType type, bool *doAlertIcon, PsiIcon **icon)
{
    QString text;

    switch (type) {
    case PopupManager::AlertOnline:
        text         = QObject::tr("Contact online");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("status/online"));
        *doAlertIcon = false;
        break;
    case PopupManager::AlertOffline:
        text         = QObject::tr("Contact offline");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("status/offline"));
        *doAlertIcon = false;
        break;
    case PopupManager::AlertStatusChange:
        text         = QObject::tr("Status change");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("status/online"));
        *doAlertIcon = false;
        break;
    case PopupManager::AlertMessage:
        text         = QObject::tr("Incoming message");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/message"));
        *doAlertIcon = true;
        break;
    case PopupManager::AlertComposing:
        text         = QObject::tr("Typing notify");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/typing"));
        *doAlertIcon = false;
        break;
    case PopupManager::AlertChat:
        text         = QObject::tr("Incoming chat message");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/chat"));
        *doAlertIcon = true;
        break;
    case PopupManager::AlertHeadline:
        text         = QObject::tr("Headline");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/headline"));
        *doAlertIcon = true;
        break;
    case PopupManager::AlertFile:
        text         = QObject::tr("Incoming file");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/file"));
        *doAlertIcon = true;
        break;
    case PopupManager::AlertAvCall:
        text         = QObject::tr("Incoming call");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/call"));
        *doAlertIcon = true;
        break;
    case PopupManager::AlertGcHighlight:
        text         = QObject::tr("Groupchat highlight");
        *icon        = const_cast<PsiIcon *>(IconsetFactory::iconPtr("psi/headline"));
        *doAlertIcon = true;
        break;
    default:
        break;
    }

    return CAP(text);
}
