/*
 * sxenewedit.h - An single SXE edit that creates a new node
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

#ifndef SXDENEWEDIT_H
#define SXDENEWEDIT_H

#include "sxeedit.h"

/*! \brief A class used for storing SXE edits that create nodes in the undo stacks and in the queue of outgoing edits.*/
class SxeNewEdit : public SxeEdit {
	public:
		/*! \brief Constructor
		 *  Constructs a SxeNewEdit for \a node.
		 */
		SxeNewEdit(const QString rid, const QDomNode &node, const QString parent, double primaryWeight, bool remote);
		/*! \brief Constructor
		 *  Parses a SxeNewEdit from \a sxeElement.
		 */
		SxeNewEdit(const QDomElement &sxeElement, bool remote = true);
		/*! \brief The type of edit.*/
		SxeEdit::EditType type() const;
		/*! \brief The XML (the SXE) representing the edit.*/
		QDomElement xml(QDomDocument &doc) const;

		/*! \brief Returns the type of the node.*/
		QDomNode::NodeType nodeType() const;
		/*! \brief Returns the rid of the parent.*/
		QString parent() const;
		/*! \brief Returns the primary-weight of the node.*/
		double primaryWeight() const;
		/*! \brief Returns the name of the node, if any.*/
		QString name() const;
		/*! \brief Returns the namespace of the node, if any.*/
		QString nameSpace() const;
		/*! \brief Returns the chdata of the node, if any.*/
		QString chdata() const;

		/*! \brief Returns the target of the processing instruction, if any.*/
		QString processingInstructionTarget() const;
		/*! \brief Returns the target of the processing instruction, if any.*/
		QString processingInstructionData() const;


	private:
		QString type_;
		QString parent_;
		double primaryWeight_;
		QString ns_;
		QString identifier_;
		QString data_;
};

#endif
