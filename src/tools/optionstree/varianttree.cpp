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

#include "varianttree.h"

#include <QRect>
#include <QSize>
#include <QDomElement>
#include <QDomDocument>
#include <QDomDocumentFragment>
#include <QKeySequence>
#include <QStringList>
#include <QtDebug>
#include "xmpp/base64/base64.h"

using namespace XMPP;

// FIXME: Helpers from xmpp_xmlcommon.h would be very appropriate for
// void VariantTree::variantToElement(const QVariant& var, QDomElement& e)
// void VariantTree::elementToVariant(const QVariant& var, QDomElement& e)

QDomDocument *VariantTree::unknownsDoc=0;

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
	foreach(VariantTree* vt, trees_.values()) {
		delete vt;
	}
}


/**
 * Split a @a node into local key and rest
 * @param node 
 * @param key part of the @a node before first dot
 * @param rest part of the @a node after first dot
 * @return 
 */
bool VariantTree::getKeyRest(const QString& node, QString &key, QString &rest)
{
	int idx = node.indexOf(QChar('.'));
	if (idx != -1) {
		key=node.left(idx);
		rest=node.mid(idx+1);
		return true;
	}
	return false;
}


bool VariantTree::isValidNodeName(const QString &name)
{
/* XML backend:
[4]   	NameChar	   ::=   	 Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender
[5]   	Name	   ::=   	(Letter | '_' | ':') (NameChar)*
but we don't want to have namespaces in the node names....
	
	but for now just allow ascii subset of this:
	*/	
	if (name.isEmpty()) return false;
	int len = name.length();
	QString other(".-_");
	QChar ch = name[0];
	if (!((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch=='_'))) return false;
	for (int i=1; i < len; i++) {
		ch = name[i];
		if (!((ch>='A' && ch<='Z')
			|| (ch>='a' && ch<='z')
			|| (other.contains(ch))
			|| (ch >= '0' && ch <= '9'))) return false;
	}
	return true;
}

/**
 * Set @a node to value @a value
 */
void VariantTree::setValue(QString node, QVariant value)
{
	QString key, subnode;
	if (getKeyRest(node, key, subnode)) {
		//not this tier
		Q_ASSERT(isValidNodeName(key));
		if (!trees_.contains(key))
		{
			if (values_.contains(key))
			{
				qWarning() << QString("Error: Trying to add option node %1 but it already exists as a value").arg(key);
				return;
			}
			//create a new tier
			trees_[key]=new VariantTree(this);
		} 
		//pass it down a level
		trees_[key]->setValue(subnode,value);
	} else {
		//this tier
		Q_ASSERT(isValidNodeName(node));
		if (trees_.contains(node))
		{
			qWarning() << QString("Error: Trying to add option value %1 but it already exists as a subtree").arg(node);
			return;
		}
		values_[node]=value;
	}
}

/**
 * Get value at @a node
 * @return the value of @a node if @a node exists, otherwise VariantTree::missingValue
 */
