/*
 * sxenewedit.cpp - An single SXE edit that creates a new node
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
 
#include "sxenewedit.h"
#include "sxesession.h"

//----------------------------------------------------------------------------
// SxeNewEdit
//----------------------------------------------------------------------------

SxeNewEdit::SxeNewEdit(const QString rid, const QDomNode &node, const QString parent, double primaryWeight, bool remote) : SxeEdit(rid, remote) {
	parent_ = parent;
	primaryWeight_ = primaryWeight;

	switch(node.nodeType()) {
		case QDomNode::ElementNode:
			type_ = "element";
			identifier_ = node.nodeName();
			ns_ = node.namespaceURI();
			break;

		case QDomNode::AttributeNode:
			type_ = "attr";
			identifier_ = node.nodeName();
			data_ = node.nodeValue();
			ns_ = node.namespaceURI();
			break;

		case QDomNode::TextNode:
			type_ = "text";
			data_ = node.toText().data();
			break;

		case QDomNode::ProcessingInstructionNode:
			type_ = "processinginstruction";
			identifier_ = node.toProcessingInstruction().target();
			data_ = node.toProcessingInstruction().data();
			break;

		case QDomNode::CommentNode:
			type_ = "comment";
			data_ = node.toComment().data();
			break;

		// case QDomNode::DocumentTypeNode:
		//	 type_ = "documenttype";
		//
		//	 break;

		default:
			qDebug() << QString("unknown QDomNode::NodeType encountered in SxeNewEdit::SxeNewEdit(). nodeType: %1.").arg(node.nodeType()).toAscii();
			break;
	}
}

SxeNewEdit::SxeNewEdit(const QDomElement &sxeElement, bool remote) : SxeEdit(sxeElement.attribute("rid"), remote) {
	type_ = sxeElement.attribute("type");
	parent_ = sxeElement.attribute("parent");
	primaryWeight_ = sxeElement.attribute("primary-weight").toDouble();

	if(type_ == "processinginstruction") {
		identifier_ = sxeElement.attribute("pitarget");
		data_ = sxeElement.attribute("pidata");
	} else {
		identifier_ = sxeElement.attribute("name");
		ns_ = sxeElement.attribute("ns");
		data_ = sxeElement.attribute("chdata");
	}
}

SxeEdit::EditType SxeNewEdit::type() const {
	return SxeEdit::New;
};


QDomElement SxeNewEdit::xml(QDomDocument &doc) const {
	QDomElement edit = doc.createElementNS(SXENS, "new");

	edit.setAttribute("rid", rid_);
	edit.setAttribute("type", type_);
	edit.setAttribute("parent", parent_);
	edit.setAttribute("primary-weight", primaryWeight_);

	if(type_ == "element" || type_ == "attr") {
		edit.setAttribute("name", identifier_);
		if(!ns_.isEmpty())
			edit.setAttribute("ns", ns_);
	} if(type_ == "text" || type_ == "attr" || type_ == "comment")
		edit.setAttribute("chdata", data_);
	if(type_ == "processinginstruction") {
		edit.setAttribute("pitarget", identifier_);
		edit.setAttribute("pidata", data_);
	}

	return edit;
}

QDomNode::NodeType SxeNewEdit::nodeType() const {
	if(type_ == "element")
		return QDomNode::ElementNode;
	else if(type_ == "attr")
		return QDomNode::AttributeNode;
	else if(type_ == "text")
		return QDomNode::TextNode;
	else if(type_ == "comment")
		return QDomNode::CommentNode;
	else if(type_ == "processinginstruction")
		return QDomNode::ProcessingInstructionNode;
	else
		return QDomNode::BaseNode;
}

QString SxeNewEdit::parent() const {
	return parent_;
}

double SxeNewEdit::primaryWeight() const {
	return primaryWeight_;
}

QString SxeNewEdit::name() const {
	return identifier_;
}

QString SxeNewEdit::nameSpace() const {
	return ns_;
}

QString SxeNewEdit::chdata() const {
	return data_;
}

QString SxeNewEdit::processingInstructionTarget() const {
	return identifier_;
}

QString SxeNewEdit::processingInstructionData() const {
	return data_;
}
