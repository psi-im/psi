/*
 * sxeedit.h - The base class for SXE edits
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

#ifndef SXDEEDIT_H
#define SXDEEDIT_H

#include <QString>
#include <QDomNode>

/*! \brief A class used for storing SXE edits in the undo stacks and in the queue of outgoing edits.*/
class SxeEdit {
	public:
		/*! \brief Describes which kind of edit the object represents.*/
		enum EditType {New, Record, Remove};

		/*! \brief Constructor
		 *  Constructs an SxeEdit for a node with \a rid.
		 */
		SxeEdit(const QString rid, bool remote);

		virtual ~SxeEdit();

		/*! \brief Indicates whether the edit is from a remote client.*/
		bool remote() const;

		/*! \brief The rid of the edit.*/
		QString rid() const;

		/*! \brief The type of edit.*/
		virtual EditType type() const = 0;

		/*! \brief The XML (the SXE) representing the edit.*/
		virtual QDomElement xml(QDomDocument &doc) const = 0;

		/*! \brief A null edit is a no-op.*/
		bool isNull();

		/*! \brief Turns the edit into a no-op.*/
		virtual void nullify();

		/*! \brief Returns true if \a e renders the SxeEdit ineffective
		 *  (e.g. a SxeRemoveEdit will make a SxeNewEdit ineffective).
		 */
		bool overridenBy(const SxeEdit &e) const;

		bool operator<(const SxeEdit &other) const;

	protected:
		/*! \brief The node id of the target.*/
		QString rid_;

		/*! \brief Indicates whether the edit is from a remote client.*/
		bool remote_;

		/*! \brief Indicates whether the edit has been nullified.*/
		bool null_;
};

#endif
