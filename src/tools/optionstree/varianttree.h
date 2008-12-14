/*
 * varianttree.h - Tree structure for QVariants and Comments
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

#ifndef _VARIANTTREE_H_
#define _VARIANTTREE_H_

#include <QObject>
#include <QVariant>
#include <QHash>

class QDomDocument;
class QDomElement;
class QDomDocumentFragment;


/**
 * \class VariantTree
 * \brief A recursive structure for storing QVariants in trees, with comments
 * 
 * All the methods in this class are recursive, based on a hierachy delimited 
 * with dots in the node name. As such, the nodes "Paris" and "Benvolio" are 
 * top level elements (members of this layer), while "Capulet.Juliet" is a 
 * member of a deeper node ("Capulet") and "Verona.Montague.Romeo" represents 
 * the node "Romeo" which is a member of "Montague", which is again a member
 * of "Verona" (which is a member of this layer).
 * 
 * As such, for each function, if the supplied node contains a dot ("."), 
 * the node name is split at the first (if there are several) dot, with the 
 * remainder passed to the same method of the member of this node with the 
 * name given by the primary component. For the set methods, multiple layers
 * in the hierachy may be created implicitly if the node is not found.
 */
class VariantTree : public QObject
{
	Q_OBJECT
public:
	VariantTree(QObject *parent = 0);
	~VariantTree();

	void setValue(QString node, QVariant value);
	QVariant getValue(const QString& node) const;
	
	bool isInternalNode(QString node) const;

	void setComment(QString node, QString comment);
	QString getComment(QString node) const;

	bool remove(const QString &node, bool internal_nodes = false);
	
	QStringList nodeChildren(const QString& node = "", bool direct = false, bool internal_nodes = false) const; 

	void toXml(QDomDocument &doc, QDomElement& ele) const;
	void fromXml(const QDomElement &ele);

	static bool isValidNodeName(const QString &name);
	
	static const QVariant missingValue;
	static const QString missingComment;

protected:
	static QVariant elementToVariant(const QDomElement&);
	static void variantToElement(const QVariant&, QDomElement&);
	
	static bool getKeyRest(const QString& node, QString &key, QString &rest);

private:
	QHash<QString, VariantTree*> trees_;
	QHash<QString, QVariant> values_;
	QHash<QString, QString> comments_;
	QHash<QString, QDomDocumentFragment> unknowns_;		// unknown types preservation
	QHash<QString, QString> unknowns2_;		// unknown types preservation
	
	// needed to have a document for the fragments.
	static QDomDocument *unknownsDoc;
	friend class OptionsTreeReader;
	friend class OptionsTreeWriter;
};

#endif /* _VARIANTTREE_H_ */
