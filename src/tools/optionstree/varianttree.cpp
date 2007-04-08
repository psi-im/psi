/*
 * varianttree.cpp - Tree structure for storing QVariants and comments
 * Copyright (C) 2006  Kevin Smith
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

#include <QDomElement>
#include <QDomDocument>

#include "varianttree.h"

/**
 * Default Constructor
 */
VariantTree::VariantTree(QObject *parent)
	: QObject(parent)
{
	
}

/**
 * Default Destructor

 */
VariantTree::~VariantTree()
{
	
}

/**
 * TODO
 */
void VariantTree::setValue(QString node, QVariant value)
{
	if (node.contains("."))
	{
		//not this tier
		QString key=node.left(node.indexOf("."));
		QString subnode=node.remove(0,node.indexOf(".")+1);
		if (!trees_.contains(key))
		{
			if (values_.contains(key))
			{
				qWarning(qPrintable(QString("Error: Trying to add option node %1 but it already exists as a value").arg(key)));
				return;
			}
			//create a new tier
			trees_[key]=new VariantTree(this);
		} 
		//pass it down a level
		trees_[key]->setValue(subnode,value);
	} else {
		//this tier
		if (trees_.contains(node))
		{
			qWarning(qPrintable(QString("Error: Trying to add option value %1 but it already exists as a node").arg(node)));
			return;
		}
		values_[node]=value;
	}
}

/**
 * TODO
 */
QVariant VariantTree::getValue(QString node)
{
	if (node.contains("."))
	{
		//not this tier
		QString key=node.left(node.indexOf("."));
		QString subnode=node.remove(0,node.indexOf(".")+1);
		if (trees_.contains(key))
		{
			return trees_[key]->getValue(subnode);
		}
	} else {
		//this tier
		if (values_.contains(node))
			return values_[node];
	}
	return missingValue;
}

/**
 * TODO
 */
void VariantTree::setComment(QString node, QString comment)
{
	if (node.contains("."))
	{
		//not this tier
		QString key=node.left(node.indexOf("."));
		QString subnode=node.remove(0,node.indexOf(".")+1);
		if (!trees_.contains(key))
		{
			if (values_.contains(key))
			{
				qWarning(qPrintable(QString("Error: Trying to add option node %1 but it already exists as a value").arg(key)));
				return;
			}
			//create a new tier
			trees_[key]=new VariantTree(this);
		} 
		//pass it down a level
		trees_[key]->setComment(subnode,comment);
	} else {
		//this tier
		comments_[node]=comment;
	}
}

/**
 * Returns the comment associated with a node. 
 */
QString VariantTree::getComment(QString node)
{
	if (node.contains("."))
	{
		//not this tier
		QString key=node.left(node.indexOf("."));
		QString subnode=node.remove(0,node.indexOf(".")+1);
		if (trees_.contains(key))
		{
			return trees_[key]->getComment(subnode);
		}
	} else {
		//this tier
		if (comments_.contains(node))
			return comments_[node];
	}
	return missingComment;
}

/**
 * Find all the children of the provided node (if no node is provided),
 * all children.
 *
 * \param direct only return direct children
 * \param internal_nodes include internal (non-final) nodes
 */
QStringList VariantTree::nodeChildren(const QString& node, bool direct, bool internal_nodes) const
{
	QStringList children;
	QString key = node;
	if (!node.isEmpty()) {
		// Go down further
		QString subnode;
		if (node.contains('.')) {
			key = node.left(node.indexOf("."));
			subnode = node.right(node.length() - node.indexOf(".") - 1);
		}
		if (trees_.contains(key)) {
			children = trees_[key]->nodeChildren(subnode,direct,internal_nodes);
		}
	}
	else {
		// Current tree
		foreach (QString subnode, trees_.keys()) {
			if (internal_nodes)
				children << subnode;
				
			if (!direct) 
				children += nodeChildren(subnode,direct,internal_nodes);
		}
		
		foreach (QString child, values_.keys()) {
			children << child;	
		}
	}
	
	if (key.isEmpty()) {
		return children;
	}
	else {
		QStringList long_children;
		foreach (QString child, children) {
			QString long_child = QString("%1.%2").arg(key).arg(child);
			long_children << long_child;
		}
		return long_children;
	}
}


/**
 *
 */
void VariantTree::toXml(QDomDocument &doc, QDomElement& ele) const
{
	// Subtrees
	foreach (QString node, trees_.keys()) {
		QDomElement nodeEle = doc.createElement(node);
		trees_[node]->toXml(doc, nodeEle);
		if (comments_.contains(node))
			nodeEle.setAttribute("comment",comments_[node]);
		ele.appendChild(nodeEle);
	}
	
	// Values
	foreach (QString child, values_.keys()) {
		QVariant var = values_[child];
		QDomElement valEle = doc.createElement(child);
		variantToElement(var,valEle);
		ele.appendChild(valEle);
		if (comments_.contains(child))
			valEle.setAttribute("comment",comments_[child]);
	}
} 

void VariantTree::fromXml(const QDomElement &ele)
{
	QDomElement child = ele.firstChildElement();
	while (!child.isNull()) {
		QString name = child.nodeName();
		if (!child.hasAttribute("type")) {
			// Subnode
			if ( !trees_.contains(name) )
				trees_[name] = new VariantTree(this);
			trees_[name]->fromXml(child);
		} 
		else {
			// Value
			values_[name] = elementToVariant(child);
		}

		// Comments
		if (child.hasAttribute("comment")) {
			QString comment=child.attribute("comment");
			comments_[name]=comment;
		}
		child=child.nextSiblingElement();
	}
}

/**
 * Extracts a variant from an element. 
 * The attribute of the element is used to determine the type.
 * The tagname of the element is ignored.
 */
QVariant VariantTree::elementToVariant(const QDomElement& e)
{
	QVariant value;
	QString type = e.attribute("type");
	if (type=="QString" || type=="bool" || type=="int") {
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			if ( node.isText() )
				value=node.toText().data();
		}
	
		if (!value.isValid())
			value = QString("");

		if (type=="QString")
			value.convert(QVariant::String);
		else if (type=="bool")
			value.convert(QVariant::Bool);
		else if (type=="int")
			value.convert(QVariant::Int);
		else if (type == "QKeySequence")
			value.convert(QVariant::KeySequence);
	}
	else if (type == "QVariantList") {
		QVariantList list;
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement e = node.toElement();
			if (!e.isNull() && e.tagName() == "item") {
				QVariant v = elementToVariant(e);
				if (v.isValid())
					list.append(v);
			}
		}
		value = list;
	}
	return value;
}

/**
 * Modifies the element e to represent the variant var.
 * This method adds an attribute 'type' and contents to the element.
 */
void VariantTree::variantToElement(const QVariant& var, QDomElement& e)
{
	QString type = var.typeName();
	if (type == "QVariantList") {
		foreach(QVariant v, var.toList()) {
			QDomElement item_element = e.ownerDocument().createElement("item");
			variantToElement(v,item_element);
			e.appendChild(item_element);
		}
	}
	else {
		QDomText text = e.ownerDocument().createTextNode(var.toString());
		e.appendChild(text);
	}
	e.setAttribute("type",type);
}

const QVariant VariantTree::missingValue=QVariant(QVariant::Invalid);
const QString VariantTree::missingComment=QString("There is no comment for this node");
