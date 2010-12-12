/*
 * sxeedit.h - A class for SXE edits that change a node
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

#ifndef SXDEMETADATAEDIT_H
#define SXDEMETADATAEDIT_H

#include "sxeedit.h"
#include <QHash>

/*! \brief A class used for storing SXE edits that change nodes in the undo stacks and in the queue of outgoing edits.*/
class SxeRecordEdit : public SxeEdit {
	public:
		/*! \brief The possible keys for record.*/
		enum Key {Parent, PrimaryWeight, Name, Chdata, ReplaceFrom, ReplaceN, ProcessingInstructionTarget, ProcessingInstructionData};

		/*! \brief Constructor
		 *  Constructs a SxeRecordEdit for \a node.
		 */
		SxeRecordEdit(const QString rid, int version, QHash<Key, QString> changes, bool remote = false);
		/*! \brief Constructor
		 *  Parses a SxeRecordEdit from \a sxeElement.
		 */
		SxeRecordEdit(const QDomElement &sxeElement, bool remote = true);

		/*! \brief The type of edit.*/
		SxeEdit::EditType type() const;
		/*! \brief The XML (the SXE) representing the edit.*/
		QDomElement xml(QDomDocument &doc) const;
		/*! \brief The version of the edit.*/
		int version() const;
		/*! \brief The keys for the record entries that the edit modifies. */
		QList<Key> keys() const;
		/*! \brief The value that the edit will set for the given record entry. */
		QString value(Key key) const;

		/*! \brief Turns the edit into a no-op.*/
		void nullify();

	private:
		static QString keyToString(Key key);

		int version_;
		QHash<Key, QString> changes_;
};

#endif
