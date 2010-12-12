/*
 * sxeedit.h - A class for SXE edits that remove a node
 * Copyright (C) 2007  Joonas Govenius
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

#ifndef SXDEREMOVEEDIT_H
#define SXDEREMOVEEDIT_H

#include "sxeedit.h"

/*! \brief A class used for storing SXE edits that remove nodes in the undo stacks and in the queue of outgoing edits.*/
class SxeRemoveEdit : public SxeEdit {
	public:
		/*! \brief Constructor
		 *  Constructs a SxeRemoveEdit for \a node.
		 */
		SxeRemoveEdit(const QString rid, bool remote = false);
		/*! \brief Constructor
		 *  Parses a SxeRemoveEdit from \a sxeElement.
		 */
		SxeRemoveEdit(const QDomElement &sxeElement, bool remote = true);
		/*! \brief The type of edit.*/
		SxeEdit::EditType type() const;
		/*! \brief The XML (the SXE) representing the edit.*/
		QDomElement xml(QDomDocument &doc) const;
};

#endif