QVariant VariantTree::getValue(const QString& node) const
{
	QString key,subnode;
	if (getKeyRest(node, key, subnode)) {
		//not this tier
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


bool VariantTree::remove(const QString &node, bool internal_nodes)
{
	QString key,subnode;
	if (getKeyRest(node, key, subnode)) {
		//not this tier
		if (trees_.contains(key)) {
			return trees_[key]->remove(subnode, internal_nodes);
		}
	} else {
		//this tier
		if (values_.contains(node)) {
			values_.remove(node);
			return true;
		} else if (internal_nodes && trees_.contains(node)) {
			trees_.remove(node);
			return true;
		}
	}
	return false;
}

/**
 * @return true iff the node @a node is an internal node (i.e. has a child tree).
 */
bool VariantTree::isInternalNode(QString node) const
{
	QString key,subnode;
	if (getKeyRest(node, key, subnode)) {
		//not this tier
		if (trees_.contains(key)) {
			return trees_[key]->isInternalNode(subnode);
		}
		qWarning("isInternalNode called on non existant node: %s", qPrintable(node));
		return false;
	} else {
		return trees_.contains(node);
	}
}



/**
 * \brief Sets the comment of the specified node.
 * \param name "Path" to the node
 * \param comment the comment to store
 */
void VariantTree::setComment(QString node, QString comment)
{
	if (node.contains(QChar('.')))
	{
		//not this tier
		QString key=node.left(node.indexOf(QChar('.')));
		QString subnode=node.remove(0,node.indexOf(QChar('.'))+1);
		Q_ASSERT(isValidNodeName(key));
		if (!trees_.contains(key))
		{
			if (values_.contains(key))
			{
				qWarning() << QString("Error: Trying to add option node %1 but it already exists as a value").arg(key);
				return;
			}
			//create a new tier
			trees_[key]=new VariantTree(this);
		} 
		//pass it down a level
		trees_[key]->setComment(subnode,comment);
	} else {
		//this tier
		Q_ASSERT(isValidNodeName(node));
		comments_[node]=comment;
	}
}

/**
 * Returns the comment associated with a node.
 * (or a null QString if the node has no comment)
 */
QString VariantTree::getComment(QString node) const
{
	if (node.contains(QChar('.')))
	{
		//not this tier
		QString key=node.left(node.indexOf(QChar('.')));
		QString subnode=node.remove(0,node.indexOf(QChar('.'))+1);
		if (trees_.contains(key))
		{
			return trees_[key]->getComment(subnode);
		}
	} else {
		//this tier
		if (comments_.contains(node))
			return comments_[node];
	}
	return QString();
}

/**
 * Find all the children of the provided node \a node or, if no node is provided,
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
			key = node.left(node.indexOf(QChar('.')));
			subnode = node.right(node.length() - node.indexOf(QChar('.')) - 1);
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
		Q_ASSERT(!node.isEmpty());
		QDomElement nodeEle = doc.createElement(node);
		trees_[node]->toXml(doc, nodeEle);
		if (comments_.contains(node))
			nodeEle.setAttribute("comment",comments_[node]);
		ele.appendChild(nodeEle);
	}
	
	// Values
	foreach (QString child, values_.keys()) {
		Q_ASSERT(!child.isEmpty());
		QVariant var = values_[child];
		QDomElement valEle = doc.createElement(child);
		variantToElement(var,valEle);
		ele.appendChild(valEle);
		if (comments_.contains(child))
			valEle.setAttribute("comment",comments_[child]);
	}
	
	// unknown types passthrough
	foreach (QDomDocumentFragment df, unknowns_) {
		ele.appendChild(doc.importNode(df, true));
	}
} 

/**
 * 
 * @param ele 
 */
void VariantTree::fromXml(const QDomElement &ele)
{
	QDomElement child = ele.firstChildElement();
	while (!child.isNull()) {
		bool isunknown=false;
		QString name = child.nodeName();
		Q_ASSERT(!name.isEmpty());
		if (!child.hasAttribute("type")) {
			// Subnode
			if ( !trees_.contains(name) )
				trees_[name] = new VariantTree(this);
			trees_[name]->fromXml(child);
		} 
		else {
			// Value
			QVariant val;
			val = elementToVariant(child);
			if (val.isValid()) {
				values_[name] = val;
			} else {
				isunknown = true;
				if (!unknownsDoc) unknownsDoc = new QDomDocument();
				QDomDocumentFragment frag(unknownsDoc->createDocumentFragment());
				frag.appendChild(unknownsDoc->importNode(child, true));
				unknowns_[name] = frag;
			}
		}

		// Comments
		if (!isunknown && child.hasAttribute("comment")) {
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
	if (type == "QStringList") {
		QStringList list;
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement e = node.toElement();
			if (!e.isNull() && e.tagName() == "item") {
				list += e.text();
			}
		}
		value = list;
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
	else if (type == "QSize") {
		int width = 0, height = 0;
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement e = node.toElement();
			if (!e.isNull()) {
				if (e.tagName() == "width") {
					width = e.text().toInt();
				}
				else if (e.tagName() == "height") {
					height = e.text().toInt();
				}
			}
		}
		value = QVariant(QSize(width,height));
	}
	else if (type == "QRect") {
		int x = 0, y = 0, width = 0, height = 0;
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			QDomElement e = node.toElement();
			if (!e.isNull()) {
				if (e.tagName() == "width") {
					width = e.text().toInt();
				}
				else if (e.tagName() == "height") {
					height = e.text().toInt();
				}
				else if (e.tagName() == "x") {
					x = e.text().toInt();
				}
				else if (e.tagName() == "y") {
					y = e.text().toInt();
				}
			}
		}
		value = QVariant(QRect(x,y,width,height));
	}
	else if (type == "QByteArray") {
		value = QByteArray();
		for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
			if (node.isText()) {
				value = Base64::decode(node.toText().data());
				break;
			}
		}
	}
	else { // Standard values
		QVariant::Type varianttype;
		bool known = true;
		
		if (type=="QString") {
			varianttype = QVariant::String;
		} else if (type=="bool") {
			varianttype = QVariant::Bool;
		} else if (type=="int") {
			varianttype = QVariant::Int;
		} else if (type == "QKeySequence") {
			varianttype = QVariant::KeySequence;
		} else if (type == "QColor") {
			varianttype = QVariant::Color;
		} else {
			known = false;
		}
		
		if (known) {
			for (QDomNode node = e.firstChild(); !node.isNull(); node = node.nextSibling()) {
				if ( node.isText() )
					value=node.toText().data();
			}
		
			if (!value.isValid())
				value = QString("");
	
			value.convert(varianttype);
		} else {
			value = QVariant();
		}
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
	else if (type == "QStringList") {
		foreach(QString s, var.toStringList()) {
			QDomElement item_element = e.ownerDocument().createElement("item");
			QDomText text = e.ownerDocument().createTextNode(s);
			item_element.appendChild(text);
			e.appendChild(item_element);
		}
	}
	else if (type == "QSize") {
		QSize size = var.toSize();
		QDomElement width_element = e.ownerDocument().createElement("width");
		width_element.appendChild(e.ownerDocument().createTextNode(QString::number(size.width())));
		e.appendChild(width_element);
		QDomElement height_element = e.ownerDocument().createElement("height");
		height_element.appendChild(e.ownerDocument().createTextNode(QString::number(size.height())));
		e.appendChild(height_element);
	}
	else if (type == "QRect") {
		QRect rect = var.toRect();
		QDomElement x_element = e.ownerDocument().createElement("x");
		x_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.x())));
		e.appendChild(x_element);
		QDomElement y_element = e.ownerDocument().createElement("y");
		y_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.y())));
		e.appendChild(y_element);
		QDomElement width_element = e.ownerDocument().createElement("width");
		width_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.width())));
		e.appendChild(width_element);
		QDomElement height_element = e.ownerDocument().createElement("height");
		height_element.appendChild(e.ownerDocument().createTextNode(QString::number(rect.height())));
		e.appendChild(height_element);
	}
	else if (type == "QByteArray") {
		QDomText text = e.ownerDocument().createTextNode(Base64::encode(var.toByteArray()));
		e.appendChild(text);
	}
	else if (type == "QKeySequence") {
		QKeySequence k = var.value<QKeySequence>();
		QDomText text = e.ownerDocument().createTextNode(k.toString());
		e.appendChild(text);
	}
	else {
		QDomText text = e.ownerDocument().createTextNode(var.toString());
		e.appendChild(text);
	}
	e.setAttribute("type",type);
}

const QVariant VariantTree::missingValue=QVariant(QVariant::Invalid);
