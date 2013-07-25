/*
 * sxeremoveedit.cpp - An single SXE edit that removes a node
 * Copyright (C) 2006  Joonas Govenius
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "sxeremoveedit.h"
#include "sxesession.h"

//----------------------------------------------------------------------------
// SxeRemoveEdit
//----------------------------------------------------------------------------

SxeRemoveEdit::SxeRemoveEdit(const QString rid, bool remote) : SxeEdit(rid, remote) {

}

SxeRemoveEdit::SxeRemoveEdit(const QDomElement &sxeElement, bool remote) : SxeEdit(sxeElement.attribute("rid"), remote) {

}

SxeEdit::EditType SxeRemoveEdit::type() const {
    return SxeEdit::Remove;
}

QDomElement SxeRemoveEdit::xml(QDomDocument &doc) const {
    QDomElement edit = doc.createElementNS(SXENS, "remove");

    edit.setAttribute("rid", rid_);

    return edit;
}
