/*
 * sxeremoveedit.cpp - An single SXE edit that changes a node
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
 
#include "sxerecordedit.h"
#include "sxesession.h"

//----------------------------------------------------------------------------
// SxeRecordEdit
//----------------------------------------------------------------------------

SxeRecordEdit::SxeRecordEdit(const QString rid, int version, QHash<Key, QString> changes, bool remote) : SxeEdit(rid, remote) {
	version_ = version;
	changes_ = changes;
}

SxeRecordEdit::SxeRecordEdit(const QDomElement &sxeElement, bool remote) : SxeEdit(sxeElement.attribute("rid"), remote) {
	version_ = sxeElement.attribute("version").toInt();

	if(sxeElement.hasAttribute("parent"))
		changes_[Parent] = sxeElement.attribute("parent");
	if(sxeElement.hasAttribute("primary-weight"))
		changes_[PrimaryWeight] = sxeElement.attribute("primary-weight");
	if(sxeElement.hasAttribute("name"))
		changes_[Name] = sxeElement.attribute("name");
	if(sxeElement.hasAttribute("chdata"))
		changes_[Chdata] = sxeElement.attribute("chdata");
	if(sxeElement.hasAttribute("replacefrom"))
		changes_[ReplaceFrom] = sxeElement.attribute("replacefrom");
	if(sxeElement.hasAttribute("replacen"))
		changes_[ReplaceN] = sxeElement.attribute("replacen");
	if(sxeElement.hasAttribute("pitarget"))
		changes_[ProcessingInstructionTarget] = sxeElement.attribute("pitarget");
	if(sxeElement.hasAttribute("pidata"))
		changes_[ProcessingInstructionData] = sxeElement.attribute("pidata");
}

SxeEdit::EditType SxeRecordEdit::type() const {
	return SxeEdit::Record;
}

QDomElement SxeRecordEdit::xml(QDomDocument &doc) const {
	QDomElement edit = doc.createElementNS(SXENS, "set");

	edit.setAttribute("rid", rid_);
	edit.setAttribute("version", version_);
	foreach(Key key, changes_.keys())
		edit.setAttribute(keyToString(key), changes_[key]);

	return edit;
}

int SxeRecordEdit::version() const {
	return version_;
}

QList<SxeRecordEdit::Key> SxeRecordEdit::keys() const {
	return changes_.keys();
}

QString SxeRecordEdit::value(Key key) const {
	return changes_.value(key);
}

QString SxeRecordEdit::keyToString(Key key) {
	if(key == Parent) return "parent";
	if(key == PrimaryWeight) return "primary-weight";
	if(key == Name) return "name";
	if(key == Chdata) return "chdata";
	if(key == ReplaceFrom) return "replacefrom";
	if(key == ReplaceN) return "replacen";
	if(key == ProcessingInstructionTarget) return "pitarget";
	if(key == ProcessingInstructionData) return "pidata";
	return "";
}

void SxeRecordEdit::nullify() {
	SxeEdit::nullify();

	changes_.clear();
}
