/*
 * sxerecord.cpp - A class for storing the record of an individual node
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

#include "sxerecord.h"


static bool referencedEditLessThan(const SxeEdit* e1, const SxeEdit* e2) { return *e1 < *e2; }


//----------------------------------------------------------------------------
// SxeRecord
//----------------------------------------------------------------------------

SxeRecord::SxeRecord(QString rid) {
	rid_ = rid;
};

SxeRecord::~SxeRecord() {
	while(!edits_.isEmpty()) {
		delete edits_.takeFirst();
	}
};

QDomNode SxeRecord::node() const {
	return node_;
}

void SxeRecord::apply(QDomDocument &doc, SxeEdit* edit) {
	if(edit->rid() == rid()) {
		if(edit->type() == SxeEdit::New)
			applySxeNewEdit(doc, dynamic_cast<SxeNewEdit*>(edit));
		else if (edit->type() == SxeEdit::Remove)
			applySxeRemoveEdit(dynamic_cast<SxeRemoveEdit*>(edit));
		else if (edit->type() == SxeEdit::Record)
			applySxeRecordEdit(dynamic_cast<SxeRecordEdit*>(edit));
	} else {
		qDebug() << QString("Tried to apply an edit meant for %1 to %2.").arg(edit->rid()).arg(rid()).toAscii();
	}
}

QList<const SxeEdit*> SxeRecord::edits() const {
	QList<const SxeEdit*> edits;
		foreach(SxeEdit* e, edits_) {
			edits.append(e);
		}
	return edits;
};

bool SxeRecord::applySxeNewEdit(QDomDocument &doc, SxeNewEdit* edit) {
	if(!(edits_.size() == 0 && node_.isNull())) {
		qDebug("Someone's not behaving! Tried to apply a SxeNewEdit to an existing node.");
		emit nodeRemovalRequired(node_);
		return false;
	}

	// create the new node
	if(edit->nodeType() == QDomNode::ElementNode) {

		if(edit->nameSpace().isEmpty())
			node_ = doc.createElement(edit->name());
		else
			node_ = doc.createElementNS(edit->nameSpace(), edit->name());

	} else if(edit->nodeType() == QDomNode::AttributeNode) {

		if(edit->nameSpace().isEmpty())
			node_ = doc.createAttribute(edit->name());
		else
			node_ = doc.createAttributeNS(edit->nameSpace(), edit->name());

		node_.toAttr().setValue(edit->chdata());

	} else if(edit->nodeType() == QDomNode::TextNode) {

		node_ = doc.createTextNode(edit->chdata());

	} else if(edit->nodeType() == QDomNode::CommentNode) {

		node_ = doc.createComment(edit->chdata());

	} else if(edit->nodeType() == QDomNode::ProcessingInstructionNode) {

		node_ = doc.createProcessingInstruction(edit->processingInstructionTarget(), edit->processingInstructionData());

	} else
		return false;

	edits_ += edit;
	revertToZero();

	emit nodeToBeAdded(node_, edit->remote(), edit->rid());

	lastParent_ = parent();
	lastPrimaryWeight_ = primaryWeight();

	return true;
}

void SxeRecord::revertToZero() {
#ifndef NDEBUG
	if (edits_[0]->type() != SxeEdit::New)
		qDebug() << QString("First edit is of type %1!").arg(edits_[0]->type());
#endif

	const SxeNewEdit* edit = dynamic_cast<const SxeNewEdit*>(edits_[0]);

	parent_ = edit->parent();
	primaryWeight_ = edit->primaryWeight();
	identifier_ = edit->name();
	data_ = edit->chdata();
}

bool SxeRecord::applySxeRemoveEdit(SxeRemoveEdit* edit) {
	if(!node_.isNull()) {
		emit nodeToBeRemoved(node_, edit->remote());

		if(node_.isAttr()) {
			QDomNode parent = node_.parentNode();
			while(!parent.isElement() && !parent.isNull())
				parent = parent.parentNode();

			if(!parent.isNull()) {
				parent.toElement().removeAttributeNode(node_.toAttr());
				emit nodeRemoved(node_, edit->remote());
			} // else, the attr hadn't been added to the doc yet.
		} else {
			node_.parentNode().removeChild(node_);
			emit nodeRemoved(node_, edit->remote());
		}

		edits_ += edit;

		// delete the record
		deleteLater();
	}

	return true;
}

bool SxeRecord::applySxeRecordEdit(SxeRecordEdit* edit) {
	if(!node_.isNull() && edits_.size() > 0) {

		if(edit->version() == version() + 1 && !edits_.last()->overridenBy(*edit)) {

			// process the "in order" edit
			edits_ += edit;
			processInOrderRecordEdit(edit);

		} else {

			edits_ += edit;
			reorderEdits();

		}

		updateNode(edit->remote());

		return true;

	}
	return false;
}

void SxeRecord::processInOrderRecordEdit(const SxeRecordEdit* edit) {

	foreach(SxeRecordEdit::Key key, edit->keys()) {

		if(key == SxeRecordEdit::Parent && edit->value(key) != parent()) {

			parent_ = edit->value(key);

		} else if(key == SxeRecordEdit::PrimaryWeight && edit->value(key).toDouble() != primaryWeight()) {

			primaryWeight_ = edit->value(key).toDouble();

		} else if(key == SxeRecordEdit::Name && edit->value(key) != name()) {

			identifier_ = edit->value(key);

		} else if(key == SxeRecordEdit::ProcessingInstructionTarget && edit->value(key) != processingInstructionTarget()) {

			identifier_ = edit->value(key);

		} else if(key == SxeRecordEdit::Chdata || key == SxeRecordEdit::ProcessingInstructionData) {

			// Check for partial replacements
			QList<SxeRecordEdit::Key> keys = edit->keys();
			if(keys.contains(SxeRecordEdit::ReplaceFrom)
				&& keys.contains(SxeRecordEdit::ReplaceN)) {

				// 'replacefrom' & 'replacen' exist, do they contain integers?
				bool ok1, ok2;
				int from = edit->value(SxeRecordEdit::ReplaceFrom).toInt(&ok1);
				int n = edit->value(SxeRecordEdit::ReplaceN).toInt(&ok2);

				if(ok1 && ok2 && from >= 0 && n >= 0 && from + n <= data_.length()) {

					// Do partial replace if the range makes sense
					data_.replace(from, n, edit->value(key));

				} else {
					if(!ok1)
						qDebug() << QString("Could not convert 'replacefrom' = '%1' to int.").arg(edit->value(SxeRecordEdit::ReplaceFrom));
					if(!ok2)
						qDebug() << QString("Could not convert 'replacen' = '%1' to int.").arg(edit->value(SxeRecordEdit::ReplaceN));
					if(from < 0)
						qDebug() << QString("'replacefrom' = '%1' is negative.").arg(edit->value(SxeRecordEdit::ReplaceFrom));
					if(n < 0)
						qDebug() << QString("'replacen' = '%1' is negative.").arg(edit->value(SxeRecordEdit::ReplaceN));
					if(from + n > data_.length())
						qDebug() << QString("from (%1) + n (%2) > data_.length() (%3).").arg(from).arg(n).arg(data_.length());
				}

			} else {

				data_ = edit->value(key);

			}

		}
	}

}

void SxeRecord::reorderEdits() {
	qSort(edits_.begin(), edits_.end(), referencedEditLessThan);

	revertToZero();

	// Apply all the valid record edits
	for(int i = 1; i < edits_.size(); i++) {

		if (edits_[i]->type() == SxeEdit::Record) {
			SxeRecordEdit* edit = dynamic_cast<SxeRecordEdit*>(edits_[i]);

			// Check that the version matches and that the next edit doesn't override it.
			if (i == edit->version() && (i+1 == edits_.size() || !edit->overridenBy(*edits_[i+1])))
				processInOrderRecordEdit(edit);
			else if (edit->version() <= i)
				edit->nullify();  // There's no way the edit could be applied anymore

		} else {
			qDebug() << QString("Edit of type %1 at %2!").arg(edits_[i]->type()).arg(i).toAscii();
		}

	}

}

void SxeRecord::updateNode(bool remote) {
	if(parent_ != lastParent_ || primaryWeight_ != lastPrimaryWeight_)
		emit nodeToBeMoved(node_, remote);

	if(identifier_ != lastIdentifier_) {

		if ((node_.isElement() || node_.isAttr())
			&& node_.nodeName() != name()) {

			// emit nameToBeChanged(node_, remote);
			// TODO: update the name somehow
			// emit nameChanged(node_, remote);

		} else if(node_.isProcessingInstruction()) {

			// emit processingInstructionTargetToBeChanged(node_, remote);
			// TODO: figure out a way to change the target
			//	  will probably need to recreate the pi
			// emit processingInstructionTargetChanged(node_, remote);
		}
	}

	if(data_ != lastData_) {

		// qDebug() << QString("Setting '%1' to \"%2\"").arg(node_.nodeName()).arg(data_).toAscii();

		if((node_.isText() || node_.isAttr() || node_.isComment())) {

			emit chdataToBeChanged(node_, remote);
			node_.setNodeValue(data_);
			emit chdataChanged(node_, remote);

		} else if(node_.isProcessingInstruction()) {

			emit processingInstructionDataToBeChanged(node_, remote);
			node_.toProcessingInstruction().setData(data_);
			emit processingInstructionDataChanged(node_, remote);

		}

	}

}

QString SxeRecord::rid() const {
	return rid_;
};

QString SxeRecord::parent() const {
	return parent_;
}

double SxeRecord::primaryWeight() const {
	return primaryWeight_;
}

int SxeRecord::version() const {
	return edits_.size() - 1;
}

QString SxeRecord::name() const {
	return identifier_;
}

QString SxeRecord::nameSpace() const {
	if(node_.isNull())
		return QString();
	else
		return node_.namespaceURI();
}

QString SxeRecord::chdata() const {
	return data_;
}

QString SxeRecord::processingInstructionTarget() const {
	return identifier_;
}

QString SxeRecord::processingInstructionData() const {
	return data_;
}

bool SxeRecord::hasSmallerSecondaryWeight(const SxeRecord &other) const {
	// compare the "secondary weight" (the id's)
	QString selfid = rid();
	QString otherid = other.rid();
	for(int i = 0; i < selfid.length() && i < otherid.length(); i++) {
		if(selfid[i].unicode() < otherid[i].unicode())
			return true;
		else if(selfid[i].unicode() > otherid[i].unicode())
			return false;
	}
	// if no difference was found. one must be shorter
	return (selfid.length() < otherid.length());
}

bool SxeRecord::operator==(const SxeRecord &other) const {
	return rid() == other.rid();
}

bool SxeRecord::operator<(const SxeRecord &other) const {
	if(other == *this)
		return false;

	if(primaryWeight() < other.primaryWeight())
		return true;
	else if(primaryWeight() == other.primaryWeight()) {
		return hasSmallerSecondaryWeight(other);
	} else
		return false;
}

bool SxeRecord::operator>(const SxeRecord &other) const {
	return !(*this == other || *this < other);
}
