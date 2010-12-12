/*
 * sxeedit.cpp - The base class for SXE edits
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include "sxeedit.h"
#include "sxerecordedit.h"

//----------------------------------------------------------------------------
// SxeEdit
//----------------------------------------------------------------------------

SxeEdit::SxeEdit(const QString rid, bool remote) {
	rid_ = rid;
	remote_ = remote;
};

SxeEdit::~SxeEdit() {

};

bool SxeEdit::remote() const {
	return remote_;
};

QString SxeEdit::rid() const {
	return rid_;
};

bool SxeEdit::isNull() {
	return null_;
}

void SxeEdit::nullify() {
	null_ = true;
}


bool SxeEdit::overridenBy(const SxeEdit &e) const {
	if(e.rid() == rid()) {
		if(e.type() == SxeEdit::Remove)
			return true;
		else if(type() == SxeEdit::Record && e.type() == SxeEdit::Record) {
			const SxeRecordEdit* ep = dynamic_cast<const SxeRecordEdit*>(&e);
			const SxeRecordEdit* tp = dynamic_cast<const SxeRecordEdit*>(this);
			return (ep->version() <= tp->version());
		}
	}

	return false;
}

bool SxeEdit::operator<(const SxeEdit &other) const {

	// Can't compare edits to different records
	if(rid() != other.rid())  {
		qDebug() << QString("Comparing SxeEdits to %1 an %2.").arg(rid()).arg(other.rid()).toAscii();
		return false;
	}

	if(type() == other.type()) {

		// Only Record edits can be unequal with other edits of the same type
		if(type() == SxeEdit::Record) {
			const SxeRecordEdit* thisp = dynamic_cast<const SxeRecordEdit*>(this);
			const SxeRecordEdit* otherp = dynamic_cast<const SxeRecordEdit*>(&other);
			return (thisp->version() < otherp->version());
		}

		return false;

	} else {

		// New < Record, Record < Remove
		if(type() == SxeEdit::New)		   return true;
		if(other.type() == SxeEdit::Remove)  return true;

		return false;

	}
}
